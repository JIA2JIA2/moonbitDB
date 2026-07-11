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
struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3592__l711__;

struct _M0TWEOUsiE;

struct _M0TPB9ArrayViewGUsfEE;

struct _M0TPB8MutLocalGORPC16string10StringViewE;

struct _M0TP19moonbitDB8Database;

struct _M0TWEOUssE;

struct _M0TWEOUsRP19moonbitDB10RedisValueE;

struct _M0TPB4IterGUsiEE;

struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE;

struct _M0TPB3MapGsfE;

struct _M0TUsiE;

struct _M0TUsbE;

struct _M0TPB13StringBuilder;

struct _M0TPB9ArrayViewGUsiEE;

struct _M0TPB17FloatingDecimal64;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE;

struct _M0TPB5EntryGssE;

struct _M0R44StringView_3a_3asplit_2eanon__u2903__l1148__;

struct _M0TUssE;

struct _M0TPB3MapGsRP19moonbitDB10RedisValueE;

struct _M0BTPB6Logger;

struct _M0DTP19moonbitDB10RedisValue6String;

struct _M0TPB4IterGcE;

struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE;

struct _M0DTP19moonbitDB10RedisValue4Hash;

struct _M0TPB6Logger;

struct _M0TWcERPC16string10StringView;

struct _M0TP19moonbitDB5Deque;

struct _M0TUsfE;

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3612__l711__;

struct _M0TPB8MutLocalGORPB5EntryGsfEE;

struct _M0TPB5EntryGsbE;

struct _M0TPB8MutLocalGORPB5EntryGssEE;

struct _M0DTPC16option6OptionGfE4Some;

struct _M0TPB19MulShiftAll64Result;

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3582__l711__;

struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2892__l391__;

struct _M0DTP19moonbitDB10RedisValue4ZSet;

struct _M0TPB4IterGRPC16string10StringViewE;

struct _M0TPB5ArrayGOsE;

struct _M0TWEOUsbE;

struct _M0DTP19moonbitDB10RedisValue4List;

struct _M0TPB5EntryGsfE;

struct _M0TPB8MutLocalGiE;

struct _M0TWERPC16option6OptionGRPC16string10StringViewE;

struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3622__l711__;

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE;

struct _M0TPB5ArrayGUsfEE;

struct _M0TPB3MapGssE;

struct _M0TUsRP19moonbitDB10RedisValueE;

struct _M0DTP19moonbitDB10RedisValue3Set;

struct _M0TPB4Show;

struct _M0TPB8MutLocalGfE;

struct _M0TWEOc;

struct _M0R42StringView_3a_3aiter_2eanon__u2763__l219__;

struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3602__l711__;

struct _M0TPB4IterGUsfEE;

struct _M0TWEOUsfE;

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

struct _M0TUiiE;

struct _M0TPB4IterGUsbEE;

struct _M0TPB4IterGUssEE;

struct _M0TPB5EntryGsiE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3592__l711__ {
  struct _M0TUsbE*(* code)(struct _M0TWEOUsbE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* $1;
  
};

struct _M0TWEOUsiE {
  struct _M0TUsiE*(* code)(struct _M0TWEOUsiE*);
  
};

struct _M0TPB9ArrayViewGUsfEE {
  struct _M0TUsfE** $0;
  int32_t $1;
  int32_t $2;
  
};

struct _M0TPB8MutLocalGORPC16string10StringViewE {
  void* $0;
  
};

struct _M0TP19moonbitDB8Database {
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* $0;
  struct _M0TPB3MapGsiE* $1;
  int32_t $2;
  
};

struct _M0TWEOUssE {
  struct _M0TUssE*(* code)(struct _M0TWEOUssE*);
  
};

struct _M0TWEOUsRP19moonbitDB10RedisValueE {
  struct _M0TUsRP19moonbitDB10RedisValueE*(* code)(
    struct _M0TWEOUsRP19moonbitDB10RedisValueE*
  );
  
};

struct _M0TPB4IterGUsiEE {
  struct _M0TWEOUsiE* $0;
  int64_t $1;
  
};

struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE {
  struct _M0TUsRP19moonbitDB10RedisValueE** $0;
  int32_t $1;
  int32_t $2;
  
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

struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE {
  int32_t $0;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* $1;
  int32_t $2;
  int32_t $3;
  moonbit_string_t $4;
  void* $5;
  
};

struct _M0TPB5EntryGssE {
  int32_t $0;
  struct _M0TPB5EntryGssE* $1;
  int32_t $2;
  int32_t $3;
  moonbit_string_t $4;
  moonbit_string_t $5;
  
};

struct _M0R44StringView_3a_3asplit_2eanon__u2903__l1148__ {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  struct _M0TPB8MutLocalGORPC16string10StringViewE* $0;
  struct _M0TPC16string10StringView $1;
  int32_t $2;
  
};

struct _M0TUssE {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0TPB3MapGsRP19moonbitDB10RedisValueE {
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** $0;
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* $5;
  int32_t $6;
  
};

struct _M0BTPB6Logger {
  int32_t(* $method_0)(void*, moonbit_string_t);
  int32_t(* $method_1)(void*, moonbit_string_t, int32_t, int32_t);
  int32_t(* $method_2)(void*, struct _M0TPC16string10StringView);
  int32_t(* $method_3)(void*, int32_t);
  int32_t(* $method_4)(void*, struct _M0TPB4Show);
  int32_t(* $method_5)(void*, struct _M0TPB4Show);
  
};

struct _M0DTP19moonbitDB10RedisValue6String {
  moonbit_string_t $0;
  
};

struct _M0TPB4IterGcE {
  struct _M0TWEOc* $0;
  int64_t $1;
  
};

struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE {
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* $0;
  
};

struct _M0DTP19moonbitDB10RedisValue4Hash {
  struct _M0TPB3MapGssE* $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TWcERPC16string10StringView {
  struct _M0TPC16string10StringView(* code)(
    struct _M0TWcERPC16string10StringView*,
    int32_t
  );
  
};

struct _M0TP19moonbitDB5Deque {
  struct _M0TPB5ArrayGsE* $0;
  struct _M0TPB5ArrayGsE* $1;
  
};

struct _M0TUsfE {
  moonbit_string_t $0;
  float $1;
  
};

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3612__l711__ {
  struct _M0TUsfE*(* code)(struct _M0TWEOUsfE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* $1;
  
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

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3582__l711__ {
  struct _M0TUssE*(* code)(struct _M0TWEOUssE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* $1;
  
};

struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2892__l391__ {
  void*(* code)(struct _M0TWERPC16option6OptionGRPC16string10StringViewE*);
  struct _M0TWcERPC16string10StringView* $0;
  struct _M0TPB4IterGcE* $1;
  
};

struct _M0DTP19moonbitDB10RedisValue4ZSet {
  struct _M0TPB3MapGsfE* $0;
  
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

struct _M0DTP19moonbitDB10RedisValue4List {
  struct _M0TP19moonbitDB5Deque* $0;
  
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

struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3622__l711__ {
  struct _M0TUsiE*(* code)(struct _M0TWEOUsiE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsiEE* $1;
  
};

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE {
  struct _M0TWEOUsRP19moonbitDB10RedisValueE* $0;
  int64_t $1;
  
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

struct _M0TUsRP19moonbitDB10RedisValueE {
  moonbit_string_t $0;
  void* $1;
  
};

struct _M0DTP19moonbitDB10RedisValue3Set {
  struct _M0TPB3MapGsbE* $0;
  
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

struct _M0R42StringView_3a_3aiter_2eanon__u2763__l219__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TPB8MutLocalGiE* $0;
  int32_t $1;
  struct _M0TPC16string10StringView $2;
  
};

struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3602__l711__ {
  struct _M0TUsRP19moonbitDB10RedisValueE*(* code)(
    struct _M0TWEOUsRP19moonbitDB10RedisValueE*
  );
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE* $1;
  
};

struct _M0TPB4IterGUsfEE {
  struct _M0TWEOUsfE* $0;
  int64_t $1;
  
};

struct _M0TWEOUsfE {
  struct _M0TUsfE*(* code)(struct _M0TWEOUsfE*);
  
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

struct _M0TPB5ArrayGsE* _M0FP39moonbitDB8examples9cli__repl16execute__command(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0FP39moonbitDB8examples9cli__repl14split__command(
  moonbit_string_t
);

void* _M0FP39moonbitDB8examples9cli__repl12parse__float(moonbit_string_t);

int64_t _M0FP39moonbitDB8examples9cli__repl10parse__int(moonbit_string_t);

moonbit_string_t _M0FP19moonbitDB16show__opt__float(void*);

moonbit_string_t _M0FP19moonbitDB14show__opt__int(int64_t);

moonbit_string_t _M0FP19moonbitDB9show__opt(moonbit_string_t);

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database7command();

struct _M0TUiiE* _M0MP19moonbitDB8Database4time(
  struct _M0TP19moonbitDB8Database*
);

moonbit_string_t _M0MP19moonbitDB8Database4info(
  struct _M0TP19moonbitDB8Database*
);

moonbit_string_t _M0MP19moonbitDB8Database4ping();

moonbit_string_t _M0MP19moonbitDB8Database9randomkey(
  struct _M0TP19moonbitDB8Database*
);

int32_t _M0MP19moonbitDB8Database7flushdb(struct _M0TP19moonbitDB8Database*);

int32_t _M0MP19moonbitDB8Database6dbsize(struct _M0TP19moonbitDB8Database*);

float _M0MP19moonbitDB8Database7zincrby(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  float,
  moonbit_string_t
);

int64_t _M0MP19moonbitDB8Database5zrank(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database9zrevrange(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  int32_t,
  int32_t
);

struct _M0TPB5ArrayGUsfEE* _M0MP19moonbitDB8Database17get__sorted__zset(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

void* _M0MP19moonbitDB8Database6zscore(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database5zcard(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database6zrange(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  int32_t,
  int32_t
);

struct _M0TPB5ArrayGUsfEE* _M0FP19moonbitDB15sort__by__score(
  struct _M0TPB5ArrayGUsfEE*
);

struct _M0TPB5ArrayGUsfEE* _M0FP19moonbitDB11merge__sort(
  struct _M0TPB5ArrayGUsfEE*
);

struct _M0TPB5ArrayGUsfEE* _M0FP19moonbitDB5merge(
  struct _M0TPB5ArrayGUsfEE*,
  struct _M0TPB5ArrayGUsfEE*
);

int32_t _M0MP19moonbitDB8Database4zadd(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  float,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database9sismember(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database5scard(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database4srem(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database8smembers(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database4sadd(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database6lrange(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  int32_t,
  int32_t
);

int32_t _M0MP19moonbitDB8Database4llen(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

moonbit_string_t _M0MP19moonbitDB8Database4rpop(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

moonbit_string_t _M0MP19moonbitDB8Database4lpop(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database5rpush(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database5lpush(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database4hlen(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

struct _M0TPB3MapGssE* _M0MP19moonbitDB8Database7hgetall(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database4hdel(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0MP19moonbitDB8Database4hget(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database4hset(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  moonbit_string_t,
  moonbit_string_t
);

int64_t _M0MP19moonbitDB8Database4decr(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

int64_t _M0MP19moonbitDB8Database4incr(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

moonbit_string_t _M0FP19moonbitDB15int__to__string(int32_t);

int64_t _M0FP19moonbitDB10parse__int(moonbit_string_t);

int64_t _M0FP19moonbitDB17uint16__to__digit(int32_t);

int32_t _M0MP19moonbitDB8Database6strlen(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database6append(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0MP19moonbitDB8Database8type__of(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database4keys(
  struct _M0TP19moonbitDB8Database*
);

int32_t _M0MP19moonbitDB8Database6exists(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database3del(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

moonbit_string_t _M0MP19moonbitDB8Database3get(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database4mdel(
  struct _M0TP19moonbitDB8Database*,
  struct _M0TPB5ArrayGsE*
);

struct _M0TPB5ArrayGOsE* _M0MP19moonbitDB8Database4mget(
  struct _M0TP19moonbitDB8Database*,
  struct _M0TPB5ArrayGsE*
);

int32_t _M0MP19moonbitDB8Database4mset(
  struct _M0TP19moonbitDB8Database*,
  struct _M0TPB5ArrayGsE*,
  struct _M0TPB5ArrayGsE*
);

int32_t _M0MP19moonbitDB8Database7persist(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database3ttl(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database6expire(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MP19moonbitDB8Database3set(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database14check__expired(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database13advance__time(
  struct _M0TP19moonbitDB8Database*,
  int32_t
);

struct _M0TP19moonbitDB8Database* _M0MP19moonbitDB8Database3new();

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB5Deque9to__array(
  struct _M0TP19moonbitDB5Deque*
);

int32_t _M0MP19moonbitDB5Deque6length(struct _M0TP19moonbitDB5Deque*);

moonbit_string_t _M0MP19moonbitDB5Deque9pop__back(
  struct _M0TP19moonbitDB5Deque*
);

moonbit_string_t _M0MP19moonbitDB5Deque10pop__front(
  struct _M0TP19moonbitDB5Deque*
);

int32_t _M0MP19moonbitDB5Deque10push__back(
  struct _M0TP19moonbitDB5Deque*,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB5Deque11push__front(
  struct _M0TP19moonbitDB5Deque*,
  moonbit_string_t
);

struct _M0TP19moonbitDB5Deque* _M0MP19moonbitDB5Deque3new();

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

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE*
);

struct _M0TUsfE* _M0MPB5Iter24nextGsfE(struct _M0TPB4IterGUsfEE*);

struct _M0TUsiE* _M0MPB5Iter24nextGsiE(struct _M0TPB4IterGUsiEE*);

struct _M0TPB4IterGUssEE* _M0MPB3Map5iter2GssE(struct _M0TPB3MapGssE*);

struct _M0TPB4IterGUsbEE* _M0MPB3Map5iter2GsbE(struct _M0TPB3MapGsbE*);

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*
);

struct _M0TPB4IterGUsfEE* _M0MPB3Map5iter2GsfE(struct _M0TPB3MapGsfE*);

struct _M0TPB4IterGUsiEE* _M0MPB3Map5iter2GsiE(struct _M0TPB3MapGsiE*);

struct _M0TPB4IterGUssEE* _M0MPB3Map4iterGssE(struct _M0TPB3MapGssE*);

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(struct _M0TPB3MapGsbE*);

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB3Map4iterGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*
);

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(struct _M0TPB3MapGsfE*);

struct _M0TPB4IterGUsiEE* _M0MPB3Map4iterGsiE(struct _M0TPB3MapGsiE*);

struct _M0TUsiE* _M0MPB3Map4iterGsiEC3622l711(struct _M0TWEOUsiE*);

struct _M0TUsfE* _M0MPB3Map4iterGsfEC3612l711(struct _M0TWEOUsfE*);

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB3Map4iterGsRP19moonbitDB10RedisValueEC3602l711(
  struct _M0TWEOUsRP19moonbitDB10RedisValueE*
);

struct _M0TUsbE* _M0MPB3Map4iterGsbEC3592l711(struct _M0TWEOUsbE*);

struct _M0TUssE* _M0MPB3Map4iterGssEC3582l711(struct _M0TWEOUssE*);

int32_t _M0MPB3Map6lengthGssE(struct _M0TPB3MapGssE*);

int32_t _M0MPB3Map6lengthGsbE(struct _M0TPB3MapGsbE*);

int32_t _M0MPB3Map6lengthGsfE(struct _M0TPB3MapGsfE*);

int32_t _M0MPB3Map6removeGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

int32_t _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  moonbit_string_t
);

int32_t _M0MPB3Map6removeGssE(struct _M0TPB3MapGssE*, moonbit_string_t);

int32_t _M0MPB3Map6removeGsbE(struct _M0TPB3MapGsbE*, moonbit_string_t);

int32_t _M0MPB3Map18remove__with__hashGsiE(
  struct _M0TPB3MapGsiE*,
  moonbit_string_t,
  int32_t
);

int32_t _M0MPB3Map18remove__with__hashGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
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

int32_t _M0MPB3Map11shift__backGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  int32_t
);

int32_t _M0MPB3Map11shift__backGssE(struct _M0TPB3MapGssE*, int32_t);

int32_t _M0MPB3Map11shift__backGsbE(struct _M0TPB3MapGsbE*, int32_t);

int32_t _M0MPB3Map13remove__entryGsiE(
  struct _M0TPB3MapGsiE*,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPB3Map13remove__entryGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*
);

int32_t _M0MPB3Map13remove__entryGssE(
  struct _M0TPB3MapGssE*,
  struct _M0TPB5EntryGssE*
);

int32_t _M0MPB3Map13remove__entryGsbE(
  struct _M0TPB3MapGsbE*,
  struct _M0TPB5EntryGsbE*
);

int32_t _M0MPB3Map8containsGsbE(struct _M0TPB3MapGsbE*, moonbit_string_t);

int32_t _M0MPB3Map8containsGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t);

int32_t _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  moonbit_string_t
);

int32_t _M0MPB3Map8containsGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

int32_t _M0MPB3Map8containsGssE(struct _M0TPB3MapGssE*, moonbit_string_t);

void* _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  moonbit_string_t
);

moonbit_string_t _M0MPB3Map3getGssE(struct _M0TPB3MapGssE*, moonbit_string_t);

void* _M0MPB3Map3getGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t);

int64_t _M0MPB3Map3getGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0MPB3Map3MapGsRP19moonbitDB10RedisValueE(
  struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE,
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

int32_t _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
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

int32_t _M0MPB3Map15set__with__hashGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
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

int32_t _M0MPB3Map4growGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*
);

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE*);

int32_t _M0MPB3Map4growGsbE(struct _M0TPB3MapGsbE*);

int32_t _M0MPB3Map4growGsfE(struct _M0TPB3MapGsfE*);

int32_t _M0MPB3Map4growGsiE(struct _M0TPB3MapGsiE*);

int32_t _M0MPB3Map20rehash__place__entryGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*
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

int32_t _M0MPB3Map10push__awayGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  int32_t,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*
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

int32_t _M0MPB3Map10set__entryGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*,
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

int32_t _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  int32_t,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*
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

struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0FPB8new__mapGsRP19moonbitDB10RedisValueE(
  int32_t
);

struct _M0TPB3MapGsiE* _M0FPB8new__mapGsiE(int32_t);

struct _M0TPB3MapGssE* _M0FPB8new__mapGssE(int32_t);

struct _M0TPB3MapGsbE* _M0FPB8new__mapGsbE(int32_t);

struct _M0TPB3MapGsfE* _M0FPB8new__mapGsfE(int32_t);

int32_t _M0MPC13int3Int20next__power__of__two(int32_t);

int32_t _M0FPB21calc__grow__threshold(int32_t);

int32_t _M0MPC16option6Option6unwrapGiE(int64_t);

struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP19moonbitDB10RedisValueEE(
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*
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

moonbit_string_t _M0MPC16string6String9to__upper(moonbit_string_t);

int32_t _M0MPC16string6String9to__upperC2949l1791(struct _M0TWcEb*, int32_t);

int32_t _M0MPC14char4Char20is__ascii__lowercase(int32_t);

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPC16string6String5split(
  moonbit_string_t,
  struct _M0TPC16string10StringView
);

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPC16string10StringView5split(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

void* _M0MPC16string10StringView5splitC2903l1148(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
);

struct _M0TPC16string10StringView _M0MPC16string10StringView5splitC2899l1145(
  struct _M0TWcERPC16string10StringView*,
  int32_t
);

moonbit_string_t _M0IPC14char4CharPB4Show10to__string(int32_t);

moonbit_string_t _M0FPB16char__to__string(int32_t);

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPB4Iter3mapGcRPC16string10StringViewE(
  struct _M0TPB4IterGcE*,
  struct _M0TWcERPC16string10StringView*
);

void* _M0MPB4Iter3mapGcRPC16string10StringViewEC2892l391(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*
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

int32_t _M0MPC16string10StringView4iterC2763l219(struct _M0TWEOc*);

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

struct _M0TPB4IterGUsbEE* _M0MPB4Iter3newGUsbEE(struct _M0TWEOUsbE*, int64_t);

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB4Iter3newGUsRP19moonbitDB10RedisValueEE(
  struct _M0TWEOUsRP19moonbitDB10RedisValueE*,
  int64_t
);

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPB4Iter3newGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE*,
  int64_t
);

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

struct _M0TUssE* _M0MPB4Iter4nextGUssEE(struct _M0TPB4IterGUssEE*);

void* _M0MPB4Iter4nextGRPC16string10StringViewE(
  struct _M0TPB4IterGRPC16string10StringViewE*
);

struct _M0TUsbE* _M0MPB4Iter4nextGUsbEE(struct _M0TPB4IterGUsbEE*);

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB4Iter4nextGUsRP19moonbitDB10RedisValueEE(
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE*
);

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

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t
);

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

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_133 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 110, 111, 
    110, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_129 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_102 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 90, 82, 
    69, 86, 82, 65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
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
} const moonbit_string_literal_174 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 83, 65, 
    68, 68, 32, 116, 97, 103, 115, 32, 116, 101, 99, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_99 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 90, 82, 
    65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
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
} const moonbit_string_literal_141 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 73, 110, 
    102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_140 =
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
} const moonbit_string_literal_121 =
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
} const moonbit_string_literal_123 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_122 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[30]; 
} const moonbit_string_literal_74 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 29, 69, 82, 
    82, 32, 119, 114, 111, 110, 103, 32, 110, 117, 109, 98, 101, 114, 
    32, 111, 102, 32, 97, 114, 103, 117, 109, 101, 110, 116, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_95 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 83, 68, 
    73, 70, 70, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_184 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_87 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 76, 80, 
    85, 83, 72, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_83 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 76, 73, 
    78, 68, 69, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_42 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 90, 65, 
    68, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_175 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 21, 83, 65, 
    68, 68, 32, 116, 97, 103, 115, 32, 112, 114, 111, 103, 114, 97, 109, 
    109, 105, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_161 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 65, 80, 
    80, 69, 78, 68, 32, 110, 97, 109, 101, 32, 39, 32, 83, 109, 105, 
    116, 104, 39, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_101 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 90, 82, 
    69, 86, 82, 65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_126 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 52, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_164 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 84, 84, 
    76, 32, 110, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_187 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 77, 71, 
    69, 84, 32, 97, 32, 98, 32, 99, 32, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_153 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 46, 108, 101, 110, 103, 116, 104, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_116 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 44, 101, 
    120, 112, 105, 114, 101, 115, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_77 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 84, 
    84, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_150 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_72 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 71, 69, 84, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_106 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 90, 80, 
    79, 80, 77, 73, 78, 0
  };

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
} const moonbit_string_literal_139 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 25, 73, 108, 
    108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 116, 69, 
    120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_118 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_96 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 77, 
    79, 86, 69, 0
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
} const moonbit_string_literal_132 =
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
} const moonbit_string_literal_127 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 53, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_104 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 90, 82, 
    69, 77, 82, 65, 78, 71, 69, 66, 89, 82, 65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_80 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 86, 
    65, 76, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_138 =
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
} const moonbit_string_literal_84 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_68 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 84, 89, 
    80, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_171 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 82, 80, 
    85, 83, 72, 32, 116, 97, 115, 107, 115, 32, 39, 84, 97, 115, 107, 
    32, 51, 39, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_109 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 82, 65, 
    78, 68, 79, 77, 75, 69, 89, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_76 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 80, 69, 
    88, 80, 73, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_67 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 65, 80, 
    80, 69, 78, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_192 =
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
} const moonbit_string_literal_154 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 40, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_103 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 90, 82, 
    69, 86, 82, 65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_197 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 32, 32, 
    9989, 32, 21629, 20196, 34892, 20132, 20114, 28436, 31034, 23436, 
    25104, 65281, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_110 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 82, 69, 
    78, 65, 77, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_39 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 83, 
    67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_169 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 76, 80, 
    85, 83, 72, 32, 116, 97, 115, 107, 115, 32, 39, 84, 97, 115, 107, 
    32, 49, 39, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_97 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_78 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 72, 69, 
    88, 73, 83, 84, 83, 0
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
} const moonbit_string_literal_125 =
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
} const moonbit_string_literal_178 =
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
} const moonbit_string_literal_158 =
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
} const moonbit_string_literal_176 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 83, 65, 
    68, 68, 32, 116, 97, 103, 115, 32, 109, 111, 111, 110, 98, 105, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_113 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 67, 79, 
    77, 77, 65, 78, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_166 =
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
} const moonbit_string_literal_177 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 83, 77, 
    69, 77, 66, 69, 82, 83, 32, 116, 97, 103, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_157 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 83, 69, 
    84, 32, 97, 103, 101, 32, 50, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_168 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 72, 76, 
    69, 78, 32, 117, 115, 101, 114, 58, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_62 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 84, 84, 76, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_160 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 73, 78, 
    67, 82, 32, 97, 103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[27]; 
} const moonbit_string_literal_165 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 26, 72, 83, 
    69, 84, 32, 117, 115, 101, 114, 58, 49, 32, 117, 115, 101, 114, 110, 
    97, 109, 101, 32, 97, 108, 105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_170 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 76, 80, 
    85, 83, 72, 32, 116, 97, 115, 107, 115, 32, 39, 84, 97, 115, 107, 
    32, 50, 39, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_115 =
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
} const moonbit_string_literal_159 =
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
} const moonbit_string_literal_146 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 30, 114, 97, 
    100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 98, 101, 
    116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_124 =
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
} const moonbit_string_literal_167 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 72, 71, 
    69, 84, 32, 117, 115, 101, 114, 58, 49, 32, 117, 115, 101, 114, 110, 
    97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_162 =
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
} const moonbit_string_literal_120 =
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
} const moonbit_string_literal_193 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 69, 88, 
    73, 83, 84, 83, 32, 110, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_152 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 44, 32, 
    108, 101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_144 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 102, 97, 
    108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_189 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 84, 89, 
    80, 69, 32, 117, 115, 101, 114, 58, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_149 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 98, 111, 
    117, 110, 100, 115, 32, 99, 104, 101, 99, 107, 32, 102, 97, 105, 
    108, 101, 100, 58, 32, 97, 108, 108, 111, 99, 97, 116, 101, 95, 108, 
    101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_98 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 90, 82, 
    69, 77, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_49 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 65, 
    68, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_107 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 90, 80, 
    79, 80, 77, 65, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_82 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 77, 
    71, 69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_137 =
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
} const moonbit_string_literal_128 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 54, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_18 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 84, 73, 
    77, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_179 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 22, 83, 73, 
    83, 77, 69, 77, 66, 69, 82, 32, 116, 97, 103, 115, 32, 109, 111, 
    111, 110, 98, 105, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_147 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 48, 49, 
    50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 103, 104, 
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 
    118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_94 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 68, 
    73, 70, 70, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_89 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 82, 80, 
    79, 80, 76, 80, 85, 83, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_183 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 90, 82, 
    65, 78, 71, 69, 32, 115, 99, 111, 114, 101, 115, 32, 48, 32, 45, 
    49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_114 =
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
} const moonbit_string_literal_172 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 76, 82, 
    65, 78, 71, 69, 32, 116, 97, 115, 107, 115, 32, 48, 32, 45, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_134 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_105 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 90, 82, 
    69, 77, 82, 65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_185 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 23, 90, 73, 
    78, 67, 82, 66, 89, 32, 115, 99, 111, 114, 101, 115, 32, 53, 48, 
    32, 97, 108, 105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_180 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 21, 90, 65, 
    68, 68, 32, 115, 99, 111, 114, 101, 115, 32, 49, 48, 48, 32, 97, 
    108, 105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_108 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 70, 76, 
    85, 83, 72, 65, 76, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_91 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 83, 73, 
    78, 84, 69, 82, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_90 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 83, 73, 
    78, 84, 69, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_19 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 32, 
    115, 101, 99, 111, 110, 100, 115, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_93 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 83, 85, 
    78, 73, 79, 78, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_155 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 21, 32, 32, 
    77, 111, 111, 110, 66, 105, 116, 68, 66, 32, 45, 32, 21629, 20196, 
    34892, 20132, 20114, 28436, 31034, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_81 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 77, 
    83, 69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_151 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    100, 115, 116, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_148 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 105, 110, 
    118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 111, 105, 
    110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_92 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 83, 85, 
    78, 73, 79, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_85 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 82, 
    69, 77, 0
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
} const moonbit_string_literal_145 =
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
} const moonbit_string_literal_194 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 32, 32, 
    55356, 57260, 32, 24320, 22987, 28436, 31034, 21629, 20196, 25191, 
    34892, 46, 46, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_188 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 84, 89, 
    80, 69, 32, 110, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_73 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 83, 69, 84, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_186 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 77, 83, 
    69, 84, 32, 97, 32, 49, 32, 98, 32, 50, 32, 99, 32, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_131 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 57, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_117 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_79 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 75, 
    69, 89, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_119 =
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
} const moonbit_string_literal_195 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 40, 45, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 
    45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_181 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 90, 65, 
    68, 68, 32, 115, 99, 111, 114, 101, 115, 32, 50, 48, 48, 32, 98, 
    111, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_86 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 76, 73, 
    78, 83, 69, 82, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_182 =
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
} const moonbit_string_literal_190 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 84, 89, 
    80, 69, 32, 116, 97, 115, 107, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_143 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 116, 114, 
    117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_163 =
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
} const moonbit_string_literal_196 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 32, 
    62, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_173 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 76, 76, 
    69, 78, 32, 116, 97, 115, 107, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_33 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_130 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 56, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_32 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 41, 32, 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_136 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 108, 105, 
    115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_88 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 82, 80, 
    85, 83, 72, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_100 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 67, 
    79, 85, 78, 84, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_111 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 82, 69, 
    78, 65, 77, 69, 78, 88, 0
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
} const moonbit_string_literal_135 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 104, 97, 
    115, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_112 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 69, 67, 
    72, 79, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_156 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 83, 69, 
    84, 32, 110, 97, 109, 101, 32, 65, 108, 105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_191 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 84, 89, 
    80, 69, 32, 115, 99, 111, 114, 101, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_142 =
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
} const _M0MPC16string6String9to__upperC2949l1791$closure =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_REGULAR),
    Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0),
    _M0MPC16string6String9to__upperC2949l1791
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWcERPC16string10StringView data;
  
} const _M0MPC16string10StringView5splitC2899l1145$closure =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_REGULAR),
    Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0),
    _M0MPC16string10StringView5splitC2899l1145
  };

uint32_t const moonbit_layout_table_data[171] =
  {
    sizeof(struct _M0TPB5ArrayGsE) / 4, 1,
    offsetof(struct _M0TPB5ArrayGsE, $0) / 4,
    sizeof(struct _M0DTP19moonbitDB10RedisValue4ZSet) / 4, 1,
    offsetof(struct _M0DTP19moonbitDB10RedisValue4ZSet, $0) / 4,
    sizeof(struct _M0TPB5ArrayGUsfEE) / 4, 1,
    offsetof(struct _M0TPB5ArrayGUsfEE, $0) / 4, sizeof(struct _M0TUsfE) / 4,
    1, offsetof(struct _M0TUsfE, $0) / 4,
    sizeof(struct _M0DTP19moonbitDB10RedisValue3Set) / 4, 1,
    offsetof(struct _M0DTP19moonbitDB10RedisValue3Set, $0) / 4,
    sizeof(struct _M0DTP19moonbitDB10RedisValue4List) / 4, 1,
    offsetof(struct _M0DTP19moonbitDB10RedisValue4List, $0) / 4,
    sizeof(struct _M0DTP19moonbitDB10RedisValue4Hash) / 4, 1,
    offsetof(struct _M0DTP19moonbitDB10RedisValue4Hash, $0) / 4,
    sizeof(struct _M0DTP19moonbitDB10RedisValue6String) / 4, 1,
    offsetof(struct _M0DTP19moonbitDB10RedisValue6String, $0) / 4,
    sizeof(struct _M0TPB5ArrayGOsE) / 4, 1,
    offsetof(struct _M0TPB5ArrayGOsE, $0) / 4,
    sizeof(struct _M0TP19moonbitDB8Database) / 4, 2,
    offsetof(struct _M0TP19moonbitDB8Database, $0) / 4,
    offsetof(struct _M0TP19moonbitDB8Database, $1) / 4,
    sizeof(struct _M0TP19moonbitDB5Deque) / 4, 2,
    offsetof(struct _M0TP19moonbitDB5Deque, $0) / 4,
    offsetof(struct _M0TP19moonbitDB5Deque, $1) / 4,
    sizeof(struct _M0TPB8MutLocalGORPB5EntryGssEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGssEE, $0) / 4,
    sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3582__l711__)
    / 4, 2,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3582__l711__, $0)
    / 4,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3582__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsbEE, $0) / 4,
    sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3592__l711__)
    / 4, 2,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3592__l711__, $0)
    / 4,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3592__l711__, $1)
    / 4,
    sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE)
    / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE, $0)
    / 4,
    sizeof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3602__l711__)
    / 4, 2,
    offsetof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3602__l711__, $0)
    / 4,
    offsetof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3602__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsfEE, $0) / 4,
    sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3612__l711__)
    / 4, 2,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3612__l711__, $0)
    / 4,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3612__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsiEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsiEE, $0) / 4,
    sizeof(struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3622__l711__)
    / 4, 2,
    offsetof(struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3622__l711__, $0)
    / 4,
    offsetof(struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3622__l711__, $1)
    / 4, sizeof(struct _M0TUsiE) / 4, 1, offsetof(struct _M0TUsiE, $0) / 4,
    sizeof(struct _M0TUsRP19moonbitDB10RedisValueE) / 4, 2,
    offsetof(struct _M0TUsRP19moonbitDB10RedisValueE, $0) / 4,
    offsetof(struct _M0TUsRP19moonbitDB10RedisValueE, $1) / 4,
    sizeof(struct _M0TUsbE) / 4, 1, offsetof(struct _M0TUsbE, $0) / 4,
    sizeof(struct _M0TUssE) / 4, 2, offsetof(struct _M0TUssE, $0) / 4,
    offsetof(struct _M0TUssE, $1) / 4,
    sizeof(struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE) / 4, 3,
    offsetof(struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE, $1) / 4,
    offsetof(struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE, $4) / 4,
    offsetof(struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE, $5) / 4,
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
    sizeof(struct _M0TPB5EntryGsiE) / 4, 2,
    offsetof(struct _M0TPB5EntryGsiE, $1) / 4,
    offsetof(struct _M0TPB5EntryGsiE, $4) / 4,
    sizeof(struct _M0TPB3MapGsRP19moonbitDB10RedisValueE) / 4, 2,
    offsetof(struct _M0TPB3MapGsRP19moonbitDB10RedisValueE, $0) / 4,
    offsetof(struct _M0TPB3MapGsRP19moonbitDB10RedisValueE, $5) / 4,
    sizeof(struct _M0TPB3MapGsiE) / 4, 2,
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
    sizeof(struct _M0R44StringView_3a_3asplit_2eanon__u2903__l1148__) / 4, 
    2,
    offsetof(struct _M0R44StringView_3a_3asplit_2eanon__u2903__l1148__, $0)
    / 4,
    (offsetof(struct _M0R44StringView_3a_3asplit_2eanon__u2903__l1148__, $1)
     + offsetof(struct _M0TPC16string10StringView, $0))
    / 4,
    sizeof(struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2892__l391__)
    / 4, 2,
    offsetof(struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2892__l391__, $0)
    / 4,
    offsetof(struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2892__l391__, $1)
    / 4, sizeof(struct _M0TPB4IterGRPC16string10StringViewE) / 4, 1,
    offsetof(struct _M0TPB4IterGRPC16string10StringViewE, $0) / 4,
    sizeof(struct _M0R42StringView_3a_3aiter_2eanon__u2763__l219__) / 4, 
    2,
    offsetof(struct _M0R42StringView_3a_3aiter_2eanon__u2763__l219__, $0) / 4,
    (offsetof(struct _M0R42StringView_3a_3aiter_2eanon__u2763__l219__, $2)
     + offsetof(struct _M0TPC16string10StringView, $0))
    / 4, sizeof(struct _M0TPB4IterGUssEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUssEE, $0) / 4,
    sizeof(struct _M0TPB4IterGUsbEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUsbEE, $0) / 4,
    sizeof(struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE, $0) / 4,
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

int64_t _M0MPB4Iter4nextN6constrS9980GUssEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUssEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GRPC16string10StringViewE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GRPC16string10StringViewE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsbEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsbEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsRP19moonbitDB10RedisValueEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsRP19moonbitDB10RedisValueEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsfEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsfEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsiEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsiEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GcE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GcE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUssEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsbEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsRP19moonbitDB10RedisValueEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GRPC16string10StringViewE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsfEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsiEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GcE = 0ll;

int64_t _M0FPB28boyer__moore__horspool__findN6constrS9990 = 0ll;

int64_t _M0FPB18brute__force__findN6constrS9991 = 0ll;

struct _M0TPB5ArrayGsE* _M0FP39moonbitDB8examples9cli__repl16execute__command(
  struct _M0TP19moonbitDB8Database* _M0L2dbS2196,
  moonbit_string_t _M0L3cmdS2194
) {
  struct _M0TPB5ArrayGsE* _M0L5partsS2193;
  int32_t _M0L6_2atmpS4386;
  moonbit_string_t _M0L6_2atmpS4678;
  moonbit_string_t _M0L7commandS2195;
  #line 105 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  #line 106 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L5partsS2193
  = _M0FP39moonbitDB8examples9cli__repl14split__command(_M0L3cmdS2194);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L6_2atmpS4386 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
  if (_M0L6_2atmpS4386 == 0) {
    moonbit_string_t* _M0L6_2atmpS4387;
    struct _M0TPB5ArrayGsE* _block_5229;
    moonbit_decref(_M0L5partsS2193);
    _M0L6_2atmpS4387 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS4387[0] = (moonbit_string_t)moonbit_string_literal_0.data;
    _block_5229
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5229)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5229->$0 = _M0L6_2atmpS4387;
    _block_5229->$1 = 1;
    return _block_5229;
  }
  #line 110 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L6_2atmpS4678 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 0);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L7commandS2195 = _M0MPC16string6String9to__upper(_M0L6_2atmpS4678);
  moonbit_decref(_M0L6_2atmpS4678);
  if (
    _M0L7commandS2195 == (moonbit_string_t)moonbit_string_literal_73.data
    || Moonbit_array_length(_M0L7commandS2195) == 3
       && 0
          == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_73.data, 6)
  ) {
    int32_t _M0L6_2atmpS4388;
    moonbit_decref(_M0L7commandS2195);
    #line 113 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4388 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4388 < 3) {
      moonbit_string_t* _M0L6_2atmpS4389;
      struct _M0TPB5ArrayGsE* _block_5364;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4389 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4389[0] = (moonbit_string_t)moonbit_string_literal_74.data;
      _block_5364
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5364)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5364->$0 = _M0L6_2atmpS4389;
      _block_5364->$1 = 1;
      return _block_5364;
    } else {
      moonbit_string_t _M0L6_2atmpS4390;
      moonbit_string_t _M0L6_2atmpS4391;
      moonbit_string_t* _M0L6_2atmpS4392;
      struct _M0TPB5ArrayGsE* _block_5365;
      #line 114 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4390 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      #line 114 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4391 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      moonbit_decref(_M0L5partsS2193);
      #line 114 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MP19moonbitDB8Database3set(_M0L2dbS2196, _M0L6_2atmpS4390, _M0L6_2atmpS4391);
      moonbit_decref(_M0L6_2atmpS4390);
      moonbit_decref(_M0L6_2atmpS4391);
      _M0L6_2atmpS4392 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4392[0] = (moonbit_string_t)moonbit_string_literal_26.data;
      _block_5365
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5365)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5365->$0 = _M0L6_2atmpS4392;
      _block_5365->$1 = 1;
      return _block_5365;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_72.data
           || Moonbit_array_length(_M0L7commandS2195) == 3
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_72.data, 6)
         ) {
    int32_t _M0L6_2atmpS4393;
    moonbit_decref(_M0L7commandS2195);
    #line 117 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4393 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4393 < 2) {
      moonbit_string_t* _M0L6_2atmpS4394;
      struct _M0TPB5ArrayGsE* _block_5360;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4394 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4394[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5360
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5360)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5360->$0 = _M0L6_2atmpS4394;
      _block_5360->$1 = 1;
      return _block_5360;
    } else {
      moonbit_string_t _M0L1vS2198;
      moonbit_string_t _M0L6_2atmpS4398;
      moonbit_string_t _M0L7_2abindS2200;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2199;
      moonbit_string_t _M0L6_2atmpS4396;
      moonbit_string_t* _M0L6_2atmpS4395;
      struct _M0TPB5ArrayGsE* _block_5363;
      #line 119 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4398 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 119 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2200
      = _M0MP19moonbitDB8Database3get(_M0L2dbS2196, _M0L6_2atmpS4398);
      moonbit_decref(_M0L6_2atmpS4398);
      if (_M0L7_2abindS2200 == 0) {
        moonbit_string_t* _M0L6_2atmpS4397;
        struct _M0TPB5ArrayGsE* _block_5362;
        if (_M0L7_2abindS2200) {
          moonbit_decref(_M0L7_2abindS2200);
        }
        _M0L6_2atmpS4397 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        _M0L6_2atmpS4397[0]
        = (moonbit_string_t)moonbit_string_literal_38.data;
        _block_5362
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5362)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5362->$0 = _M0L6_2atmpS4397;
        _block_5362->$1 = 1;
        return _block_5362;
      } else {
        moonbit_string_t _M0L7_2aSomeS2201 = _M0L7_2abindS2200;
        moonbit_string_t _M0L4_2avS2202 = _M0L7_2aSomeS2201;
        _M0L1vS2198 = _M0L4_2avS2202;
        goto join_2197;
      }
      join_2197:;
      #line 120 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2199
      = _M0MPB13StringBuilder21StringBuilder_2einner(2);
      #line 120 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2199, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 120 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2199, _M0L1vS2198);
      moonbit_decref(_M0L1vS2198);
      #line 120 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2199, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 120 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4396
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2199);
      moonbit_decref(_M0L18_2astring__builderS2199);
      _M0L6_2atmpS4395 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4395[0] = _M0L6_2atmpS4396;
      _block_5363
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5363)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5363->$0 = _M0L6_2atmpS4395;
      _block_5363->$1 = 1;
      return _block_5363;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_71.data
           || Moonbit_array_length(_M0L7commandS2195) == 3
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_71.data, 6)
         ) {
    int32_t _M0L6_2atmpS4399;
    moonbit_decref(_M0L7commandS2195);
    #line 126 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4399 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4399 < 2) {
      moonbit_string_t* _M0L6_2atmpS4400;
      struct _M0TPB5ArrayGsE* _block_5356;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4400 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4400[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5356
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5356)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5356->$0 = _M0L6_2atmpS4400;
      _block_5356->$1 = 1;
      return _block_5356;
    } else {
      moonbit_string_t _M0L6_2atmpS4401;
      int32_t _result_5357;
      #line 127 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4401 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 127 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5357
      = _M0MP19moonbitDB8Database3del(_M0L2dbS2196, _M0L6_2atmpS4401);
      moonbit_decref(_M0L6_2atmpS4401);
      if (_result_5357) {
        moonbit_string_t* _M0L6_2atmpS4402 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5358;
        _M0L6_2atmpS4402[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5358
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5358)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5358->$0 = _M0L6_2atmpS4402;
        _block_5358->$1 = 1;
        return _block_5358;
      } else {
        moonbit_string_t* _M0L6_2atmpS4403 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5359;
        _M0L6_2atmpS4403[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5359
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5359)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5359->$0 = _M0L6_2atmpS4403;
        _block_5359->$1 = 1;
        return _block_5359;
      }
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_70.data
           || Moonbit_array_length(_M0L7commandS2195) == 6
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_70.data, 12)
         ) {
    int32_t _M0L6_2atmpS4404;
    moonbit_decref(_M0L7commandS2195);
    #line 130 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4404 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4404 < 2) {
      moonbit_string_t* _M0L6_2atmpS4405;
      struct _M0TPB5ArrayGsE* _block_5352;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4405 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4405[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5352
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5352)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5352->$0 = _M0L6_2atmpS4405;
      _block_5352->$1 = 1;
      return _block_5352;
    } else {
      moonbit_string_t _M0L6_2atmpS4406;
      int32_t _result_5353;
      #line 131 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4406 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 131 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5353
      = _M0MP19moonbitDB8Database6exists(_M0L2dbS2196, _M0L6_2atmpS4406);
      moonbit_decref(_M0L6_2atmpS4406);
      if (_result_5353) {
        moonbit_string_t* _M0L6_2atmpS4407 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5354;
        _M0L6_2atmpS4407[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5354
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5354)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5354->$0 = _M0L6_2atmpS4407;
        _block_5354->$1 = 1;
        return _block_5354;
      } else {
        moonbit_string_t* _M0L6_2atmpS4408 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5355;
        _M0L6_2atmpS4408[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5355
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5355)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5355->$0 = _M0L6_2atmpS4408;
        _block_5355->$1 = 1;
        return _block_5355;
      }
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_69.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_69.data, 8)
         ) {
    struct _M0TPB5ArrayGsE* _M0L4keysS2203;
    moonbit_string_t* _M0L6_2atmpS4414;
    struct _M0TPB5ArrayGsE* _M0L6resultS2204;
    int32_t _M0L1iS2205;
    moonbit_decref(_M0L7commandS2195);
    moonbit_decref(_M0L5partsS2193);
    #line 134 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L4keysS2203 = _M0MP19moonbitDB8Database4keys(_M0L2dbS2196);
    _M0L6_2atmpS4414 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS2204
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS2204)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS2204->$0 = _M0L6_2atmpS4414;
    _M0L6resultS2204->$1 = 0;
    _M0L1iS2205 = 0;
    while (1) {
      int32_t _M0L6_2atmpS4409;
      #line 136 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4409 = _M0MPC15array5Array6lengthGsE(_M0L4keysS2203);
      if (_M0L1iS2205 < _M0L6_2atmpS4409) {
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2206;
        int32_t _M0L6_2atmpS4411;
        moonbit_string_t _M0L6_2atmpS4412;
        moonbit_string_t _M0L6_2atmpS4410;
        int32_t _M0L6_2atmpS4413;
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L18_2astring__builderS2206
        = _M0MPB13StringBuilder21StringBuilder_2einner(6);
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2206, (moonbit_string_t)moonbit_string_literal_23.data);
        _M0L6_2atmpS4411 = _M0L1iS2205 + 1;
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2206, _M0L6_2atmpS4411);
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2206, (moonbit_string_t)moonbit_string_literal_32.data);
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4412
        = _M0MPC15array5Array2atGsE(_M0L4keysS2203, _M0L1iS2205);
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2206, _M0L6_2atmpS4412);
        moonbit_decref(_M0L6_2atmpS4412);
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2206, (moonbit_string_t)moonbit_string_literal_33.data);
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4410
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2206);
        moonbit_decref(_M0L18_2astring__builderS2206);
        #line 137 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS2204, _M0L6_2atmpS4410);
        moonbit_decref(_M0L6_2atmpS4410);
        _M0L6_2atmpS4413 = _M0L1iS2205 + 1;
        _M0L1iS2205 = _M0L6_2atmpS4413;
        continue;
      } else {
        moonbit_decref(_M0L4keysS2203);
      }
      break;
    }
    return _M0L6resultS2204;
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_68.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_68.data, 8)
         ) {
    int32_t _M0L6_2atmpS4415;
    moonbit_decref(_M0L7commandS2195);
    #line 142 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4415 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4415 < 2) {
      moonbit_string_t* _M0L6_2atmpS4416;
      struct _M0TPB5ArrayGsE* _block_5349;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4416 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4416[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5349
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5349)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5349->$0 = _M0L6_2atmpS4416;
      _block_5349->$1 = 1;
      return _block_5349;
    } else {
      moonbit_string_t _M0L6_2atmpS4419;
      moonbit_string_t _M0L6_2atmpS4418;
      moonbit_string_t* _M0L6_2atmpS4417;
      struct _M0TPB5ArrayGsE* _block_5350;
      #line 143 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4419 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 143 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4418
      = _M0MP19moonbitDB8Database8type__of(_M0L2dbS2196, _M0L6_2atmpS4419);
      moonbit_decref(_M0L6_2atmpS4419);
      _M0L6_2atmpS4417 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4417[0] = _M0L6_2atmpS4418;
      _block_5350
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5350)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5350->$0 = _M0L6_2atmpS4417;
      _block_5350->$1 = 1;
      return _block_5350;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_67.data
           || Moonbit_array_length(_M0L7commandS2195) == 6
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_67.data, 12)
         ) {
    int32_t _M0L6_2atmpS4420;
    moonbit_decref(_M0L7commandS2195);
    #line 146 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4420 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4420 < 3) {
      moonbit_string_t* _M0L6_2atmpS4421;
      struct _M0TPB5ArrayGsE* _block_5347;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4421 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4421[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5347
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5347)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5347->$0 = _M0L6_2atmpS4421;
      _block_5347->$1 = 1;
      return _block_5347;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2208;
      moonbit_string_t _M0L6_2atmpS4425;
      moonbit_string_t _M0L6_2atmpS4426;
      int32_t _M0L6_2atmpS4424;
      moonbit_string_t _M0L6_2atmpS4423;
      moonbit_string_t* _M0L6_2atmpS4422;
      struct _M0TPB5ArrayGsE* _block_5348;
      #line 147 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2208
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 147 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2208, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 147 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4425 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      #line 147 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4426 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      moonbit_decref(_M0L5partsS2193);
      #line 147 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4424
      = _M0MP19moonbitDB8Database6append(_M0L2dbS2196, _M0L6_2atmpS4425, _M0L6_2atmpS4426);
      moonbit_decref(_M0L6_2atmpS4425);
      moonbit_decref(_M0L6_2atmpS4426);
      #line 147 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2208, _M0L6_2atmpS4424);
      #line 147 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4423
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2208);
      moonbit_decref(_M0L18_2astring__builderS2208);
      _M0L6_2atmpS4422 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4422[0] = _M0L6_2atmpS4423;
      _block_5348
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5348)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5348->$0 = _M0L6_2atmpS4422;
      _block_5348->$1 = 1;
      return _block_5348;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_66.data
           || Moonbit_array_length(_M0L7commandS2195) == 6
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_66.data, 12)
         ) {
    int32_t _M0L6_2atmpS4427;
    moonbit_decref(_M0L7commandS2195);
    #line 150 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4427 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4427 < 2) {
      moonbit_string_t* _M0L6_2atmpS4428;
      struct _M0TPB5ArrayGsE* _block_5345;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4428 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4428[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5345
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5345)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5345->$0 = _M0L6_2atmpS4428;
      _block_5345->$1 = 1;
      return _block_5345;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2209;
      moonbit_string_t _M0L6_2atmpS4432;
      int32_t _M0L6_2atmpS4431;
      moonbit_string_t _M0L6_2atmpS4430;
      moonbit_string_t* _M0L6_2atmpS4429;
      struct _M0TPB5ArrayGsE* _block_5346;
      #line 151 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2209
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2209, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4432 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4431
      = _M0MP19moonbitDB8Database6strlen(_M0L2dbS2196, _M0L6_2atmpS4432);
      moonbit_decref(_M0L6_2atmpS4432);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2209, _M0L6_2atmpS4431);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4430
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2209);
      moonbit_decref(_M0L18_2astring__builderS2209);
      _M0L6_2atmpS4429 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4429[0] = _M0L6_2atmpS4430;
      _block_5346
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5346)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5346->$0 = _M0L6_2atmpS4429;
      _block_5346->$1 = 1;
      return _block_5346;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_65.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_65.data, 8)
         ) {
    int32_t _M0L6_2atmpS4433;
    moonbit_decref(_M0L7commandS2195);
    #line 154 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4433 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4433 < 2) {
      moonbit_string_t* _M0L6_2atmpS4434;
      struct _M0TPB5ArrayGsE* _block_5341;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4434 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4434[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5341
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5341)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5341->$0 = _M0L6_2atmpS4434;
      _block_5341->$1 = 1;
      return _block_5341;
    } else {
      int32_t _M0L1vS2211;
      moonbit_string_t _M0L6_2atmpS4438;
      int64_t _M0L7_2abindS2213;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2212;
      moonbit_string_t _M0L6_2atmpS4436;
      moonbit_string_t* _M0L6_2atmpS4435;
      struct _M0TPB5ArrayGsE* _block_5344;
      #line 156 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4438 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 156 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2213
      = _M0MP19moonbitDB8Database4incr(_M0L2dbS2196, _M0L6_2atmpS4438);
      moonbit_decref(_M0L6_2atmpS4438);
      if (_M0L7_2abindS2213 == 4294967296ll) {
        moonbit_string_t* _M0L6_2atmpS4437 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5343;
        _M0L6_2atmpS4437[0]
        = (moonbit_string_t)moonbit_string_literal_15.data;
        _block_5343
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5343)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5343->$0 = _M0L6_2atmpS4437;
        _block_5343->$1 = 1;
        return _block_5343;
      } else {
        int64_t _M0L7_2aSomeS2214 = _M0L7_2abindS2213;
        int32_t _M0L4_2avS2215 = (int32_t)_M0L7_2aSomeS2214;
        _M0L1vS2211 = _M0L4_2avS2215;
        goto join_2210;
      }
      join_2210:;
      #line 157 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2212
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 157 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2212, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 157 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2212, _M0L1vS2211);
      #line 157 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4436
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2212);
      moonbit_decref(_M0L18_2astring__builderS2212);
      _M0L6_2atmpS4435 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4435[0] = _M0L6_2atmpS4436;
      _block_5344
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5344)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5344->$0 = _M0L6_2atmpS4435;
      _block_5344->$1 = 1;
      return _block_5344;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_64.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_64.data, 8)
         ) {
    int32_t _M0L6_2atmpS4439;
    moonbit_decref(_M0L7commandS2195);
    #line 163 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4439 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4439 < 2) {
      moonbit_string_t* _M0L6_2atmpS4440;
      struct _M0TPB5ArrayGsE* _block_5337;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4440 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4440[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5337
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5337)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5337->$0 = _M0L6_2atmpS4440;
      _block_5337->$1 = 1;
      return _block_5337;
    } else {
      int32_t _M0L1vS2217;
      moonbit_string_t _M0L6_2atmpS4444;
      int64_t _M0L7_2abindS2219;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2218;
      moonbit_string_t _M0L6_2atmpS4442;
      moonbit_string_t* _M0L6_2atmpS4441;
      struct _M0TPB5ArrayGsE* _block_5340;
      #line 165 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4444 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 165 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2219
      = _M0MP19moonbitDB8Database4decr(_M0L2dbS2196, _M0L6_2atmpS4444);
      moonbit_decref(_M0L6_2atmpS4444);
      if (_M0L7_2abindS2219 == 4294967296ll) {
        moonbit_string_t* _M0L6_2atmpS4443 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5339;
        _M0L6_2atmpS4443[0]
        = (moonbit_string_t)moonbit_string_literal_15.data;
        _block_5339
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5339)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5339->$0 = _M0L6_2atmpS4443;
        _block_5339->$1 = 1;
        return _block_5339;
      } else {
        int64_t _M0L7_2aSomeS2220 = _M0L7_2abindS2219;
        int32_t _M0L4_2avS2221 = (int32_t)_M0L7_2aSomeS2220;
        _M0L1vS2217 = _M0L4_2avS2221;
        goto join_2216;
      }
      join_2216:;
      #line 166 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2218
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 166 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2218, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 166 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2218, _M0L1vS2217);
      #line 166 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4442
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2218);
      moonbit_decref(_M0L18_2astring__builderS2218);
      _M0L6_2atmpS4441 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4441[0] = _M0L6_2atmpS4442;
      _block_5340
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5340)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5340->$0 = _M0L6_2atmpS4441;
      _block_5340->$1 = 1;
      return _block_5340;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_63.data
           || Moonbit_array_length(_M0L7commandS2195) == 6
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_63.data, 12)
         ) {
    int32_t _M0L6_2atmpS4445;
    moonbit_decref(_M0L7commandS2195);
    #line 172 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4445 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4445 < 3) {
      moonbit_string_t* _M0L6_2atmpS4446;
      struct _M0TPB5ArrayGsE* _block_5331;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4446 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4446[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5331
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5331)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5331->$0 = _M0L6_2atmpS4446;
      _block_5331->$1 = 1;
      return _block_5331;
    } else {
      int32_t _M0L1sS2223;
      moonbit_string_t _M0L6_2atmpS4451;
      int64_t _M0L7_2abindS2224;
      moonbit_string_t _M0L6_2atmpS4447;
      int32_t _result_5334;
      #line 174 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4451 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      #line 174 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2224
      = _M0FP39moonbitDB8examples9cli__repl10parse__int(_M0L6_2atmpS4451);
      moonbit_decref(_M0L6_2atmpS4451);
      if (_M0L7_2abindS2224 == 4294967296ll) {
        moonbit_string_t* _M0L6_2atmpS4450;
        struct _M0TPB5ArrayGsE* _block_5333;
        moonbit_decref(_M0L5partsS2193);
        _M0L6_2atmpS4450 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        _M0L6_2atmpS4450[0]
        = (moonbit_string_t)moonbit_string_literal_15.data;
        _block_5333
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5333)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5333->$0 = _M0L6_2atmpS4450;
        _block_5333->$1 = 1;
        return _block_5333;
      } else {
        int64_t _M0L7_2aSomeS2225 = _M0L7_2abindS2224;
        int32_t _M0L4_2asS2226 = (int32_t)_M0L7_2aSomeS2225;
        _M0L1sS2223 = _M0L4_2asS2226;
        goto join_2222;
      }
      join_2222:;
      #line 175 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4447 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 175 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5334
      = _M0MP19moonbitDB8Database6expire(_M0L2dbS2196, _M0L6_2atmpS4447, _M0L1sS2223);
      moonbit_decref(_M0L6_2atmpS4447);
      if (_result_5334) {
        moonbit_string_t* _M0L6_2atmpS4448 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5335;
        _M0L6_2atmpS4448[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5335
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5335)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5335->$0 = _M0L6_2atmpS4448;
        _block_5335->$1 = 1;
        return _block_5335;
      } else {
        moonbit_string_t* _M0L6_2atmpS4449 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5336;
        _M0L6_2atmpS4449[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5336
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5336)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5336->$0 = _M0L6_2atmpS4449;
        _block_5336->$1 = 1;
        return _block_5336;
      }
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_62.data
           || Moonbit_array_length(_M0L7commandS2195) == 3
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_62.data, 6)
         ) {
    int32_t _M0L6_2atmpS4452;
    moonbit_decref(_M0L7commandS2195);
    #line 181 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4452 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4452 < 2) {
      moonbit_string_t* _M0L6_2atmpS4453;
      struct _M0TPB5ArrayGsE* _block_5329;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4453 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4453[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5329
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5329)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5329->$0 = _M0L6_2atmpS4453;
      _block_5329->$1 = 1;
      return _block_5329;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2227;
      moonbit_string_t _M0L6_2atmpS4457;
      int32_t _M0L6_2atmpS4456;
      moonbit_string_t _M0L6_2atmpS4455;
      moonbit_string_t* _M0L6_2atmpS4454;
      struct _M0TPB5ArrayGsE* _block_5330;
      #line 182 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2227
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 182 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2227, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 182 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4457 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 182 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4456
      = _M0MP19moonbitDB8Database3ttl(_M0L2dbS2196, _M0L6_2atmpS4457);
      moonbit_decref(_M0L6_2atmpS4457);
      #line 182 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2227, _M0L6_2atmpS4456);
      #line 182 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4455
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2227);
      moonbit_decref(_M0L18_2astring__builderS2227);
      _M0L6_2atmpS4454 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4454[0] = _M0L6_2atmpS4455;
      _block_5330
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5330)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5330->$0 = _M0L6_2atmpS4454;
      _block_5330->$1 = 1;
      return _block_5330;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_61.data
           || Moonbit_array_length(_M0L7commandS2195) == 7
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_61.data, 14)
         ) {
    int32_t _M0L6_2atmpS4458;
    moonbit_decref(_M0L7commandS2195);
    #line 185 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4458 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4458 < 2) {
      moonbit_string_t* _M0L6_2atmpS4459;
      struct _M0TPB5ArrayGsE* _block_5325;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4459 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4459[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5325
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5325)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5325->$0 = _M0L6_2atmpS4459;
      _block_5325->$1 = 1;
      return _block_5325;
    } else {
      moonbit_string_t _M0L6_2atmpS4460;
      int32_t _result_5326;
      #line 186 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4460 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 186 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5326
      = _M0MP19moonbitDB8Database7persist(_M0L2dbS2196, _M0L6_2atmpS4460);
      moonbit_decref(_M0L6_2atmpS4460);
      if (_result_5326) {
        moonbit_string_t* _M0L6_2atmpS4461 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5327;
        _M0L6_2atmpS4461[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5327
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5327)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5327->$0 = _M0L6_2atmpS4461;
        _block_5327->$1 = 1;
        return _block_5327;
      } else {
        moonbit_string_t* _M0L6_2atmpS4462 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5328;
        _M0L6_2atmpS4462[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5328
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5328)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5328->$0 = _M0L6_2atmpS4462;
        _block_5328->$1 = 1;
        return _block_5328;
      }
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_60.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_60.data, 8)
         ) {
    int32_t _M0L6_2atmpS4463;
    moonbit_decref(_M0L7commandS2195);
    #line 189 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4463 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4463 < 4) {
      moonbit_string_t* _M0L6_2atmpS4464;
      struct _M0TPB5ArrayGsE* _block_5323;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4464 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4464[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5323
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5323)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5323->$0 = _M0L6_2atmpS4464;
      _block_5323->$1 = 1;
      return _block_5323;
    } else {
      moonbit_string_t _M0L6_2atmpS4466;
      moonbit_string_t _M0L6_2atmpS4467;
      moonbit_string_t _M0L6_2atmpS4468;
      int32_t _M0L6_2atmpS4465;
      moonbit_string_t* _M0L6_2atmpS4469;
      struct _M0TPB5ArrayGsE* _block_5324;
      #line 190 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4466 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      #line 190 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4467 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      #line 190 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4468 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 3);
      moonbit_decref(_M0L5partsS2193);
      #line 190 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4465
      = _M0MP19moonbitDB8Database4hset(_M0L2dbS2196, _M0L6_2atmpS4466, _M0L6_2atmpS4467, _M0L6_2atmpS4468);
      moonbit_decref(_M0L6_2atmpS4466);
      moonbit_decref(_M0L6_2atmpS4467);
      moonbit_decref(_M0L6_2atmpS4468);
      _M0L6_2atmpS4469 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4469[0] = (moonbit_string_t)moonbit_string_literal_26.data;
      _block_5324
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5324)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5324->$0 = _M0L6_2atmpS4469;
      _block_5324->$1 = 1;
      return _block_5324;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_59.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_59.data, 8)
         ) {
    int32_t _M0L6_2atmpS4470;
    moonbit_decref(_M0L7commandS2195);
    #line 193 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4470 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4470 < 3) {
      moonbit_string_t* _M0L6_2atmpS4471;
      struct _M0TPB5ArrayGsE* _block_5319;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4471 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4471[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5319
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5319)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5319->$0 = _M0L6_2atmpS4471;
      _block_5319->$1 = 1;
      return _block_5319;
    } else {
      moonbit_string_t _M0L1vS2229;
      moonbit_string_t _M0L6_2atmpS4475;
      moonbit_string_t _M0L6_2atmpS4476;
      moonbit_string_t _M0L7_2abindS2231;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2230;
      moonbit_string_t _M0L6_2atmpS4473;
      moonbit_string_t* _M0L6_2atmpS4472;
      struct _M0TPB5ArrayGsE* _block_5322;
      #line 195 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4475 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      #line 195 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4476 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      moonbit_decref(_M0L5partsS2193);
      #line 195 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2231
      = _M0MP19moonbitDB8Database4hget(_M0L2dbS2196, _M0L6_2atmpS4475, _M0L6_2atmpS4476);
      moonbit_decref(_M0L6_2atmpS4475);
      moonbit_decref(_M0L6_2atmpS4476);
      if (_M0L7_2abindS2231 == 0) {
        moonbit_string_t* _M0L6_2atmpS4474;
        struct _M0TPB5ArrayGsE* _block_5321;
        if (_M0L7_2abindS2231) {
          moonbit_decref(_M0L7_2abindS2231);
        }
        _M0L6_2atmpS4474 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        _M0L6_2atmpS4474[0]
        = (moonbit_string_t)moonbit_string_literal_38.data;
        _block_5321
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5321)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5321->$0 = _M0L6_2atmpS4474;
        _block_5321->$1 = 1;
        return _block_5321;
      } else {
        moonbit_string_t _M0L7_2aSomeS2232 = _M0L7_2abindS2231;
        moonbit_string_t _M0L4_2avS2233 = _M0L7_2aSomeS2232;
        _M0L1vS2229 = _M0L4_2avS2233;
        goto join_2228;
      }
      join_2228:;
      #line 196 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2230
      = _M0MPB13StringBuilder21StringBuilder_2einner(2);
      #line 196 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2230, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 196 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2230, _M0L1vS2229);
      moonbit_decref(_M0L1vS2229);
      #line 196 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2230, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 196 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4473
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2230);
      moonbit_decref(_M0L18_2astring__builderS2230);
      _M0L6_2atmpS4472 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4472[0] = _M0L6_2atmpS4473;
      _block_5322
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5322)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5322->$0 = _M0L6_2atmpS4472;
      _block_5322->$1 = 1;
      return _block_5322;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_58.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_58.data, 8)
         ) {
    int32_t _M0L6_2atmpS4477;
    moonbit_decref(_M0L7commandS2195);
    #line 202 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4477 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4477 < 3) {
      moonbit_string_t* _M0L6_2atmpS4478;
      struct _M0TPB5ArrayGsE* _block_5315;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4478 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4478[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5315
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5315)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5315->$0 = _M0L6_2atmpS4478;
      _block_5315->$1 = 1;
      return _block_5315;
    } else {
      moonbit_string_t _M0L6_2atmpS4479;
      moonbit_string_t _M0L6_2atmpS4480;
      int32_t _result_5316;
      #line 203 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4479 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      #line 203 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4480 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      moonbit_decref(_M0L5partsS2193);
      #line 203 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5316
      = _M0MP19moonbitDB8Database4hdel(_M0L2dbS2196, _M0L6_2atmpS4479, _M0L6_2atmpS4480);
      moonbit_decref(_M0L6_2atmpS4479);
      moonbit_decref(_M0L6_2atmpS4480);
      if (_result_5316) {
        moonbit_string_t* _M0L6_2atmpS4481 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5317;
        _M0L6_2atmpS4481[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5317
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5317)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5317->$0 = _M0L6_2atmpS4481;
        _block_5317->$1 = 1;
        return _block_5317;
      } else {
        moonbit_string_t* _M0L6_2atmpS4482 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5318;
        _M0L6_2atmpS4482[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5318
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5318)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5318->$0 = _M0L6_2atmpS4482;
        _block_5318->$1 = 1;
        return _block_5318;
      }
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_57.data
           || Moonbit_array_length(_M0L7commandS2195) == 7
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_57.data, 14)
         ) {
    int32_t _M0L6_2atmpS4483;
    moonbit_decref(_M0L7commandS2195);
    #line 206 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4483 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4483 < 2) {
      moonbit_string_t* _M0L6_2atmpS4484;
      struct _M0TPB5ArrayGsE* _block_5312;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4484 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4484[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5312
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5312)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5312->$0 = _M0L6_2atmpS4484;
      _block_5312->$1 = 1;
      return _block_5312;
    } else {
      moonbit_string_t _M0L6_2atmpS4493;
      struct _M0TPB3MapGssE* _M0L3allS2234;
      moonbit_string_t* _M0L6_2atmpS4492;
      struct _M0TPB5ArrayGsE* _M0L6resultS2235;
      struct _M0TPB8MutLocalGiE* _M0L3idxS2236;
      struct _M0TPB4IterGUssEE* _M0L5_2aitS2237;
      #line 208 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4493 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 208 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L3allS2234
      = _M0MP19moonbitDB8Database7hgetall(_M0L2dbS2196, _M0L6_2atmpS4493);
      moonbit_decref(_M0L6_2atmpS4493);
      _M0L6_2atmpS4492 = (moonbit_string_t*)moonbit_empty_ref_array;
      _M0L6resultS2235
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_M0L6resultS2235)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _M0L6resultS2235->$0 = _M0L6_2atmpS4492;
      _M0L6resultS2235->$1 = 0;
      _M0L3idxS2236
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L3idxS2236)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L3idxS2236->$0 = 1;
      #line 210 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L5_2aitS2237 = _M0MPB3Map5iter2GssE(_M0L3allS2234);
      moonbit_decref(_M0L3allS2234);
      while (1) {
        moonbit_string_t _M0L1fS2239;
        moonbit_string_t _M0L1vS2240;
        struct _M0TUssE* _M0L7_2abindS2244;
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2241;
        int32_t _M0L3valS4486;
        moonbit_string_t _M0L6_2atmpS4485;
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2242;
        int32_t _M0L3valS4489;
        int32_t _M0L6_2atmpS4488;
        moonbit_string_t _M0L6_2atmpS4487;
        int32_t _M0L3valS4491;
        int32_t _M0L6_2atmpS4490;
        #line 211 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L7_2abindS2244 = _M0MPB5Iter24nextGssE(_M0L5_2aitS2237);
        if (_M0L7_2abindS2244 == 0) {
          if (_M0L7_2abindS2244) {
            moonbit_decref(_M0L7_2abindS2244);
          }
          moonbit_decref(_M0L5_2aitS2237);
          moonbit_decref(_M0L3idxS2236);
        } else {
          struct _M0TUssE* _M0L7_2aSomeS2245 = _M0L7_2abindS2244;
          struct _M0TUssE* _M0L4_2axS2246 = _M0L7_2aSomeS2245;
          moonbit_string_t _M0L4_2afS2247 = _M0L4_2axS2246->$0;
          moonbit_string_t _M0L8_2afieldS4679 = _M0L4_2axS2246->$1;
          int32_t _M0L6_2acntS5141 =
            Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS2246));
          moonbit_string_t _M0L4_2avS2248;
          if (_M0L6_2acntS5141 > 1) {
            int32_t _M0L11_2anew__cntS5142 = _M0L6_2acntS5141 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS2246), _M0L11_2anew__cntS5142);
            moonbit_incref(_M0L8_2afieldS4679);
            moonbit_incref(_M0L4_2afS2247);
          } else if (_M0L6_2acntS5141 == 1) {
            #line 211 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            moonbit_free(_M0L4_2axS2246);
          }
          _M0L4_2avS2248 = _M0L8_2afieldS4679;
          _M0L1fS2239 = _M0L4_2afS2247;
          _M0L1vS2240 = _M0L4_2avS2248;
          goto join_2238;
        }
        goto joinlet_5314;
        join_2238:;
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L18_2astring__builderS2241
        = _M0MPB13StringBuilder21StringBuilder_2einner(6);
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2241, (moonbit_string_t)moonbit_string_literal_23.data);
        _M0L3valS4486 = _M0L3idxS2236->$0;
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2241, _M0L3valS4486);
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2241, (moonbit_string_t)moonbit_string_literal_32.data);
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2241, _M0L1fS2239);
        moonbit_decref(_M0L1fS2239);
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2241, (moonbit_string_t)moonbit_string_literal_33.data);
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4485
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2241);
        moonbit_decref(_M0L18_2astring__builderS2241);
        #line 212 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS2235, _M0L6_2atmpS4485);
        moonbit_decref(_M0L6_2atmpS4485);
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L18_2astring__builderS2242
        = _M0MPB13StringBuilder21StringBuilder_2einner(6);
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2242, (moonbit_string_t)moonbit_string_literal_23.data);
        _M0L3valS4489 = _M0L3idxS2236->$0;
        _M0L6_2atmpS4488 = _M0L3valS4489 + 1;
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2242, _M0L6_2atmpS4488);
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2242, (moonbit_string_t)moonbit_string_literal_32.data);
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2242, _M0L1vS2240);
        moonbit_decref(_M0L1vS2240);
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2242, (moonbit_string_t)moonbit_string_literal_33.data);
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4487
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2242);
        moonbit_decref(_M0L18_2astring__builderS2242);
        #line 213 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS2235, _M0L6_2atmpS4487);
        moonbit_decref(_M0L6_2atmpS4487);
        _M0L3valS4491 = _M0L3idxS2236->$0;
        _M0L6_2atmpS4490 = _M0L3valS4491 + 2;
        _M0L3idxS2236->$0 = _M0L6_2atmpS4490;
        continue;
        joinlet_5314:;
        break;
      }
      return _M0L6resultS2235;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_56.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_56.data, 8)
         ) {
    int32_t _M0L6_2atmpS4494;
    moonbit_decref(_M0L7commandS2195);
    #line 220 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4494 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4494 < 2) {
      moonbit_string_t* _M0L6_2atmpS4495;
      struct _M0TPB5ArrayGsE* _block_5310;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4495 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4495[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5310
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5310)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5310->$0 = _M0L6_2atmpS4495;
      _block_5310->$1 = 1;
      return _block_5310;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2249;
      moonbit_string_t _M0L6_2atmpS4499;
      int32_t _M0L6_2atmpS4498;
      moonbit_string_t _M0L6_2atmpS4497;
      moonbit_string_t* _M0L6_2atmpS4496;
      struct _M0TPB5ArrayGsE* _block_5311;
      #line 221 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2249
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 221 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2249, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 221 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4499 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 221 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4498
      = _M0MP19moonbitDB8Database4hlen(_M0L2dbS2196, _M0L6_2atmpS4499);
      moonbit_decref(_M0L6_2atmpS4499);
      #line 221 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2249, _M0L6_2atmpS4498);
      #line 221 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4497
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2249);
      moonbit_decref(_M0L18_2astring__builderS2249);
      _M0L6_2atmpS4496 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4496[0] = _M0L6_2atmpS4497;
      _block_5311
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5311)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5311->$0 = _M0L6_2atmpS4496;
      _block_5311->$1 = 1;
      return _block_5311;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_55.data
           || Moonbit_array_length(_M0L7commandS2195) == 5
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_55.data, 10)
         ) {
    int32_t _M0L6_2atmpS4500;
    moonbit_decref(_M0L7commandS2195);
    #line 224 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4500 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4500 < 3) {
      moonbit_string_t* _M0L6_2atmpS4501;
      struct _M0TPB5ArrayGsE* _block_5308;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4501 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4501[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5308
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5308)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5308->$0 = _M0L6_2atmpS4501;
      _block_5308->$1 = 1;
      return _block_5308;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2250;
      moonbit_string_t _M0L6_2atmpS4505;
      moonbit_string_t _M0L6_2atmpS4506;
      int32_t _M0L6_2atmpS4504;
      moonbit_string_t _M0L6_2atmpS4503;
      moonbit_string_t* _M0L6_2atmpS4502;
      struct _M0TPB5ArrayGsE* _block_5309;
      #line 225 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2250
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 225 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2250, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 225 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4505 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      #line 225 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4506 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      moonbit_decref(_M0L5partsS2193);
      #line 225 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4504
      = _M0MP19moonbitDB8Database5lpush(_M0L2dbS2196, _M0L6_2atmpS4505, _M0L6_2atmpS4506);
      moonbit_decref(_M0L6_2atmpS4505);
      moonbit_decref(_M0L6_2atmpS4506);
      #line 225 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2250, _M0L6_2atmpS4504);
      #line 225 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4503
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2250);
      moonbit_decref(_M0L18_2astring__builderS2250);
      _M0L6_2atmpS4502 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4502[0] = _M0L6_2atmpS4503;
      _block_5309
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5309)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5309->$0 = _M0L6_2atmpS4502;
      _block_5309->$1 = 1;
      return _block_5309;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_54.data
           || Moonbit_array_length(_M0L7commandS2195) == 5
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_54.data, 10)
         ) {
    int32_t _M0L6_2atmpS4507;
    moonbit_decref(_M0L7commandS2195);
    #line 228 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4507 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4507 < 3) {
      moonbit_string_t* _M0L6_2atmpS4508;
      struct _M0TPB5ArrayGsE* _block_5306;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4508 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4508[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5306
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5306)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5306->$0 = _M0L6_2atmpS4508;
      _block_5306->$1 = 1;
      return _block_5306;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2251;
      moonbit_string_t _M0L6_2atmpS4512;
      moonbit_string_t _M0L6_2atmpS4513;
      int32_t _M0L6_2atmpS4511;
      moonbit_string_t _M0L6_2atmpS4510;
      moonbit_string_t* _M0L6_2atmpS4509;
      struct _M0TPB5ArrayGsE* _block_5307;
      #line 229 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2251
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 229 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2251, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 229 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4512 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      #line 229 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4513 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      moonbit_decref(_M0L5partsS2193);
      #line 229 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4511
      = _M0MP19moonbitDB8Database5rpush(_M0L2dbS2196, _M0L6_2atmpS4512, _M0L6_2atmpS4513);
      moonbit_decref(_M0L6_2atmpS4512);
      moonbit_decref(_M0L6_2atmpS4513);
      #line 229 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2251, _M0L6_2atmpS4511);
      #line 229 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4510
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2251);
      moonbit_decref(_M0L18_2astring__builderS2251);
      _M0L6_2atmpS4509 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4509[0] = _M0L6_2atmpS4510;
      _block_5307
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5307)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5307->$0 = _M0L6_2atmpS4509;
      _block_5307->$1 = 1;
      return _block_5307;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_53.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_53.data, 8)
         ) {
    int32_t _M0L6_2atmpS4514;
    moonbit_decref(_M0L7commandS2195);
    #line 232 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4514 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4514 < 2) {
      moonbit_string_t* _M0L6_2atmpS4515;
      struct _M0TPB5ArrayGsE* _block_5302;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4515 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4515[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5302
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5302)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5302->$0 = _M0L6_2atmpS4515;
      _block_5302->$1 = 1;
      return _block_5302;
    } else {
      moonbit_string_t _M0L1vS2253;
      moonbit_string_t _M0L6_2atmpS4519;
      moonbit_string_t _M0L7_2abindS2255;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2254;
      moonbit_string_t _M0L6_2atmpS4517;
      moonbit_string_t* _M0L6_2atmpS4516;
      struct _M0TPB5ArrayGsE* _block_5305;
      #line 234 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4519 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 234 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2255
      = _M0MP19moonbitDB8Database4lpop(_M0L2dbS2196, _M0L6_2atmpS4519);
      moonbit_decref(_M0L6_2atmpS4519);
      if (_M0L7_2abindS2255 == 0) {
        moonbit_string_t* _M0L6_2atmpS4518;
        struct _M0TPB5ArrayGsE* _block_5304;
        if (_M0L7_2abindS2255) {
          moonbit_decref(_M0L7_2abindS2255);
        }
        _M0L6_2atmpS4518 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        _M0L6_2atmpS4518[0]
        = (moonbit_string_t)moonbit_string_literal_38.data;
        _block_5304
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5304)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5304->$0 = _M0L6_2atmpS4518;
        _block_5304->$1 = 1;
        return _block_5304;
      } else {
        moonbit_string_t _M0L7_2aSomeS2256 = _M0L7_2abindS2255;
        moonbit_string_t _M0L4_2avS2257 = _M0L7_2aSomeS2256;
        _M0L1vS2253 = _M0L4_2avS2257;
        goto join_2252;
      }
      join_2252:;
      #line 235 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2254
      = _M0MPB13StringBuilder21StringBuilder_2einner(2);
      #line 235 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2254, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 235 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2254, _M0L1vS2253);
      moonbit_decref(_M0L1vS2253);
      #line 235 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2254, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 235 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4517
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2254);
      moonbit_decref(_M0L18_2astring__builderS2254);
      _M0L6_2atmpS4516 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4516[0] = _M0L6_2atmpS4517;
      _block_5305
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5305)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5305->$0 = _M0L6_2atmpS4516;
      _block_5305->$1 = 1;
      return _block_5305;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_52.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_52.data, 8)
         ) {
    int32_t _M0L6_2atmpS4520;
    moonbit_decref(_M0L7commandS2195);
    #line 241 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4520 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4520 < 2) {
      moonbit_string_t* _M0L6_2atmpS4521;
      struct _M0TPB5ArrayGsE* _block_5298;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4521 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4521[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5298
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5298)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5298->$0 = _M0L6_2atmpS4521;
      _block_5298->$1 = 1;
      return _block_5298;
    } else {
      moonbit_string_t _M0L1vS2259;
      moonbit_string_t _M0L6_2atmpS4525;
      moonbit_string_t _M0L7_2abindS2261;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2260;
      moonbit_string_t _M0L6_2atmpS4523;
      moonbit_string_t* _M0L6_2atmpS4522;
      struct _M0TPB5ArrayGsE* _block_5301;
      #line 243 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4525 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 243 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2261
      = _M0MP19moonbitDB8Database4rpop(_M0L2dbS2196, _M0L6_2atmpS4525);
      moonbit_decref(_M0L6_2atmpS4525);
      if (_M0L7_2abindS2261 == 0) {
        moonbit_string_t* _M0L6_2atmpS4524;
        struct _M0TPB5ArrayGsE* _block_5300;
        if (_M0L7_2abindS2261) {
          moonbit_decref(_M0L7_2abindS2261);
        }
        _M0L6_2atmpS4524 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        _M0L6_2atmpS4524[0]
        = (moonbit_string_t)moonbit_string_literal_38.data;
        _block_5300
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5300)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5300->$0 = _M0L6_2atmpS4524;
        _block_5300->$1 = 1;
        return _block_5300;
      } else {
        moonbit_string_t _M0L7_2aSomeS2262 = _M0L7_2abindS2261;
        moonbit_string_t _M0L4_2avS2263 = _M0L7_2aSomeS2262;
        _M0L1vS2259 = _M0L4_2avS2263;
        goto join_2258;
      }
      join_2258:;
      #line 244 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2260
      = _M0MPB13StringBuilder21StringBuilder_2einner(2);
      #line 244 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2260, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 244 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2260, _M0L1vS2259);
      moonbit_decref(_M0L1vS2259);
      #line 244 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2260, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 244 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4523
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2260);
      moonbit_decref(_M0L18_2astring__builderS2260);
      _M0L6_2atmpS4522 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4522[0] = _M0L6_2atmpS4523;
      _block_5301
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5301)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5301->$0 = _M0L6_2atmpS4522;
      _block_5301->$1 = 1;
      return _block_5301;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_51.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_51.data, 8)
         ) {
    int32_t _M0L6_2atmpS4526;
    moonbit_decref(_M0L7commandS2195);
    #line 250 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4526 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4526 < 2) {
      moonbit_string_t* _M0L6_2atmpS4527;
      struct _M0TPB5ArrayGsE* _block_5296;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4527 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4527[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5296
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5296)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5296->$0 = _M0L6_2atmpS4527;
      _block_5296->$1 = 1;
      return _block_5296;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2264;
      moonbit_string_t _M0L6_2atmpS4531;
      int32_t _M0L6_2atmpS4530;
      moonbit_string_t _M0L6_2atmpS4529;
      moonbit_string_t* _M0L6_2atmpS4528;
      struct _M0TPB5ArrayGsE* _block_5297;
      #line 251 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2264
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 251 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2264, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 251 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4531 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 251 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4530
      = _M0MP19moonbitDB8Database4llen(_M0L2dbS2196, _M0L6_2atmpS4531);
      moonbit_decref(_M0L6_2atmpS4531);
      #line 251 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2264, _M0L6_2atmpS4530);
      #line 251 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4529
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2264);
      moonbit_decref(_M0L18_2astring__builderS2264);
      _M0L6_2atmpS4528 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4528[0] = _M0L6_2atmpS4529;
      _block_5297
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5297)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5297->$0 = _M0L6_2atmpS4528;
      _block_5297->$1 = 1;
      return _block_5297;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_50.data
           || Moonbit_array_length(_M0L7commandS2195) == 6
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_50.data, 12)
         ) {
    int32_t _M0L6_2atmpS4532;
    moonbit_decref(_M0L7commandS2195);
    #line 254 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4532 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4532 < 4) {
      moonbit_string_t* _M0L6_2atmpS4533;
      struct _M0TPB5ArrayGsE* _block_5291;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4533 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4533[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5291
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5291)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5291->$0 = _M0L6_2atmpS4533;
      _block_5291->$1 = 1;
      return _block_5291;
    } else {
      int32_t _M0L2svS2267;
      int32_t _M0L2evS2268;
      moonbit_string_t _M0L6_2atmpS4543;
      int64_t _M0L7_2abindS2274;
      moonbit_string_t _M0L6_2atmpS4542;
      int64_t _M0L7_2abindS2275;
      moonbit_string_t _M0L6_2atmpS4541;
      struct _M0TPB5ArrayGsE* _M0L5itemsS2269;
      moonbit_string_t* _M0L6_2atmpS4540;
      struct _M0TPB5ArrayGsE* _M0L6resultS2270;
      int32_t _M0L1iS2271;
      moonbit_string_t* _M0L6_2atmpS4534;
      struct _M0TPB5ArrayGsE* _block_5295;
      #line 256 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4543 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      #line 256 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2274
      = _M0FP39moonbitDB8examples9cli__repl10parse__int(_M0L6_2atmpS4543);
      moonbit_decref(_M0L6_2atmpS4543);
      #line 256 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4542 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 3);
      #line 256 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2275
      = _M0FP39moonbitDB8examples9cli__repl10parse__int(_M0L6_2atmpS4542);
      moonbit_decref(_M0L6_2atmpS4542);
      if (_M0L7_2abindS2274 == 4294967296ll) {
        moonbit_decref(_M0L5partsS2193);
        goto join_2265;
      } else {
        int64_t _M0L7_2aSomeS2276 = _M0L7_2abindS2274;
        int32_t _M0L5_2asvS2277 = (int32_t)_M0L7_2aSomeS2276;
        if (_M0L7_2abindS2275 == 4294967296ll) {
          moonbit_decref(_M0L5partsS2193);
          goto join_2265;
        } else {
          int64_t _M0L7_2aSomeS2278 = _M0L7_2abindS2275;
          int32_t _M0L5_2aevS2279 = (int32_t)_M0L7_2aSomeS2278;
          _M0L2svS2267 = _M0L5_2asvS2277;
          _M0L2evS2268 = _M0L5_2aevS2279;
          goto join_2266;
        }
      }
      join_2266:;
      #line 258 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4541 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 258 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L5itemsS2269
      = _M0MP19moonbitDB8Database6lrange(_M0L2dbS2196, _M0L6_2atmpS4541, _M0L2svS2267, _M0L2evS2268);
      moonbit_decref(_M0L6_2atmpS4541);
      _M0L6_2atmpS4540 = (moonbit_string_t*)moonbit_empty_ref_array;
      _M0L6resultS2270
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_M0L6resultS2270)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _M0L6resultS2270->$0 = _M0L6_2atmpS4540;
      _M0L6resultS2270->$1 = 0;
      _M0L1iS2271 = 0;
      while (1) {
        int32_t _M0L6_2atmpS4535;
        #line 260 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4535 = _M0MPC15array5Array6lengthGsE(_M0L5itemsS2269);
        if (_M0L1iS2271 < _M0L6_2atmpS4535) {
          struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2272;
          int32_t _M0L6_2atmpS4537;
          moonbit_string_t _M0L6_2atmpS4538;
          moonbit_string_t _M0L6_2atmpS4536;
          int32_t _M0L6_2atmpS4539;
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L18_2astring__builderS2272
          = _M0MPB13StringBuilder21StringBuilder_2einner(6);
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2272, (moonbit_string_t)moonbit_string_literal_23.data);
          _M0L6_2atmpS4537 = _M0L1iS2271 + 1;
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2272, _M0L6_2atmpS4537);
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2272, (moonbit_string_t)moonbit_string_literal_32.data);
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS4538
          = _M0MPC15array5Array2atGsE(_M0L5itemsS2269, _M0L1iS2271);
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2272, _M0L6_2atmpS4538);
          moonbit_decref(_M0L6_2atmpS4538);
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2272, (moonbit_string_t)moonbit_string_literal_33.data);
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS4536
          = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2272);
          moonbit_decref(_M0L18_2astring__builderS2272);
          #line 261 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPC15array5Array4pushGsE(_M0L6resultS2270, _M0L6_2atmpS4536);
          moonbit_decref(_M0L6_2atmpS4536);
          _M0L6_2atmpS4539 = _M0L1iS2271 + 1;
          _M0L1iS2271 = _M0L6_2atmpS4539;
          continue;
        } else {
          moonbit_decref(_M0L5itemsS2269);
        }
        break;
      }
      return _M0L6resultS2270;
      join_2265:;
      _M0L6_2atmpS4534 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4534[0] = (moonbit_string_t)moonbit_string_literal_15.data;
      _block_5295
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5295)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5295->$0 = _M0L6_2atmpS4534;
      _block_5295->$1 = 1;
      return _block_5295;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_49.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_49.data, 8)
         ) {
    int32_t _M0L6_2atmpS4544;
    moonbit_decref(_M0L7commandS2195);
    #line 270 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4544 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4544 < 3) {
      moonbit_string_t* _M0L6_2atmpS4545;
      struct _M0TPB5ArrayGsE* _block_5287;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4545 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4545[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5287
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5287)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5287->$0 = _M0L6_2atmpS4545;
      _block_5287->$1 = 1;
      return _block_5287;
    } else {
      moonbit_string_t _M0L6_2atmpS4546;
      moonbit_string_t _M0L6_2atmpS4547;
      int32_t _result_5288;
      #line 271 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4546 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      #line 271 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4547 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      moonbit_decref(_M0L5partsS2193);
      #line 271 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5288
      = _M0MP19moonbitDB8Database4sadd(_M0L2dbS2196, _M0L6_2atmpS4546, _M0L6_2atmpS4547);
      moonbit_decref(_M0L6_2atmpS4546);
      moonbit_decref(_M0L6_2atmpS4547);
      if (_result_5288) {
        moonbit_string_t* _M0L6_2atmpS4548 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5289;
        _M0L6_2atmpS4548[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5289
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5289)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5289->$0 = _M0L6_2atmpS4548;
        _block_5289->$1 = 1;
        return _block_5289;
      } else {
        moonbit_string_t* _M0L6_2atmpS4549 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5290;
        _M0L6_2atmpS4549[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5290
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5290)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5290->$0 = _M0L6_2atmpS4549;
        _block_5290->$1 = 1;
        return _block_5290;
      }
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_48.data
           || Moonbit_array_length(_M0L7commandS2195) == 8
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_48.data, 16)
         ) {
    int32_t _M0L6_2atmpS4550;
    moonbit_decref(_M0L7commandS2195);
    #line 274 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4550 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4550 < 2) {
      moonbit_string_t* _M0L6_2atmpS4551;
      struct _M0TPB5ArrayGsE* _block_5285;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4551 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4551[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5285
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5285)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5285->$0 = _M0L6_2atmpS4551;
      _block_5285->$1 = 1;
      return _block_5285;
    } else {
      moonbit_string_t _M0L6_2atmpS4558;
      struct _M0TPB5ArrayGsE* _M0L7membersS2280;
      moonbit_string_t* _M0L6_2atmpS4557;
      struct _M0TPB5ArrayGsE* _M0L6resultS2281;
      int32_t _M0L1iS2282;
      #line 276 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4558 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 276 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7membersS2280
      = _M0MP19moonbitDB8Database8smembers(_M0L2dbS2196, _M0L6_2atmpS4558);
      moonbit_decref(_M0L6_2atmpS4558);
      _M0L6_2atmpS4557 = (moonbit_string_t*)moonbit_empty_ref_array;
      _M0L6resultS2281
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_M0L6resultS2281)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _M0L6resultS2281->$0 = _M0L6_2atmpS4557;
      _M0L6resultS2281->$1 = 0;
      _M0L1iS2282 = 0;
      while (1) {
        int32_t _M0L6_2atmpS4552;
        #line 278 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4552 = _M0MPC15array5Array6lengthGsE(_M0L7membersS2280);
        if (_M0L1iS2282 < _M0L6_2atmpS4552) {
          struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2283;
          int32_t _M0L6_2atmpS4554;
          moonbit_string_t _M0L6_2atmpS4555;
          moonbit_string_t _M0L6_2atmpS4553;
          int32_t _M0L6_2atmpS4556;
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L18_2astring__builderS2283
          = _M0MPB13StringBuilder21StringBuilder_2einner(6);
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2283, (moonbit_string_t)moonbit_string_literal_23.data);
          _M0L6_2atmpS4554 = _M0L1iS2282 + 1;
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2283, _M0L6_2atmpS4554);
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2283, (moonbit_string_t)moonbit_string_literal_32.data);
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS4555
          = _M0MPC15array5Array2atGsE(_M0L7membersS2280, _M0L1iS2282);
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2283, _M0L6_2atmpS4555);
          moonbit_decref(_M0L6_2atmpS4555);
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2283, (moonbit_string_t)moonbit_string_literal_33.data);
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS4553
          = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2283);
          moonbit_decref(_M0L18_2astring__builderS2283);
          #line 279 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPC15array5Array4pushGsE(_M0L6resultS2281, _M0L6_2atmpS4553);
          moonbit_decref(_M0L6_2atmpS4553);
          _M0L6_2atmpS4556 = _M0L1iS2282 + 1;
          _M0L1iS2282 = _M0L6_2atmpS4556;
          continue;
        } else {
          moonbit_decref(_M0L7membersS2280);
        }
        break;
      }
      return _M0L6resultS2281;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_47.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_47.data, 8)
         ) {
    int32_t _M0L6_2atmpS4559;
    moonbit_decref(_M0L7commandS2195);
    #line 285 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4559 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4559 < 3) {
      moonbit_string_t* _M0L6_2atmpS4560;
      struct _M0TPB5ArrayGsE* _block_5281;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4560 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4560[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5281
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5281)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5281->$0 = _M0L6_2atmpS4560;
      _block_5281->$1 = 1;
      return _block_5281;
    } else {
      moonbit_string_t _M0L6_2atmpS4561;
      moonbit_string_t _M0L6_2atmpS4562;
      int32_t _result_5282;
      #line 286 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4561 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      #line 286 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4562 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      moonbit_decref(_M0L5partsS2193);
      #line 286 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5282
      = _M0MP19moonbitDB8Database4srem(_M0L2dbS2196, _M0L6_2atmpS4561, _M0L6_2atmpS4562);
      moonbit_decref(_M0L6_2atmpS4561);
      moonbit_decref(_M0L6_2atmpS4562);
      if (_result_5282) {
        moonbit_string_t* _M0L6_2atmpS4563 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5283;
        _M0L6_2atmpS4563[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5283
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5283)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5283->$0 = _M0L6_2atmpS4563;
        _block_5283->$1 = 1;
        return _block_5283;
      } else {
        moonbit_string_t* _M0L6_2atmpS4564 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5284;
        _M0L6_2atmpS4564[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5284
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5284)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5284->$0 = _M0L6_2atmpS4564;
        _block_5284->$1 = 1;
        return _block_5284;
      }
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_46.data
           || Moonbit_array_length(_M0L7commandS2195) == 5
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_46.data, 10)
         ) {
    int32_t _M0L6_2atmpS4565;
    moonbit_decref(_M0L7commandS2195);
    #line 289 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4565 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4565 < 2) {
      moonbit_string_t* _M0L6_2atmpS4566;
      struct _M0TPB5ArrayGsE* _block_5279;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4566 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4566[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5279
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5279)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5279->$0 = _M0L6_2atmpS4566;
      _block_5279->$1 = 1;
      return _block_5279;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2285;
      moonbit_string_t _M0L6_2atmpS4570;
      int32_t _M0L6_2atmpS4569;
      moonbit_string_t _M0L6_2atmpS4568;
      moonbit_string_t* _M0L6_2atmpS4567;
      struct _M0TPB5ArrayGsE* _block_5280;
      #line 290 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2285
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 290 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2285, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 290 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4570 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 290 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4569
      = _M0MP19moonbitDB8Database5scard(_M0L2dbS2196, _M0L6_2atmpS4570);
      moonbit_decref(_M0L6_2atmpS4570);
      #line 290 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2285, _M0L6_2atmpS4569);
      #line 290 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4568
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2285);
      moonbit_decref(_M0L18_2astring__builderS2285);
      _M0L6_2atmpS4567 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4567[0] = _M0L6_2atmpS4568;
      _block_5280
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5280)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5280->$0 = _M0L6_2atmpS4567;
      _block_5280->$1 = 1;
      return _block_5280;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_45.data
           || Moonbit_array_length(_M0L7commandS2195) == 9
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_45.data, 18)
         ) {
    int32_t _M0L6_2atmpS4571;
    moonbit_decref(_M0L7commandS2195);
    #line 293 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4571 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4571 < 3) {
      moonbit_string_t* _M0L6_2atmpS4572;
      struct _M0TPB5ArrayGsE* _block_5275;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4572 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4572[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5275
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5275)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5275->$0 = _M0L6_2atmpS4572;
      _block_5275->$1 = 1;
      return _block_5275;
    } else {
      moonbit_string_t _M0L6_2atmpS4573;
      moonbit_string_t _M0L6_2atmpS4574;
      int32_t _result_5276;
      #line 294 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4573 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      #line 294 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4574 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      moonbit_decref(_M0L5partsS2193);
      #line 294 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5276
      = _M0MP19moonbitDB8Database9sismember(_M0L2dbS2196, _M0L6_2atmpS4573, _M0L6_2atmpS4574);
      moonbit_decref(_M0L6_2atmpS4573);
      moonbit_decref(_M0L6_2atmpS4574);
      if (_result_5276) {
        moonbit_string_t* _M0L6_2atmpS4575 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5277;
        _M0L6_2atmpS4575[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5277
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5277)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5277->$0 = _M0L6_2atmpS4575;
        _block_5277->$1 = 1;
        return _block_5277;
      } else {
        moonbit_string_t* _M0L6_2atmpS4576 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5278;
        _M0L6_2atmpS4576[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5278
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5278)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5278->$0 = _M0L6_2atmpS4576;
        _block_5278->$1 = 1;
        return _block_5278;
      }
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_42.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_42.data, 8)
         ) {
    int32_t _M0L6_2atmpS4577;
    moonbit_decref(_M0L7commandS2195);
    #line 297 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4577 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4577 < 4) {
      moonbit_string_t* _M0L6_2atmpS4578;
      struct _M0TPB5ArrayGsE* _block_5269;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4578 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4578[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5269
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5269)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5269->$0 = _M0L6_2atmpS4578;
      _block_5269->$1 = 1;
      return _block_5269;
    } else {
      float _M0L1sS2287;
      moonbit_string_t _M0L6_2atmpS4584;
      void* _M0L7_2abindS2288;
      moonbit_string_t _M0L6_2atmpS4579;
      moonbit_string_t _M0L6_2atmpS4580;
      int32_t _result_5272;
      #line 299 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4584 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      #line 299 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2288
      = _M0FP39moonbitDB8examples9cli__repl12parse__float(_M0L6_2atmpS4584);
      moonbit_decref(_M0L6_2atmpS4584);
      switch (Moonbit_object_tag(_M0L7_2abindS2288)) {
        case 1: {
          struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS2289 =
            (struct _M0DTPC16option6OptionGfE4Some*)_M0L7_2abindS2288;
          float _M0L4_2asS2290 = _M0L7_2aSomeS2289->$0;
          moonbit_decref(_M0L7_2aSomeS2289);
          _M0L1sS2287 = _M0L4_2asS2290;
          goto join_2286;
          break;
        }
        default: {
          moonbit_string_t* _M0L6_2atmpS4583;
          struct _M0TPB5ArrayGsE* _block_5271;
          moonbit_decref(_M0L7_2abindS2288);
          moonbit_decref(_M0L5partsS2193);
          _M0L6_2atmpS4583 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
          _M0L6_2atmpS4583[0]
          = (moonbit_string_t)moonbit_string_literal_36.data;
          _block_5271
          = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
          Moonbit_object_header(_block_5271)->meta
          = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
          _block_5271->$0 = _M0L6_2atmpS4583;
          _block_5271->$1 = 1;
          return _block_5271;
          break;
        }
      }
      join_2286:;
      #line 300 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4579 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      #line 300 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4580 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 3);
      moonbit_decref(_M0L5partsS2193);
      #line 300 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _result_5272
      = _M0MP19moonbitDB8Database4zadd(_M0L2dbS2196, _M0L6_2atmpS4579, _M0L1sS2287, _M0L6_2atmpS4580);
      moonbit_decref(_M0L6_2atmpS4579);
      moonbit_decref(_M0L6_2atmpS4580);
      if (_result_5272) {
        moonbit_string_t* _M0L6_2atmpS4581 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5273;
        _M0L6_2atmpS4581[0]
        = (moonbit_string_t)moonbit_string_literal_43.data;
        _block_5273
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5273)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5273->$0 = _M0L6_2atmpS4581;
        _block_5273->$1 = 1;
        return _block_5273;
      } else {
        moonbit_string_t* _M0L6_2atmpS4582 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5274;
        _M0L6_2atmpS4582[0]
        = (moonbit_string_t)moonbit_string_literal_44.data;
        _block_5274
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5274)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5274->$0 = _M0L6_2atmpS4582;
        _block_5274->$1 = 1;
        return _block_5274;
      }
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_41.data
           || Moonbit_array_length(_M0L7commandS2195) == 6
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_41.data, 12)
         ) {
    int32_t _M0L6_2atmpS4585;
    moonbit_decref(_M0L7commandS2195);
    #line 306 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4585 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4585 < 4) {
      moonbit_string_t* _M0L6_2atmpS4586;
      struct _M0TPB5ArrayGsE* _block_5264;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4586 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4586[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5264
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5264)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5264->$0 = _M0L6_2atmpS4586;
      _block_5264->$1 = 1;
      return _block_5264;
    } else {
      int32_t _M0L2svS2293;
      int32_t _M0L2evS2294;
      moonbit_string_t _M0L6_2atmpS4596;
      int64_t _M0L7_2abindS2300;
      moonbit_string_t _M0L6_2atmpS4595;
      int64_t _M0L7_2abindS2301;
      moonbit_string_t _M0L6_2atmpS4594;
      struct _M0TPB5ArrayGsE* _M0L5itemsS2295;
      moonbit_string_t* _M0L6_2atmpS4593;
      struct _M0TPB5ArrayGsE* _M0L6resultS2296;
      int32_t _M0L1iS2297;
      moonbit_string_t* _M0L6_2atmpS4587;
      struct _M0TPB5ArrayGsE* _block_5268;
      #line 308 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4596 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      #line 308 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2300
      = _M0FP39moonbitDB8examples9cli__repl10parse__int(_M0L6_2atmpS4596);
      moonbit_decref(_M0L6_2atmpS4596);
      #line 308 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4595 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 3);
      #line 308 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2301
      = _M0FP39moonbitDB8examples9cli__repl10parse__int(_M0L6_2atmpS4595);
      moonbit_decref(_M0L6_2atmpS4595);
      if (_M0L7_2abindS2300 == 4294967296ll) {
        moonbit_decref(_M0L5partsS2193);
        goto join_2291;
      } else {
        int64_t _M0L7_2aSomeS2302 = _M0L7_2abindS2300;
        int32_t _M0L5_2asvS2303 = (int32_t)_M0L7_2aSomeS2302;
        if (_M0L7_2abindS2301 == 4294967296ll) {
          moonbit_decref(_M0L5partsS2193);
          goto join_2291;
        } else {
          int64_t _M0L7_2aSomeS2304 = _M0L7_2abindS2301;
          int32_t _M0L5_2aevS2305 = (int32_t)_M0L7_2aSomeS2304;
          _M0L2svS2293 = _M0L5_2asvS2303;
          _M0L2evS2294 = _M0L5_2aevS2305;
          goto join_2292;
        }
      }
      join_2292:;
      #line 310 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4594 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 310 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L5itemsS2295
      = _M0MP19moonbitDB8Database6zrange(_M0L2dbS2196, _M0L6_2atmpS4594, _M0L2svS2293, _M0L2evS2294);
      moonbit_decref(_M0L6_2atmpS4594);
      _M0L6_2atmpS4593 = (moonbit_string_t*)moonbit_empty_ref_array;
      _M0L6resultS2296
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_M0L6resultS2296)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _M0L6resultS2296->$0 = _M0L6_2atmpS4593;
      _M0L6resultS2296->$1 = 0;
      _M0L1iS2297 = 0;
      while (1) {
        int32_t _M0L6_2atmpS4588;
        #line 312 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4588 = _M0MPC15array5Array6lengthGsE(_M0L5itemsS2295);
        if (_M0L1iS2297 < _M0L6_2atmpS4588) {
          struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2298;
          int32_t _M0L6_2atmpS4590;
          moonbit_string_t _M0L6_2atmpS4591;
          moonbit_string_t _M0L6_2atmpS4589;
          int32_t _M0L6_2atmpS4592;
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L18_2astring__builderS2298
          = _M0MPB13StringBuilder21StringBuilder_2einner(6);
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2298, (moonbit_string_t)moonbit_string_literal_23.data);
          _M0L6_2atmpS4590 = _M0L1iS2297 + 1;
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2298, _M0L6_2atmpS4590);
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2298, (moonbit_string_t)moonbit_string_literal_32.data);
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS4591
          = _M0MPC15array5Array2atGsE(_M0L5itemsS2295, _M0L1iS2297);
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2298, _M0L6_2atmpS4591);
          moonbit_decref(_M0L6_2atmpS4591);
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2298, (moonbit_string_t)moonbit_string_literal_33.data);
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS4589
          = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2298);
          moonbit_decref(_M0L18_2astring__builderS2298);
          #line 313 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPC15array5Array4pushGsE(_M0L6resultS2296, _M0L6_2atmpS4589);
          moonbit_decref(_M0L6_2atmpS4589);
          _M0L6_2atmpS4592 = _M0L1iS2297 + 1;
          _M0L1iS2297 = _M0L6_2atmpS4592;
          continue;
        } else {
          moonbit_decref(_M0L5itemsS2295);
        }
        break;
      }
      return _M0L6resultS2296;
      join_2291:;
      _M0L6_2atmpS4587 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4587[0] = (moonbit_string_t)moonbit_string_literal_15.data;
      _block_5268
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5268)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5268->$0 = _M0L6_2atmpS4587;
      _block_5268->$1 = 1;
      return _block_5268;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_40.data
           || Moonbit_array_length(_M0L7commandS2195) == 5
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_40.data, 10)
         ) {
    int32_t _M0L6_2atmpS4597;
    moonbit_decref(_M0L7commandS2195);
    #line 322 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4597 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4597 < 2) {
      moonbit_string_t* _M0L6_2atmpS4598;
      struct _M0TPB5ArrayGsE* _block_5262;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4598 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4598[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5262
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5262)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5262->$0 = _M0L6_2atmpS4598;
      _block_5262->$1 = 1;
      return _block_5262;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2306;
      moonbit_string_t _M0L6_2atmpS4602;
      int32_t _M0L6_2atmpS4601;
      moonbit_string_t _M0L6_2atmpS4600;
      moonbit_string_t* _M0L6_2atmpS4599;
      struct _M0TPB5ArrayGsE* _block_5263;
      #line 323 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2306
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 323 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2306, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 323 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4602 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 323 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4601
      = _M0MP19moonbitDB8Database5zcard(_M0L2dbS2196, _M0L6_2atmpS4602);
      moonbit_decref(_M0L6_2atmpS4602);
      #line 323 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2306, _M0L6_2atmpS4601);
      #line 323 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4600
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2306);
      moonbit_decref(_M0L18_2astring__builderS2306);
      _M0L6_2atmpS4599 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4599[0] = _M0L6_2atmpS4600;
      _block_5263
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5263)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5263->$0 = _M0L6_2atmpS4599;
      _block_5263->$1 = 1;
      return _block_5263;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_39.data
           || Moonbit_array_length(_M0L7commandS2195) == 6
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_39.data, 12)
         ) {
    int32_t _M0L6_2atmpS4603;
    moonbit_decref(_M0L7commandS2195);
    #line 326 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4603 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4603 < 3) {
      moonbit_string_t* _M0L6_2atmpS4604;
      struct _M0TPB5ArrayGsE* _block_5258;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4604 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4604[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5258
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5258)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5258->$0 = _M0L6_2atmpS4604;
      _block_5258->$1 = 1;
      return _block_5258;
    } else {
      float _M0L1vS2308;
      moonbit_string_t _M0L6_2atmpS4608;
      moonbit_string_t _M0L6_2atmpS4609;
      void* _M0L7_2abindS2310;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2309;
      moonbit_string_t _M0L6_2atmpS4606;
      moonbit_string_t* _M0L6_2atmpS4605;
      struct _M0TPB5ArrayGsE* _block_5261;
      #line 328 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4608 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      #line 328 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4609 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      moonbit_decref(_M0L5partsS2193);
      #line 328 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2310
      = _M0MP19moonbitDB8Database6zscore(_M0L2dbS2196, _M0L6_2atmpS4608, _M0L6_2atmpS4609);
      moonbit_decref(_M0L6_2atmpS4608);
      moonbit_decref(_M0L6_2atmpS4609);
      switch (Moonbit_object_tag(_M0L7_2abindS2310)) {
        case 1: {
          struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS2311 =
            (struct _M0DTPC16option6OptionGfE4Some*)_M0L7_2abindS2310;
          float _M0L4_2avS2312 = _M0L7_2aSomeS2311->$0;
          moonbit_decref(_M0L7_2aSomeS2311);
          _M0L1vS2308 = _M0L4_2avS2312;
          goto join_2307;
          break;
        }
        default: {
          moonbit_string_t* _M0L6_2atmpS4607;
          struct _M0TPB5ArrayGsE* _block_5260;
          moonbit_decref(_M0L7_2abindS2310);
          _M0L6_2atmpS4607 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
          _M0L6_2atmpS4607[0]
          = (moonbit_string_t)moonbit_string_literal_38.data;
          _block_5260
          = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
          Moonbit_object_header(_block_5260)->meta
          = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
          _block_5260->$0 = _M0L6_2atmpS4607;
          _block_5260->$1 = 1;
          return _block_5260;
          break;
        }
      }
      join_2307:;
      #line 329 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2309
      = _M0MPB13StringBuilder21StringBuilder_2einner(2);
      #line 329 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2309, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 329 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS2309, _M0L1vS2308);
      #line 329 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2309, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 329 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4606
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2309);
      moonbit_decref(_M0L18_2astring__builderS2309);
      _M0L6_2atmpS4605 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4605[0] = _M0L6_2atmpS4606;
      _block_5261
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5261)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5261->$0 = _M0L6_2atmpS4605;
      _block_5261->$1 = 1;
      return _block_5261;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_37.data
           || Moonbit_array_length(_M0L7commandS2195) == 5
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_37.data, 10)
         ) {
    int32_t _M0L6_2atmpS4610;
    moonbit_decref(_M0L7commandS2195);
    #line 335 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4610 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4610 < 3) {
      moonbit_string_t* _M0L6_2atmpS4611;
      struct _M0TPB5ArrayGsE* _block_5254;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4611 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4611[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5254
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5254)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5254->$0 = _M0L6_2atmpS4611;
      _block_5254->$1 = 1;
      return _block_5254;
    } else {
      int32_t _M0L1vS2314;
      moonbit_string_t _M0L6_2atmpS4615;
      moonbit_string_t _M0L6_2atmpS4616;
      int64_t _M0L7_2abindS2316;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2315;
      moonbit_string_t _M0L6_2atmpS4613;
      moonbit_string_t* _M0L6_2atmpS4612;
      struct _M0TPB5ArrayGsE* _block_5257;
      #line 337 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4615 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      #line 337 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4616 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      moonbit_decref(_M0L5partsS2193);
      #line 337 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2316
      = _M0MP19moonbitDB8Database5zrank(_M0L2dbS2196, _M0L6_2atmpS4615, _M0L6_2atmpS4616);
      moonbit_decref(_M0L6_2atmpS4615);
      moonbit_decref(_M0L6_2atmpS4616);
      if (_M0L7_2abindS2316 == 4294967296ll) {
        moonbit_string_t* _M0L6_2atmpS4614 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5256;
        _M0L6_2atmpS4614[0]
        = (moonbit_string_t)moonbit_string_literal_38.data;
        _block_5256
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5256)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5256->$0 = _M0L6_2atmpS4614;
        _block_5256->$1 = 1;
        return _block_5256;
      } else {
        int64_t _M0L7_2aSomeS2317 = _M0L7_2abindS2316;
        int32_t _M0L4_2avS2318 = (int32_t)_M0L7_2aSomeS2317;
        _M0L1vS2314 = _M0L4_2avS2318;
        goto join_2313;
      }
      join_2313:;
      #line 338 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2315
      = _M0MPB13StringBuilder21StringBuilder_2einner(10);
      #line 338 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2315, (moonbit_string_t)moonbit_string_literal_28.data);
      #line 338 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2315, _M0L1vS2314);
      #line 338 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4613
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2315);
      moonbit_decref(_M0L18_2astring__builderS2315);
      _M0L6_2atmpS4612 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4612[0] = _M0L6_2atmpS4613;
      _block_5257
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5257)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5257->$0 = _M0L6_2atmpS4612;
      _block_5257->$1 = 1;
      return _block_5257;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_35.data
           || Moonbit_array_length(_M0L7commandS2195) == 7
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_35.data, 14)
         ) {
    int32_t _M0L6_2atmpS4617;
    moonbit_decref(_M0L7commandS2195);
    #line 344 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4617 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4617 < 4) {
      moonbit_string_t* _M0L6_2atmpS4618;
      struct _M0TPB5ArrayGsE* _block_5250;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4618 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4618[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5250
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5250)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5250->$0 = _M0L6_2atmpS4618;
      _block_5250->$1 = 1;
      return _block_5250;
    } else {
      float _M0L3incS2320;
      moonbit_string_t _M0L6_2atmpS4624;
      void* _M0L7_2abindS2323;
      moonbit_string_t _M0L6_2atmpS4621;
      moonbit_string_t _M0L6_2atmpS4622;
      float _M0L10new__scoreS2321;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2322;
      moonbit_string_t _M0L6_2atmpS4620;
      moonbit_string_t* _M0L6_2atmpS4619;
      struct _M0TPB5ArrayGsE* _block_5253;
      #line 346 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4624 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 2);
      #line 346 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2323
      = _M0FP39moonbitDB8examples9cli__repl12parse__float(_M0L6_2atmpS4624);
      moonbit_decref(_M0L6_2atmpS4624);
      switch (Moonbit_object_tag(_M0L7_2abindS2323)) {
        case 1: {
          struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS2324 =
            (struct _M0DTPC16option6OptionGfE4Some*)_M0L7_2abindS2323;
          float _M0L6_2aincS2325 = _M0L7_2aSomeS2324->$0;
          moonbit_decref(_M0L7_2aSomeS2324);
          _M0L3incS2320 = _M0L6_2aincS2325;
          goto join_2319;
          break;
        }
        default: {
          moonbit_string_t* _M0L6_2atmpS4623;
          struct _M0TPB5ArrayGsE* _block_5252;
          moonbit_decref(_M0L7_2abindS2323);
          moonbit_decref(_M0L5partsS2193);
          _M0L6_2atmpS4623 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
          _M0L6_2atmpS4623[0]
          = (moonbit_string_t)moonbit_string_literal_36.data;
          _block_5252
          = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
          Moonbit_object_header(_block_5252)->meta
          = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
          _block_5252->$0 = _M0L6_2atmpS4623;
          _block_5252->$1 = 1;
          return _block_5252;
          break;
        }
      }
      join_2319:;
      #line 348 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4621 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      #line 348 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4622 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 3);
      moonbit_decref(_M0L5partsS2193);
      #line 348 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L10new__scoreS2321
      = _M0MP19moonbitDB8Database7zincrby(_M0L2dbS2196, _M0L6_2atmpS4621, _M0L3incS2320, _M0L6_2atmpS4622);
      moonbit_decref(_M0L6_2atmpS4621);
      moonbit_decref(_M0L6_2atmpS4622);
      #line 349 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2322
      = _M0MPB13StringBuilder21StringBuilder_2einner(2);
      #line 349 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2322, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 349 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS2322, _M0L10new__scoreS2321);
      #line 349 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2322, (moonbit_string_t)moonbit_string_literal_33.data);
      #line 349 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4620
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2322);
      moonbit_decref(_M0L18_2astring__builderS2322);
      _M0L6_2atmpS4619 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4619[0] = _M0L6_2atmpS4620;
      _block_5253
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5253)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5253->$0 = _M0L6_2atmpS4619;
      _block_5253->$1 = 1;
      return _block_5253;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_34.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_34.data, 8)
         ) {
    moonbit_string_t* _M0L6_2atmpS4637;
    struct _M0TPB5ArrayGsE* _M0L4keysS2326;
    moonbit_string_t* _M0L6_2atmpS4636;
    struct _M0TPB5ArrayGsE* _M0L4valsS2327;
    struct _M0TPB8MutLocalGiE* _M0L1iS2328;
    moonbit_string_t* _M0L6_2atmpS4635;
    struct _M0TPB5ArrayGsE* _block_5249;
    moonbit_decref(_M0L7commandS2195);
    _M0L6_2atmpS4637 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L4keysS2326
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L4keysS2326)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L4keysS2326->$0 = _M0L6_2atmpS4637;
    _M0L4keysS2326->$1 = 0;
    _M0L6_2atmpS4636 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L4valsS2327
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L4valsS2327)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L4valsS2327->$0 = _M0L6_2atmpS4636;
    _M0L4valsS2327->$1 = 0;
    _M0L1iS2328
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L1iS2328)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L1iS2328->$0 = 1;
    while (1) {
      int32_t _M0L3valS4627 = _M0L1iS2328->$0;
      int32_t _M0L6_2atmpS4625 = _M0L3valS4627 + 1;
      int32_t _M0L6_2atmpS4626;
      #line 359 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4626 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
      if (_M0L6_2atmpS4625 < _M0L6_2atmpS4626) {
        int32_t _M0L3valS4629 = _M0L1iS2328->$0;
        moonbit_string_t _M0L6_2atmpS4628;
        int32_t _M0L3valS4632;
        int32_t _M0L6_2atmpS4631;
        moonbit_string_t _M0L6_2atmpS4630;
        int32_t _M0L3valS4634;
        int32_t _M0L6_2atmpS4633;
        #line 360 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4628
        = _M0MPC15array5Array2atGsE(_M0L5partsS2193, _M0L3valS4629);
        #line 360 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L4keysS2326, _M0L6_2atmpS4628);
        moonbit_decref(_M0L6_2atmpS4628);
        _M0L3valS4632 = _M0L1iS2328->$0;
        _M0L6_2atmpS4631 = _M0L3valS4632 + 1;
        #line 361 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4630
        = _M0MPC15array5Array2atGsE(_M0L5partsS2193, _M0L6_2atmpS4631);
        #line 361 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L4valsS2327, _M0L6_2atmpS4630);
        moonbit_decref(_M0L6_2atmpS4630);
        _M0L3valS4634 = _M0L1iS2328->$0;
        _M0L6_2atmpS4633 = _M0L3valS4634 + 2;
        _M0L1iS2328->$0 = _M0L6_2atmpS4633;
        continue;
      } else {
        moonbit_decref(_M0L1iS2328);
        moonbit_decref(_M0L5partsS2193);
      }
      break;
    }
    #line 364 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0MP19moonbitDB8Database4mset(_M0L2dbS2196, _M0L4keysS2326, _M0L4valsS2327);
    moonbit_decref(_M0L4keysS2326);
    moonbit_decref(_M0L4valsS2327);
    _M0L6_2atmpS4635 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS4635[0] = (moonbit_string_t)moonbit_string_literal_26.data;
    _block_5249
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5249)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5249->$0 = _M0L6_2atmpS4635;
    _block_5249->$1 = 1;
    return _block_5249;
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_30.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_30.data, 8)
         ) {
    moonbit_string_t* _M0L6_2atmpS4648;
    struct _M0TPB5ArrayGsE* _M0L4keysS2330;
    int32_t _M0L1iS2331;
    struct _M0TPB5ArrayGOsE* _M0L4valsS2333;
    moonbit_string_t* _M0L6_2atmpS4647;
    struct _M0TPB5ArrayGsE* _M0L6resultS2334;
    int32_t _M0L1iS2335;
    moonbit_decref(_M0L7commandS2195);
    _M0L6_2atmpS4648 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L4keysS2330
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L4keysS2330)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L4keysS2330->$0 = _M0L6_2atmpS4648;
    _M0L4keysS2330->$1 = 0;
    _M0L1iS2331 = 1;
    while (1) {
      int32_t _M0L6_2atmpS4638;
      #line 369 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4638 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
      if (_M0L1iS2331 < _M0L6_2atmpS4638) {
        moonbit_string_t _M0L6_2atmpS4639;
        int32_t _M0L6_2atmpS4640;
        #line 370 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4639
        = _M0MPC15array5Array2atGsE(_M0L5partsS2193, _M0L1iS2331);
        #line 370 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L4keysS2330, _M0L6_2atmpS4639);
        moonbit_decref(_M0L6_2atmpS4639);
        _M0L6_2atmpS4640 = _M0L1iS2331 + 1;
        _M0L1iS2331 = _M0L6_2atmpS4640;
        continue;
      } else {
        moonbit_decref(_M0L5partsS2193);
      }
      break;
    }
    #line 372 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L4valsS2333
    = _M0MP19moonbitDB8Database4mget(_M0L2dbS2196, _M0L4keysS2330);
    moonbit_decref(_M0L4keysS2330);
    _M0L6_2atmpS4647 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS2334
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS2334)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS2334->$0 = _M0L6_2atmpS4647;
    _M0L6resultS2334->$1 = 0;
    _M0L1iS2335 = 0;
    while (1) {
      int32_t _M0L6_2atmpS4641;
      #line 374 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4641 = _M0MPC15array5Array6lengthGOsE(_M0L4valsS2333);
      if (_M0L1iS2335 < _M0L6_2atmpS4641) {
        moonbit_string_t _M0L1vS2337;
        moonbit_string_t _M0L7_2abindS2339;
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2338;
        int32_t _M0L6_2atmpS4643;
        moonbit_string_t _M0L6_2atmpS4642;
        int32_t _M0L6_2atmpS4646;
        #line 375 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L7_2abindS2339
        = _M0MPC15array5Array2atGOsE(_M0L4valsS2333, _M0L1iS2335);
        if (_M0L7_2abindS2339 == 0) {
          struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2342;
          int32_t _M0L6_2atmpS4645;
          moonbit_string_t _M0L6_2atmpS4644;
          if (_M0L7_2abindS2339) {
            moonbit_decref(_M0L7_2abindS2339);
          }
          #line 377 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L18_2astring__builderS2342
          = _M0MPB13StringBuilder21StringBuilder_2einner(9);
          #line 377 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2342, (moonbit_string_t)moonbit_string_literal_23.data);
          _M0L6_2atmpS4645 = _M0L1iS2335 + 1;
          #line 377 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2342, _M0L6_2atmpS4645);
          #line 377 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2342, (moonbit_string_t)moonbit_string_literal_31.data);
          #line 377 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS4644
          = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2342);
          moonbit_decref(_M0L18_2astring__builderS2342);
          #line 377 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPC15array5Array4pushGsE(_M0L6resultS2334, _M0L6_2atmpS4644);
          moonbit_decref(_M0L6_2atmpS4644);
        } else {
          moonbit_string_t _M0L7_2aSomeS2340 = _M0L7_2abindS2339;
          moonbit_string_t _M0L4_2avS2341 = _M0L7_2aSomeS2340;
          _M0L1vS2337 = _M0L4_2avS2341;
          goto join_2336;
        }
        goto joinlet_5247;
        join_2336:;
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L18_2astring__builderS2338
        = _M0MPB13StringBuilder21StringBuilder_2einner(6);
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2338, (moonbit_string_t)moonbit_string_literal_23.data);
        _M0L6_2atmpS4643 = _M0L1iS2335 + 1;
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2338, _M0L6_2atmpS4643);
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2338, (moonbit_string_t)moonbit_string_literal_32.data);
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2338, _M0L1vS2337);
        moonbit_decref(_M0L1vS2337);
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2338, (moonbit_string_t)moonbit_string_literal_33.data);
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4642
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2338);
        moonbit_decref(_M0L18_2astring__builderS2338);
        #line 376 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS2334, _M0L6_2atmpS4642);
        moonbit_decref(_M0L6_2atmpS4642);
        joinlet_5247:;
        _M0L6_2atmpS4646 = _M0L1iS2335 + 1;
        _M0L1iS2335 = _M0L6_2atmpS4646;
        continue;
      } else {
        moonbit_decref(_M0L4valsS2333);
      }
      break;
    }
    return _M0L6resultS2334;
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_29.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_29.data, 8)
         ) {
    moonbit_string_t* _M0L6_2atmpS4655;
    struct _M0TPB5ArrayGsE* _M0L4keysS2344;
    int32_t _M0L1iS2345;
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2347;
    int32_t _M0L6_2atmpS4654;
    moonbit_string_t _M0L6_2atmpS4653;
    moonbit_string_t* _M0L6_2atmpS4652;
    struct _M0TPB5ArrayGsE* _block_5244;
    moonbit_decref(_M0L7commandS2195);
    _M0L6_2atmpS4655 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L4keysS2344
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L4keysS2344)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L4keysS2344->$0 = _M0L6_2atmpS4655;
    _M0L4keysS2344->$1 = 0;
    _M0L1iS2345 = 1;
    while (1) {
      int32_t _M0L6_2atmpS4649;
      #line 384 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4649 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
      if (_M0L1iS2345 < _M0L6_2atmpS4649) {
        moonbit_string_t _M0L6_2atmpS4650;
        int32_t _M0L6_2atmpS4651;
        #line 385 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0L6_2atmpS4650
        = _M0MPC15array5Array2atGsE(_M0L5partsS2193, _M0L1iS2345);
        #line 385 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        _M0MPC15array5Array4pushGsE(_M0L4keysS2344, _M0L6_2atmpS4650);
        moonbit_decref(_M0L6_2atmpS4650);
        _M0L6_2atmpS4651 = _M0L1iS2345 + 1;
        _M0L1iS2345 = _M0L6_2atmpS4651;
        continue;
      } else {
        moonbit_decref(_M0L5partsS2193);
      }
      break;
    }
    #line 387 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L18_2astring__builderS2347
    = _M0MPB13StringBuilder21StringBuilder_2einner(10);
    #line 387 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2347, (moonbit_string_t)moonbit_string_literal_28.data);
    #line 387 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4654
    = _M0MP19moonbitDB8Database4mdel(_M0L2dbS2196, _M0L4keysS2344);
    moonbit_decref(_M0L4keysS2344);
    #line 387 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2347, _M0L6_2atmpS4654);
    #line 387 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4653
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2347);
    moonbit_decref(_M0L18_2astring__builderS2347);
    _M0L6_2atmpS4652 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS4652[0] = _M0L6_2atmpS4653;
    _block_5244
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5244)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5244->$0 = _M0L6_2atmpS4652;
    _block_5244->$1 = 1;
    return _block_5244;
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_27.data
           || Moonbit_array_length(_M0L7commandS2195) == 6
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_27.data, 12)
         ) {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2348;
    int32_t _M0L6_2atmpS4658;
    moonbit_string_t _M0L6_2atmpS4657;
    moonbit_string_t* _M0L6_2atmpS4656;
    struct _M0TPB5ArrayGsE* _block_5242;
    moonbit_decref(_M0L7commandS2195);
    moonbit_decref(_M0L5partsS2193);
    #line 389 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L18_2astring__builderS2348
    = _M0MPB13StringBuilder21StringBuilder_2einner(10);
    #line 389 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2348, (moonbit_string_t)moonbit_string_literal_28.data);
    #line 389 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4658 = _M0MP19moonbitDB8Database6dbsize(_M0L2dbS2196);
    #line 389 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2348, _M0L6_2atmpS4658);
    #line 389 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4657
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2348);
    moonbit_decref(_M0L18_2astring__builderS2348);
    _M0L6_2atmpS4656 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS4656[0] = _M0L6_2atmpS4657;
    _block_5242
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5242)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5242->$0 = _M0L6_2atmpS4656;
    _block_5242->$1 = 1;
    return _block_5242;
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_25.data
           || Moonbit_array_length(_M0L7commandS2195) == 7
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_25.data, 14)
         ) {
    moonbit_string_t* _M0L6_2atmpS4659;
    struct _M0TPB5ArrayGsE* _block_5241;
    moonbit_decref(_M0L7commandS2195);
    moonbit_decref(_M0L5partsS2193);
    #line 390 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0MP19moonbitDB8Database7flushdb(_M0L2dbS2196);
    _M0L6_2atmpS4659 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS4659[0] = (moonbit_string_t)moonbit_string_literal_26.data;
    _block_5241
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5241)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5241->$0 = _M0L6_2atmpS4659;
    _block_5241->$1 = 1;
    return _block_5241;
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_24.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_24.data, 8)
         ) {
    moonbit_string_t _M0L6_2atmpS4661;
    moonbit_string_t* _M0L6_2atmpS4660;
    struct _M0TPB5ArrayGsE* _block_5240;
    moonbit_decref(_M0L7commandS2195);
    moonbit_decref(_M0L5partsS2193);
    #line 391 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4661 = _M0MP19moonbitDB8Database4ping();
    _M0L6_2atmpS4660 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS4660[0] = _M0L6_2atmpS4661;
    _block_5240
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5240)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5240->$0 = _M0L6_2atmpS4660;
    _block_5240->$1 = 1;
    return _block_5240;
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_21.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_21.data, 8)
         ) {
    moonbit_string_t _M0L4infoS2349;
    moonbit_string_t _M0L7_2abindS2351;
    int32_t _M0L6_2atmpS4665;
    struct _M0TPC16string10StringView _M0L6_2atmpS4664;
    struct _M0TPB4IterGRPC16string10StringViewE* _M0L5linesS2350;
    moonbit_string_t* _M0L6_2atmpS4663;
    struct _M0TPB5ArrayGsE* _M0L6resultS2352;
    struct _M0TPB4IterGRPC16string10StringViewE* _M0L5_2aitS2353;
    moonbit_decref(_M0L7commandS2195);
    moonbit_decref(_M0L5partsS2193);
    #line 393 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L4infoS2349 = _M0MP19moonbitDB8Database4info(_M0L2dbS2196);
    _M0L7_2abindS2351 = (moonbit_string_t)moonbit_string_literal_22.data;
    _M0L6_2atmpS4665 = Moonbit_array_length(_M0L7_2abindS2351);
    _M0L6_2atmpS4664
    = (struct _M0TPC16string10StringView){
      .$0 = _M0L7_2abindS2351, .$1 = 0, .$2 = _M0L6_2atmpS4665
    };
    #line 394 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L5linesS2350
    = _M0MPC16string6String5split(_M0L4infoS2349, _M0L6_2atmpS4664);
    moonbit_decref(_M0L4infoS2349);
    moonbit_decref(_M0L6_2atmpS4664.$0);
    _M0L6_2atmpS4663 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS2352
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS2352)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS2352->$0 = _M0L6_2atmpS4663;
    _M0L6resultS2352->$1 = 0;
    _M0L5_2aitS2353 = _M0L5linesS2350;
    while (1) {
      struct _M0TPC16string10StringView _M0L4lineS2355;
      void* _M0L7_2abindS2358;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2356;
      moonbit_string_t _M0L6_2atmpS4662;
      #line 396 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2358
      = _M0MPB4Iter4nextGRPC16string10StringViewE(_M0L5_2aitS2353);
      switch (Moonbit_object_tag(_M0L7_2abindS2358)) {
        case 1: {
          struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS2359 =
            (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L7_2abindS2358;
          struct _M0TPC16string10StringView _M0L8_2afieldS4681 =
            _M0L7_2aSomeS2359->$0;
          int32_t _M0L6_2acntS5143 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aSomeS2359));
          struct _M0TPC16string10StringView _M0L7_2alineS2360;
          if (_M0L6_2acntS5143 > 1) {
            int32_t _M0L11_2anew__cntS5144 = _M0L6_2acntS5143 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aSomeS2359), _M0L11_2anew__cntS5144);
            moonbit_incref(_M0L8_2afieldS4681.$0);
          } else if (_M0L6_2acntS5143 == 1) {
            #line 396 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            moonbit_free(_M0L7_2aSomeS2359);
          }
          _M0L7_2alineS2360 = _M0L8_2afieldS4681;
          _M0L4lineS2355 = _M0L7_2alineS2360;
          goto join_2354;
          break;
        }
        default: {
          moonbit_decref(_M0L7_2abindS2358);
          moonbit_decref(_M0L5_2aitS2353);
          break;
        }
      }
      goto joinlet_5239;
      join_2354:;
      #line 397 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2356
      = _M0MPB13StringBuilder21StringBuilder_2einner(2);
      #line 397 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2356, (moonbit_string_t)moonbit_string_literal_23.data);
      #line 397 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L18_2astring__builderS2356, _M0L4lineS2355);
      moonbit_decref(_M0L4lineS2355.$0);
      #line 397 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4662
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2356);
      moonbit_decref(_M0L18_2astring__builderS2356);
      #line 397 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS2352, _M0L6_2atmpS4662);
      moonbit_decref(_M0L6_2atmpS4662);
      continue;
      joinlet_5239:;
      break;
    }
    return _M0L6resultS2352;
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_18.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_18.data, 8)
         ) {
    int32_t _M0L1sS2362;
    int32_t _M0L2usS2363;
    struct _M0TUiiE* _M0L7_2abindS2366;
    int32_t _M0L4_2asS2367;
    int32_t _M0L5_2ausS2368;
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2364;
    moonbit_string_t _M0L6_2atmpS4667;
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2365;
    moonbit_string_t _M0L6_2atmpS4668;
    moonbit_string_t* _M0L6_2atmpS4666;
    struct _M0TPB5ArrayGsE* _block_5237;
    moonbit_decref(_M0L7commandS2195);
    moonbit_decref(_M0L5partsS2193);
    #line 402 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L7_2abindS2366 = _M0MP19moonbitDB8Database4time(_M0L2dbS2196);
    _M0L4_2asS2367 = _M0L7_2abindS2366->$0;
    _M0L5_2ausS2368 = _M0L7_2abindS2366->$1;
    moonbit_decref(_M0L7_2abindS2366);
    _M0L1sS2362 = _M0L4_2asS2367;
    _M0L2usS2363 = _M0L5_2ausS2368;
    goto join_2361;
    join_2361:;
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L18_2astring__builderS2364
    = _M0MPB13StringBuilder21StringBuilder_2einner(11);
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2364, (moonbit_string_t)moonbit_string_literal_19.data);
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2364, _M0L1sS2362);
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4667
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2364);
    moonbit_decref(_M0L18_2astring__builderS2364);
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L18_2astring__builderS2365
    = _M0MPB13StringBuilder21StringBuilder_2einner(16);
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2365, (moonbit_string_t)moonbit_string_literal_20.data);
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2365, _M0L2usS2363);
    #line 403 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4668
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2365);
    moonbit_decref(_M0L18_2astring__builderS2365);
    _M0L6_2atmpS4666 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
    _M0L6_2atmpS4666[0] = _M0L6_2atmpS4667;
    _M0L6_2atmpS4666[1] = _M0L6_2atmpS4668;
    _block_5237
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5237)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5237->$0 = _M0L6_2atmpS4666;
    _block_5237->$1 = 2;
    return _block_5237;
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_13.data
           || Moonbit_array_length(_M0L7commandS2195) == 7
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_13.data, 14)
         ) {
    int32_t _M0L6_2atmpS4669;
    moonbit_decref(_M0L7commandS2195);
    #line 406 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4669 = _M0MPC15array5Array6lengthGsE(_M0L5partsS2193);
    if (_M0L6_2atmpS4669 < 2) {
      moonbit_string_t* _M0L6_2atmpS4670;
      struct _M0TPB5ArrayGsE* _block_5232;
      moonbit_decref(_M0L5partsS2193);
      _M0L6_2atmpS4670 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4670[0] = (moonbit_string_t)moonbit_string_literal_14.data;
      _block_5232
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5232)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5232->$0 = _M0L6_2atmpS4670;
      _block_5232->$1 = 1;
      return _block_5232;
    } else {
      int32_t _M0L1mS2370;
      moonbit_string_t _M0L6_2atmpS4674;
      int64_t _M0L7_2abindS2372;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2371;
      moonbit_string_t _M0L6_2atmpS4672;
      moonbit_string_t* _M0L6_2atmpS4671;
      struct _M0TPB5ArrayGsE* _block_5235;
      #line 408 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4674 = _M0MPC15array5Array2atGsE(_M0L5partsS2193, 1);
      moonbit_decref(_M0L5partsS2193);
      #line 408 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L7_2abindS2372
      = _M0FP39moonbitDB8examples9cli__repl10parse__int(_M0L6_2atmpS4674);
      moonbit_decref(_M0L6_2atmpS4674);
      if (_M0L7_2abindS2372 == 4294967296ll) {
        moonbit_string_t* _M0L6_2atmpS4673 =
          (moonbit_string_t*)moonbit_make_ref_array_raw(1);
        struct _M0TPB5ArrayGsE* _block_5234;
        _M0L6_2atmpS4673[0]
        = (moonbit_string_t)moonbit_string_literal_15.data;
        _block_5234
        = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
        Moonbit_object_header(_block_5234)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
        _block_5234->$0 = _M0L6_2atmpS4673;
        _block_5234->$1 = 1;
        return _block_5234;
      } else {
        int64_t _M0L7_2aSomeS2373 = _M0L7_2abindS2372;
        int32_t _M0L4_2amS2374 = (int32_t)_M0L7_2aSomeS2373;
        _M0L1mS2370 = _M0L4_2amS2374;
        goto join_2369;
      }
      join_2369:;
      #line 409 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MP19moonbitDB8Database13advance__time(_M0L2dbS2196, _M0L1mS2370);
      #line 409 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2371
      = _M0MPB13StringBuilder21StringBuilder_2einner(16);
      #line 409 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2371, (moonbit_string_t)moonbit_string_literal_16.data);
      #line 409 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2371, _M0L1mS2370);
      #line 409 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2371, (moonbit_string_t)moonbit_string_literal_17.data);
      #line 409 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4672
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2371);
      moonbit_decref(_M0L18_2astring__builderS2371);
      _M0L6_2atmpS4671 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
      _M0L6_2atmpS4671[0] = _M0L6_2atmpS4672;
      _block_5235
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_block_5235)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
      _block_5235->$0 = _M0L6_2atmpS4671;
      _block_5235->$1 = 1;
      return _block_5235;
    }
  } else if (
           _M0L7commandS2195
           == (moonbit_string_t)moonbit_string_literal_3.data
           || Moonbit_array_length(_M0L7commandS2195) == 4
              && 0
                 == memcmp(_M0L7commandS2195, (moonbit_string_t)moonbit_string_literal_3.data, 8)
         ) {
    moonbit_string_t* _M0L6_2atmpS4675;
    struct _M0TPB5ArrayGsE* _block_5231;
    moonbit_decref(_M0L7commandS2195);
    moonbit_decref(_M0L5partsS2193);
    _M0L6_2atmpS4675 = (moonbit_string_t*)moonbit_make_ref_array_raw(9);
    _M0L6_2atmpS4675[0] = (moonbit_string_t)moonbit_string_literal_4.data;
    _M0L6_2atmpS4675[1] = (moonbit_string_t)moonbit_string_literal_5.data;
    _M0L6_2atmpS4675[2] = (moonbit_string_t)moonbit_string_literal_6.data;
    _M0L6_2atmpS4675[3] = (moonbit_string_t)moonbit_string_literal_7.data;
    _M0L6_2atmpS4675[4] = (moonbit_string_t)moonbit_string_literal_8.data;
    _M0L6_2atmpS4675[5] = (moonbit_string_t)moonbit_string_literal_9.data;
    _M0L6_2atmpS4675[6] = (moonbit_string_t)moonbit_string_literal_10.data;
    _M0L6_2atmpS4675[7] = (moonbit_string_t)moonbit_string_literal_11.data;
    _M0L6_2atmpS4675[8] = (moonbit_string_t)moonbit_string_literal_12.data;
    _block_5231
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5231)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5231->$0 = _M0L6_2atmpS4675;
    _block_5231->$1 = 9;
    return _block_5231;
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2375;
    moonbit_string_t _M0L6_2atmpS4677;
    moonbit_string_t* _M0L6_2atmpS4676;
    struct _M0TPB5ArrayGsE* _block_5230;
    moonbit_decref(_M0L5partsS2193);
    #line 427 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L18_2astring__builderS2375
    = _M0MPB13StringBuilder21StringBuilder_2einner(22);
    #line 427 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2375, (moonbit_string_t)moonbit_string_literal_1.data);
    #line 427 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2375, _M0L7commandS2195);
    moonbit_decref(_M0L7commandS2195);
    #line 427 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2375, (moonbit_string_t)moonbit_string_literal_2.data);
    #line 427 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
    _M0L6_2atmpS4677
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2375);
    moonbit_decref(_M0L18_2astring__builderS2375);
    _M0L6_2atmpS4676 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
    _M0L6_2atmpS4676[0] = _M0L6_2atmpS4677;
    _block_5230
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5230)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5230->$0 = _M0L6_2atmpS4676;
    _block_5230->$1 = 1;
    return _block_5230;
  }
}

struct _M0TPB5ArrayGsE* _M0FP39moonbitDB8examples9cli__repl14split__command(
  moonbit_string_t _M0L3cmdS2190
) {
  moonbit_string_t* _M0L6_2atmpS4385;
  struct _M0TPB5ArrayGsE* _M0L5partsS2186;
  struct _M0TPB8MutLocalGiE* _M0L5startS2187;
  struct _M0TPB8MutLocalGiE* _M0L1iS2188;
  struct _M0TPB8MutLocalGbE* _M0L9in__quoteS2189;
  int32_t _M0L3valS4379;
  int32_t _if__result_5370;
  #line 62 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L6_2atmpS4385 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L5partsS2186
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L5partsS2186)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L5partsS2186->$0 = _M0L6_2atmpS4385;
  _M0L5partsS2186->$1 = 0;
  _M0L5startS2187
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5startS2187)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5startS2187->$0 = -1;
  _M0L1iS2188
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS2188)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS2188->$0 = 0;
  _M0L9in__quoteS2189
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L9in__quoteS2189)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9in__quoteS2189->$0 = 0;
  while (1) {
    int32_t _M0L3valS4354 = _M0L1iS2188->$0;
    int32_t _M0L6_2atmpS4355 = Moonbit_array_length(_M0L3cmdS2190);
    if (_M0L3valS4354 < _M0L6_2atmpS4355) {
      int32_t _M0L3valS4376 = _M0L1iS2188->$0;
      int32_t _M0L1cS2191;
      int32_t _if__result_5367;
      int32_t _M0L3valS4375;
      int32_t _M0L6_2atmpS4374;
      if (
        _M0L3valS4376 < 0
        || _M0L3valS4376 >= Moonbit_array_length(_M0L3cmdS2190)
      ) {
        #line 68 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        moonbit_panic();
      }
      _M0L1cS2191 = _M0L3cmdS2190[_M0L3valS4376];
      if (_M0L1cS2191 == 39) {
        _if__result_5367 = 1;
      } else {
        _if__result_5367 = _M0L1cS2191 == 34;
      }
      if (_if__result_5367) {
        if (_M0L9in__quoteS2189->$0) {
          int32_t _M0L3valS4356 = _M0L5startS2187->$0;
          if (_M0L3valS4356 >= 0) {
            int32_t _M0L3valS4359 = _M0L5startS2187->$0;
            int32_t _M0L3valS4361 = _M0L1iS2188->$0;
            int64_t _M0L6_2atmpS4360 = (int64_t)_M0L3valS4361;
            struct _M0TPC16string10StringView _M0L6_2atmpS4358;
            moonbit_string_t _M0L6_2atmpS4357;
            #line 73 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            _M0L6_2atmpS4358
            = _M0MPC16string6String11sub_2einner(_M0L3cmdS2190, _M0L3valS4359, _M0L6_2atmpS4360);
            #line 73 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            _M0L6_2atmpS4357
            = _M0MPC16string10StringView9to__owned(_M0L6_2atmpS4358);
            moonbit_decref(_M0L6_2atmpS4358.$0);
            #line 73 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5partsS2186, _M0L6_2atmpS4357);
            moonbit_decref(_M0L6_2atmpS4357);
            _M0L5startS2187->$0 = -1;
          }
          _M0L9in__quoteS2189->$0 = 0;
        } else {
          int32_t _M0L3valS4363;
          int32_t _M0L6_2atmpS4362;
          _M0L9in__quoteS2189->$0 = 1;
          _M0L3valS4363 = _M0L1iS2188->$0;
          _M0L6_2atmpS4362 = _M0L3valS4363 + 1;
          _M0L5startS2187->$0 = _M0L6_2atmpS4362;
        }
      } else {
        int32_t _if__result_5368;
        if (_M0L1cS2191 == 32) {
          int32_t _M0L3valS4364 = _M0L9in__quoteS2189->$0;
          _if__result_5368 = !_M0L3valS4364;
        } else {
          _if__result_5368 = 0;
        }
        if (_if__result_5368) {
          int32_t _M0L3valS4365 = _M0L5startS2187->$0;
          if (_M0L3valS4365 >= 0) {
            int32_t _M0L3valS4368 = _M0L5startS2187->$0;
            int32_t _M0L3valS4370 = _M0L1iS2188->$0;
            int64_t _M0L6_2atmpS4369 = (int64_t)_M0L3valS4370;
            struct _M0TPC16string10StringView _M0L6_2atmpS4367;
            moonbit_string_t _M0L6_2atmpS4366;
            #line 84 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            _M0L6_2atmpS4367
            = _M0MPC16string6String11sub_2einner(_M0L3cmdS2190, _M0L3valS4368, _M0L6_2atmpS4369);
            #line 84 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            _M0L6_2atmpS4366
            = _M0MPC16string10StringView9to__owned(_M0L6_2atmpS4367);
            moonbit_decref(_M0L6_2atmpS4367.$0);
            #line 84 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5partsS2186, _M0L6_2atmpS4366);
            moonbit_decref(_M0L6_2atmpS4366);
            _M0L5startS2187->$0 = -1;
          }
        } else {
          int32_t _M0L3valS4372 = _M0L5startS2187->$0;
          int32_t _if__result_5369;
          if (_M0L3valS4372 == -1) {
            int32_t _M0L3valS4371 = _M0L9in__quoteS2189->$0;
            _if__result_5369 = !_M0L3valS4371;
          } else {
            _if__result_5369 = 0;
          }
          if (_if__result_5369) {
            int32_t _M0L3valS4373 = _M0L1iS2188->$0;
            _M0L5startS2187->$0 = _M0L3valS4373;
          }
        }
      }
      _M0L3valS4375 = _M0L1iS2188->$0;
      _M0L6_2atmpS4374 = _M0L3valS4375 + 1;
      _M0L1iS2188->$0 = _M0L6_2atmpS4374;
      continue;
    } else {
      moonbit_decref(_M0L9in__quoteS2189);
      moonbit_decref(_M0L1iS2188);
    }
    break;
  }
  _M0L3valS4379 = _M0L5startS2187->$0;
  if (_M0L3valS4379 >= 0) {
    int32_t _M0L3valS4377 = _M0L5startS2187->$0;
    int32_t _M0L6_2atmpS4378 = Moonbit_array_length(_M0L3cmdS2190);
    _if__result_5370 = _M0L3valS4377 <= _M0L6_2atmpS4378;
  } else {
    _if__result_5370 = 0;
  }
  if (_if__result_5370) {
    int32_t _M0L3valS4380 = _M0L5startS2187->$0;
    int32_t _M0L6_2atmpS4381 = Moonbit_array_length(_M0L3cmdS2190);
    if (_M0L3valS4380 == _M0L6_2atmpS4381) {
      moonbit_decref(_M0L5startS2187);
      #line 97 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPC15array5Array4pushGsE(_M0L5partsS2186, (moonbit_string_t)moonbit_string_literal_75.data);
    } else {
      int32_t _M0L3valS4384 = _M0L5startS2187->$0;
      struct _M0TPC16string10StringView _M0L6_2atmpS4383;
      moonbit_string_t _M0L6_2atmpS4382;
      moonbit_decref(_M0L5startS2187);
      #line 99 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4383
      = _M0MPC16string6String11sub_2einner(_M0L3cmdS2190, _M0L3valS4384, 4294967296ll);
      #line 99 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS4382
      = _M0MPC16string10StringView9to__owned(_M0L6_2atmpS4383);
      moonbit_decref(_M0L6_2atmpS4383.$0);
      #line 99 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPC15array5Array4pushGsE(_M0L5partsS2186, _M0L6_2atmpS4382);
      moonbit_decref(_M0L6_2atmpS4382);
    }
  } else {
    moonbit_decref(_M0L5startS2187);
  }
  return _M0L5partsS2186;
}

void* _M0FP39moonbitDB8examples9cli__repl12parse__float(
  moonbit_string_t _M0L1sS2179
) {
  struct _M0TPB8MutLocalGfE* _M0L6resultS2176;
  struct _M0TPB8MutLocalGiE* _M0L1iS2177;
  struct _M0TPB8MutLocalGbE* _M0L3negS2178;
  int32_t _M0L6_2atmpS4329;
  int32_t _if__result_5371;
  int32_t _M0L3valS4330;
  int32_t _M0L6_2atmpS4331;
  struct _M0TPB8MutLocalGbE* _M0L8has__dotS2180;
  struct _M0TPB8MutLocalGfE* _M0L12decimal__posS2181;
  struct _M0TPB8MutLocalGbE* _M0L10has__digitS2182;
  int32_t _M0L3valS4350;
  int32_t _result_5375;
  #line 24 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L6resultS2176
  = (struct _M0TPB8MutLocalGfE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGfE));
  Moonbit_object_header(_M0L6resultS2176)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6resultS2176->$0 = 0x0p+0f;
  _M0L1iS2177
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS2177)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS2177->$0 = 0;
  _M0L3negS2178
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L3negS2178)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L3negS2178->$0 = 0;
  _M0L6_2atmpS4329 = Moonbit_array_length(_M0L1sS2179);
  if (_M0L6_2atmpS4329 > 0) {
    int32_t _M0L6_2atmpS4328;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS2179)) {
      #line 28 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4328 = _M0L1sS2179[0];
    _if__result_5371 = _M0L6_2atmpS4328 == 45;
  } else {
    _if__result_5371 = 0;
  }
  if (_if__result_5371) {
    _M0L3negS2178->$0 = 1;
    _M0L1iS2177->$0 = 1;
  }
  _M0L3valS4330 = _M0L1iS2177->$0;
  _M0L6_2atmpS4331 = Moonbit_array_length(_M0L1sS2179);
  if (_M0L3valS4330 >= _M0L6_2atmpS4331) {
    moonbit_decref(_M0L3negS2178);
    moonbit_decref(_M0L1iS2177);
    moonbit_decref(_M0L6resultS2176);
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
  _M0L8has__dotS2180
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L8has__dotS2180)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L8has__dotS2180->$0 = 0;
  _M0L12decimal__posS2181
  = (struct _M0TPB8MutLocalGfE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGfE));
  Moonbit_object_header(_M0L12decimal__posS2181)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L12decimal__posS2181->$0 = 0x1.4p+3f;
  _M0L10has__digitS2182
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L10has__digitS2182)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L10has__digitS2182->$0 = 0;
  while (1) {
    int32_t _M0L3valS4332 = _M0L1iS2177->$0;
    int32_t _M0L6_2atmpS4333 = Moonbit_array_length(_M0L1sS2179);
    if (_M0L3valS4332 < _M0L6_2atmpS4333) {
      int32_t _M0L3valS4349 = _M0L1iS2177->$0;
      int32_t _M0L1cS2183;
      int32_t _if__result_5373;
      int32_t _M0L3valS4348;
      int32_t _M0L6_2atmpS4347;
      if (
        _M0L3valS4349 < 0
        || _M0L3valS4349 >= Moonbit_array_length(_M0L1sS2179)
      ) {
        #line 39 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        moonbit_panic();
      }
      _M0L1cS2183 = _M0L1sS2179[_M0L3valS4349];
      if (_M0L1cS2183 >= 48) {
        _if__result_5373 = _M0L1cS2183 <= 57;
      } else {
        _if__result_5373 = 0;
      }
      if (_if__result_5373) {
        int32_t _M0L6_2atmpS4344;
        int32_t _M0L6_2atmpS4345;
        int32_t _M0L6_2atmpS4343;
        float _M0L5digitS2184;
        _M0L10has__digitS2182->$0 = 1;
        _M0L6_2atmpS4344 = (int32_t)_M0L1cS2183;
        _M0L6_2atmpS4345 = 48;
        _M0L6_2atmpS4343 = _M0L6_2atmpS4344 - _M0L6_2atmpS4345;
        _M0L5digitS2184 = (float)_M0L6_2atmpS4343;
        if (_M0L8has__dotS2180->$0) {
          float _M0L3valS4335 = _M0L6resultS2176->$0;
          float _M0L3valS4337 = _M0L12decimal__posS2181->$0;
          float _M0L6_2atmpS4336 = _M0L5digitS2184 / _M0L3valS4337;
          float _M0L6_2atmpS4334 = _M0L3valS4335 + _M0L6_2atmpS4336;
          float _M0L3valS4339;
          float _M0L6_2atmpS4338;
          _M0L6resultS2176->$0 = _M0L6_2atmpS4334;
          _M0L3valS4339 = _M0L12decimal__posS2181->$0;
          _M0L6_2atmpS4338 = _M0L3valS4339 * 0x1.4p+3f;
          _M0L12decimal__posS2181->$0 = _M0L6_2atmpS4338;
        } else {
          float _M0L3valS4342 = _M0L6resultS2176->$0;
          float _M0L6_2atmpS4341 = _M0L3valS4342 * 0x1.4p+3f;
          float _M0L6_2atmpS4340 = _M0L6_2atmpS4341 + _M0L5digitS2184;
          _M0L6resultS2176->$0 = _M0L6_2atmpS4340;
        }
      } else {
        int32_t _if__result_5374;
        if (_M0L1cS2183 == 46) {
          int32_t _M0L3valS4346 = _M0L8has__dotS2180->$0;
          _if__result_5374 = !_M0L3valS4346;
        } else {
          _if__result_5374 = 0;
        }
        if (_if__result_5374) {
          _M0L8has__dotS2180->$0 = 1;
        } else {
          moonbit_decref(_M0L10has__digitS2182);
          moonbit_decref(_M0L12decimal__posS2181);
          moonbit_decref(_M0L8has__dotS2180);
          moonbit_decref(_M0L3negS2178);
          moonbit_decref(_M0L1iS2177);
          moonbit_decref(_M0L6resultS2176);
          return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        }
      }
      _M0L3valS4348 = _M0L1iS2177->$0;
      _M0L6_2atmpS4347 = _M0L3valS4348 + 1;
      _M0L1iS2177->$0 = _M0L6_2atmpS4347;
      continue;
    } else {
      moonbit_decref(_M0L12decimal__posS2181);
      moonbit_decref(_M0L8has__dotS2180);
      moonbit_decref(_M0L1iS2177);
    }
    break;
  }
  _M0L3valS4350 = _M0L10has__digitS2182->$0;
  moonbit_decref(_M0L10has__digitS2182);
  if (!_M0L3valS4350) {
    moonbit_decref(_M0L3negS2178);
    moonbit_decref(_M0L6resultS2176);
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
  _result_5375 = _M0L3negS2178->$0;
  moonbit_decref(_M0L3negS2178);
  if (_result_5375) {
    float _M0L3valS4352 = _M0L6resultS2176->$0;
    float _M0L6_2atmpS4351;
    void* _block_5376;
    moonbit_decref(_M0L6resultS2176);
    _M0L6_2atmpS4351 = -_M0L3valS4352;
    _block_5376
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGfE4Some));
    Moonbit_object_header(_block_5376)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 1);
    ((struct _M0DTPC16option6OptionGfE4Some*)_block_5376)->$0
    = _M0L6_2atmpS4351;
    return _block_5376;
  } else {
    float _M0L3valS4353 = _M0L6resultS2176->$0;
    void* _block_5377;
    moonbit_decref(_M0L6resultS2176);
    _block_5377
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGfE4Some));
    Moonbit_object_header(_block_5377)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 1);
    ((struct _M0DTPC16option6OptionGfE4Some*)_block_5377)->$0 = _M0L3valS4353;
    return _block_5377;
  }
}

int64_t _M0FP39moonbitDB8examples9cli__repl10parse__int(
  moonbit_string_t _M0L1sS2173
) {
  struct _M0TPB8MutLocalGiE* _M0L6resultS2170;
  struct _M0TPB8MutLocalGiE* _M0L1iS2171;
  struct _M0TPB8MutLocalGbE* _M0L3negS2172;
  int32_t _M0L6_2atmpS4311;
  int32_t _if__result_5378;
  int32_t _M0L3valS4312;
  int32_t _M0L6_2atmpS4313;
  int32_t _result_5381;
  #line 1 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L6resultS2170
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L6resultS2170)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6resultS2170->$0 = 0;
  _M0L1iS2171
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS2171)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS2171->$0 = 0;
  _M0L3negS2172
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L3negS2172)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L3negS2172->$0 = 0;
  _M0L6_2atmpS4311 = Moonbit_array_length(_M0L1sS2173);
  if (_M0L6_2atmpS4311 > 0) {
    int32_t _M0L6_2atmpS4310;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS2173)) {
      #line 5 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4310 = _M0L1sS2173[0];
    _if__result_5378 = _M0L6_2atmpS4310 == 45;
  } else {
    _if__result_5378 = 0;
  }
  if (_if__result_5378) {
    _M0L3negS2172->$0 = 1;
    _M0L1iS2171->$0 = 1;
  }
  _M0L3valS4312 = _M0L1iS2171->$0;
  _M0L6_2atmpS4313 = Moonbit_array_length(_M0L1sS2173);
  if (_M0L3valS4312 >= _M0L6_2atmpS4313) {
    moonbit_decref(_M0L3negS2172);
    moonbit_decref(_M0L1iS2171);
    moonbit_decref(_M0L6resultS2170);
    return 4294967296ll;
  }
  while (1) {
    int32_t _M0L3valS4314 = _M0L1iS2171->$0;
    int32_t _M0L6_2atmpS4315 = Moonbit_array_length(_M0L1sS2173);
    if (_M0L3valS4314 < _M0L6_2atmpS4315) {
      int32_t _M0L3valS4324 = _M0L1iS2171->$0;
      int32_t _M0L1cS2174;
      int32_t _if__result_5380;
      int32_t _M0L3valS4323;
      int32_t _M0L6_2atmpS4322;
      if (
        _M0L3valS4324 < 0
        || _M0L3valS4324 >= Moonbit_array_length(_M0L1sS2173)
      ) {
        #line 13 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
        moonbit_panic();
      }
      _M0L1cS2174 = _M0L1sS2173[_M0L3valS4324];
      if (_M0L1cS2174 >= 48) {
        _if__result_5380 = _M0L1cS2174 <= 57;
      } else {
        _if__result_5380 = 0;
      }
      if (_if__result_5380) {
        int32_t _M0L3valS4321 = _M0L6resultS2170->$0;
        int32_t _M0L6_2atmpS4317 = _M0L3valS4321 * 10;
        int32_t _M0L6_2atmpS4319 = (int32_t)_M0L1cS2174;
        int32_t _M0L6_2atmpS4320 = 48;
        int32_t _M0L6_2atmpS4318 = _M0L6_2atmpS4319 - _M0L6_2atmpS4320;
        int32_t _M0L6_2atmpS4316 = _M0L6_2atmpS4317 + _M0L6_2atmpS4318;
        _M0L6resultS2170->$0 = _M0L6_2atmpS4316;
      } else {
        moonbit_decref(_M0L3negS2172);
        moonbit_decref(_M0L1iS2171);
        moonbit_decref(_M0L6resultS2170);
        return 4294967296ll;
      }
      _M0L3valS4323 = _M0L1iS2171->$0;
      _M0L6_2atmpS4322 = _M0L3valS4323 + 1;
      _M0L1iS2171->$0 = _M0L6_2atmpS4322;
      continue;
    } else {
      moonbit_decref(_M0L1iS2171);
    }
    break;
  }
  _result_5381 = _M0L3negS2172->$0;
  moonbit_decref(_M0L3negS2172);
  if (_result_5381) {
    int32_t _M0L3valS4326 = _M0L6resultS2170->$0;
    int32_t _M0L6_2atmpS4325;
    moonbit_decref(_M0L6resultS2170);
    _M0L6_2atmpS4325 = -_M0L3valS4326;
    return (int64_t)_M0L6_2atmpS4325;
  } else {
    int32_t _M0L3valS4327 = _M0L6resultS2170->$0;
    moonbit_decref(_M0L6resultS2170);
    return (int64_t)_M0L3valS4327;
  }
}

moonbit_string_t _M0FP19moonbitDB16show__opt__float(void* _M0L3optS2106) {
  float _M0L1vS2104;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2105;
  moonbit_string_t _result_5383;
  #line 15 "/home/developer/Documents2/moonbitDB/demo.mbt"
  switch (Moonbit_object_tag(_M0L3optS2106)) {
    case 1: {
      struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS2107 =
        (struct _M0DTPC16option6OptionGfE4Some*)_M0L3optS2106;
      float _M0L4_2avS2108 = _M0L7_2aSomeS2107->$0;
      _M0L1vS2104 = _M0L4_2avS2108;
      goto join_2103;
      break;
    }
    default: {
      return (moonbit_string_t)moonbit_string_literal_38.data;
      break;
    }
  }
  join_2103:;
  #line 17 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS2105
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 17 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS2105, _M0L1vS2104);
  #line 17 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _result_5383
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2105);
  moonbit_decref(_M0L18_2astring__builderS2105);
  return _result_5383;
}

moonbit_string_t _M0FP19moonbitDB14show__opt__int(int64_t _M0L3optS2100) {
  int32_t _M0L1vS2098;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2099;
  moonbit_string_t _result_5385;
  #line 8 "/home/developer/Documents2/moonbitDB/demo.mbt"
  if (_M0L3optS2100 == 4294967296ll) {
    return (moonbit_string_t)moonbit_string_literal_38.data;
  } else {
    int64_t _M0L7_2aSomeS2101 = _M0L3optS2100;
    int32_t _M0L4_2avS2102 = (int32_t)_M0L7_2aSomeS2101;
    _M0L1vS2098 = _M0L4_2avS2102;
    goto join_2097;
  }
  join_2097:;
  #line 10 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS2099
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 10 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2099, _M0L1vS2098);
  #line 10 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _result_5385
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2099);
  moonbit_decref(_M0L18_2astring__builderS2099);
  return _result_5385;
}

moonbit_string_t _M0FP19moonbitDB9show__opt(moonbit_string_t _M0L3optS2094) {
  moonbit_string_t _M0L1vS2092;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2093;
  moonbit_string_t _result_5387;
  #line 1 "/home/developer/Documents2/moonbitDB/demo.mbt"
  if (_M0L3optS2094 == 0) {
    return (moonbit_string_t)moonbit_string_literal_38.data;
  } else {
    moonbit_string_t _M0L7_2aSomeS2095 = _M0L3optS2094;
    moonbit_string_t _M0L4_2avS2096 = _M0L7_2aSomeS2095;
    moonbit_incref(_M0L4_2avS2096);
    _M0L1vS2092 = _M0L4_2avS2096;
    goto join_2091;
  }
  join_2091:;
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS2093
  = _M0MPB13StringBuilder21StringBuilder_2einner(2);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2093, (moonbit_string_t)moonbit_string_literal_33.data);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2093, _M0L1vS2092);
  moonbit_decref(_M0L1vS2092);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2093, (moonbit_string_t)moonbit_string_literal_33.data);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _result_5387
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2093);
  moonbit_decref(_M0L18_2astring__builderS2093);
  return _result_5387;
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database7command() {
  moonbit_string_t* _M0L6_2atmpS4309;
  struct _M0TPB5ArrayGsE* _block_5388;
  #line 1594 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4309 = (moonbit_string_t*)moonbit_make_ref_array_raw(81);
  _M0L6_2atmpS4309[0] = (moonbit_string_t)moonbit_string_literal_73.data;
  _M0L6_2atmpS4309[1] = (moonbit_string_t)moonbit_string_literal_72.data;
  _M0L6_2atmpS4309[2] = (moonbit_string_t)moonbit_string_literal_71.data;
  _M0L6_2atmpS4309[3] = (moonbit_string_t)moonbit_string_literal_70.data;
  _M0L6_2atmpS4309[4] = (moonbit_string_t)moonbit_string_literal_69.data;
  _M0L6_2atmpS4309[5] = (moonbit_string_t)moonbit_string_literal_68.data;
  _M0L6_2atmpS4309[6] = (moonbit_string_t)moonbit_string_literal_67.data;
  _M0L6_2atmpS4309[7] = (moonbit_string_t)moonbit_string_literal_66.data;
  _M0L6_2atmpS4309[8] = (moonbit_string_t)moonbit_string_literal_65.data;
  _M0L6_2atmpS4309[9] = (moonbit_string_t)moonbit_string_literal_64.data;
  _M0L6_2atmpS4309[10] = (moonbit_string_t)moonbit_string_literal_63.data;
  _M0L6_2atmpS4309[11] = (moonbit_string_t)moonbit_string_literal_76.data;
  _M0L6_2atmpS4309[12] = (moonbit_string_t)moonbit_string_literal_62.data;
  _M0L6_2atmpS4309[13] = (moonbit_string_t)moonbit_string_literal_77.data;
  _M0L6_2atmpS4309[14] = (moonbit_string_t)moonbit_string_literal_61.data;
  _M0L6_2atmpS4309[15] = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L6_2atmpS4309[16] = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS4309[17] = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L6_2atmpS4309[18] = (moonbit_string_t)moonbit_string_literal_60.data;
  _M0L6_2atmpS4309[19] = (moonbit_string_t)moonbit_string_literal_59.data;
  _M0L6_2atmpS4309[20] = (moonbit_string_t)moonbit_string_literal_58.data;
  _M0L6_2atmpS4309[21] = (moonbit_string_t)moonbit_string_literal_57.data;
  _M0L6_2atmpS4309[22] = (moonbit_string_t)moonbit_string_literal_56.data;
  _M0L6_2atmpS4309[23] = (moonbit_string_t)moonbit_string_literal_78.data;
  _M0L6_2atmpS4309[24] = (moonbit_string_t)moonbit_string_literal_79.data;
  _M0L6_2atmpS4309[25] = (moonbit_string_t)moonbit_string_literal_80.data;
  _M0L6_2atmpS4309[26] = (moonbit_string_t)moonbit_string_literal_81.data;
  _M0L6_2atmpS4309[27] = (moonbit_string_t)moonbit_string_literal_82.data;
  _M0L6_2atmpS4309[28] = (moonbit_string_t)moonbit_string_literal_55.data;
  _M0L6_2atmpS4309[29] = (moonbit_string_t)moonbit_string_literal_54.data;
  _M0L6_2atmpS4309[30] = (moonbit_string_t)moonbit_string_literal_53.data;
  _M0L6_2atmpS4309[31] = (moonbit_string_t)moonbit_string_literal_52.data;
  _M0L6_2atmpS4309[32] = (moonbit_string_t)moonbit_string_literal_51.data;
  _M0L6_2atmpS4309[33] = (moonbit_string_t)moonbit_string_literal_50.data;
  _M0L6_2atmpS4309[34] = (moonbit_string_t)moonbit_string_literal_83.data;
  _M0L6_2atmpS4309[35] = (moonbit_string_t)moonbit_string_literal_84.data;
  _M0L6_2atmpS4309[36] = (moonbit_string_t)moonbit_string_literal_85.data;
  _M0L6_2atmpS4309[37] = (moonbit_string_t)moonbit_string_literal_86.data;
  _M0L6_2atmpS4309[38] = (moonbit_string_t)moonbit_string_literal_87.data;
  _M0L6_2atmpS4309[39] = (moonbit_string_t)moonbit_string_literal_88.data;
  _M0L6_2atmpS4309[40] = (moonbit_string_t)moonbit_string_literal_89.data;
  _M0L6_2atmpS4309[41] = (moonbit_string_t)moonbit_string_literal_49.data;
  _M0L6_2atmpS4309[42] = (moonbit_string_t)moonbit_string_literal_48.data;
  _M0L6_2atmpS4309[43] = (moonbit_string_t)moonbit_string_literal_47.data;
  _M0L6_2atmpS4309[44] = (moonbit_string_t)moonbit_string_literal_46.data;
  _M0L6_2atmpS4309[45] = (moonbit_string_t)moonbit_string_literal_45.data;
  _M0L6_2atmpS4309[46] = (moonbit_string_t)moonbit_string_literal_90.data;
  _M0L6_2atmpS4309[47] = (moonbit_string_t)moonbit_string_literal_91.data;
  _M0L6_2atmpS4309[48] = (moonbit_string_t)moonbit_string_literal_92.data;
  _M0L6_2atmpS4309[49] = (moonbit_string_t)moonbit_string_literal_93.data;
  _M0L6_2atmpS4309[50] = (moonbit_string_t)moonbit_string_literal_94.data;
  _M0L6_2atmpS4309[51] = (moonbit_string_t)moonbit_string_literal_95.data;
  _M0L6_2atmpS4309[52] = (moonbit_string_t)moonbit_string_literal_96.data;
  _M0L6_2atmpS4309[53] = (moonbit_string_t)moonbit_string_literal_97.data;
  _M0L6_2atmpS4309[54] = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS4309[55] = (moonbit_string_t)moonbit_string_literal_41.data;
  _M0L6_2atmpS4309[56] = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L6_2atmpS4309[57] = (moonbit_string_t)moonbit_string_literal_39.data;
  _M0L6_2atmpS4309[58] = (moonbit_string_t)moonbit_string_literal_98.data;
  _M0L6_2atmpS4309[59] = (moonbit_string_t)moonbit_string_literal_99.data;
  _M0L6_2atmpS4309[60] = (moonbit_string_t)moonbit_string_literal_100.data;
  _M0L6_2atmpS4309[61] = (moonbit_string_t)moonbit_string_literal_101.data;
  _M0L6_2atmpS4309[62] = (moonbit_string_t)moonbit_string_literal_102.data;
  _M0L6_2atmpS4309[63] = (moonbit_string_t)moonbit_string_literal_37.data;
  _M0L6_2atmpS4309[64] = (moonbit_string_t)moonbit_string_literal_103.data;
  _M0L6_2atmpS4309[65] = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L6_2atmpS4309[66] = (moonbit_string_t)moonbit_string_literal_104.data;
  _M0L6_2atmpS4309[67] = (moonbit_string_t)moonbit_string_literal_105.data;
  _M0L6_2atmpS4309[68] = (moonbit_string_t)moonbit_string_literal_106.data;
  _M0L6_2atmpS4309[69] = (moonbit_string_t)moonbit_string_literal_107.data;
  _M0L6_2atmpS4309[70] = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L6_2atmpS4309[71] = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS4309[72] = (moonbit_string_t)moonbit_string_literal_108.data;
  _M0L6_2atmpS4309[73] = (moonbit_string_t)moonbit_string_literal_109.data;
  _M0L6_2atmpS4309[74] = (moonbit_string_t)moonbit_string_literal_110.data;
  _M0L6_2atmpS4309[75] = (moonbit_string_t)moonbit_string_literal_111.data;
  _M0L6_2atmpS4309[76] = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L6_2atmpS4309[77] = (moonbit_string_t)moonbit_string_literal_112.data;
  _M0L6_2atmpS4309[78] = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS4309[79] = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS4309[80] = (moonbit_string_t)moonbit_string_literal_113.data;
  _block_5388
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_block_5388)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _block_5388->$0 = _M0L6_2atmpS4309;
  _block_5388->$1 = 81;
  return _block_5388;
}

struct _M0TUiiE* _M0MP19moonbitDB8Database4time(
  struct _M0TP19moonbitDB8Database* _M0L4selfS2089
) {
  int32_t _M0L13current__timeS4308;
  int32_t _M0L7secondsS2088;
  int32_t _M0L13current__timeS4307;
  int32_t _M0L6_2atmpS4306;
  int32_t _M0L12microsecondsS2090;
  struct _M0TUiiE* _block_5389;
  #line 1588 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L13current__timeS4308 = _M0L4selfS2089->$2;
  _M0L7secondsS2088 = _M0L13current__timeS4308 / 1000;
  _M0L13current__timeS4307 = _M0L4selfS2089->$2;
  _M0L6_2atmpS4306 = _M0L13current__timeS4307 % 1000;
  _M0L12microsecondsS2090 = _M0L6_2atmpS4306 * 1000;
  _block_5389 = (struct _M0TUiiE*)moonbit_malloc(sizeof(struct _M0TUiiE));
  Moonbit_object_header(_block_5389)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _block_5389->$0 = _M0L7secondsS2088;
  _block_5389->$1 = _M0L12microsecondsS2090;
  return _block_5389;
}

moonbit_string_t _M0MP19moonbitDB8Database4info(
  struct _M0TP19moonbitDB8Database* _M0L4selfS2068
) {
  struct _M0TPB8MutLocalGiE* _M0L10key__countS2065;
  struct _M0TPB8MutLocalGiE* _M0L13expire__countS2066;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4293;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L5_2aitS2067;
  struct _M0TPB3MapGsiE* _M0L7expiresS4297;
  struct _M0TPB4IterGUsiEE* _M0L5_2aitS2076;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2085;
  int32_t _M0L13current__timeS4305;
  moonbit_string_t _M0L6_2atmpS4301;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2086;
  int32_t _M0L3valS4303;
  int32_t _M0L3valS4304;
  moonbit_string_t _M0L6_2atmpS4302;
  moonbit_string_t* _M0L6_2atmpS4300;
  struct _M0TPB5ArrayGsE* _M0L9info__arrS2084;
  moonbit_string_t _M0L7_2abindS2087;
  int32_t _M0L6_2atmpS4299;
  struct _M0TPC16string10StringView _M0L6_2atmpS4298;
  moonbit_string_t _result_5395;
  #line 1564 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L10key__countS2065
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L10key__countS2065)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L10key__countS2065->$0 = 0;
  _M0L13expire__countS2066
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L13expire__countS2066)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L13expire__countS2066->$0 = 0;
  _M0L4dataS4293 = _M0L4selfS2068->$0;
  moonbit_incref(_M0L4dataS4293);
  #line 1566 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS2067
  = _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(_M0L4dataS4293);
  moonbit_decref(_M0L4dataS4293);
  while (1) {
    moonbit_string_t _M0L3keyS2070;
    struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2abindS2072;
    int32_t _M0L6_2atmpS4290;
    #line 1567 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS2072
    = _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(_M0L5_2aitS2067);
    if (_M0L7_2abindS2072 == 0) {
      if (_M0L7_2abindS2072) {
        moonbit_decref(_M0L7_2abindS2072);
      }
      moonbit_decref(_M0L5_2aitS2067);
    } else {
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS2073 =
        _M0L7_2abindS2072;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L4_2axS2074 =
        _M0L7_2aSomeS2073;
      moonbit_string_t _M0L8_2afieldS4685 = _M0L4_2axS2074->$0;
      int32_t _M0L6_2acntS5145 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS2074));
      moonbit_string_t _M0L6_2akeyS2075;
      if (_M0L6_2acntS5145 > 1) {
        int32_t _M0L11_2anew__cntS5147 = _M0L6_2acntS5145 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS2074), _M0L11_2anew__cntS5147);
        moonbit_incref(_M0L8_2afieldS4685);
      } else if (_M0L6_2acntS5145 == 1) {
        void* _M0L8_2afieldS5146 = _M0L4_2axS2074->$1;
        moonbit_decref(_M0L8_2afieldS5146);
        #line 1567 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS2074);
      }
      _M0L6_2akeyS2075 = _M0L8_2afieldS4685;
      _M0L3keyS2070 = _M0L6_2akeyS2075;
      goto join_2069;
    }
    goto joinlet_5391;
    join_2069:;
    #line 1568 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS4290
    = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS2068, _M0L3keyS2070);
    moonbit_decref(_M0L3keyS2070);
    if (!_M0L6_2atmpS4290) {
      int32_t _M0L3valS4292 = _M0L10key__countS2065->$0;
      int32_t _M0L6_2atmpS4291 = _M0L3valS4292 + 1;
      _M0L10key__countS2065->$0 = _M0L6_2atmpS4291;
    }
    continue;
    joinlet_5391:;
    break;
  }
  _M0L7expiresS4297 = _M0L4selfS2068->$1;
  moonbit_incref(_M0L7expiresS4297);
  #line 1566 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS2076 = _M0MPB3Map5iter2GsiE(_M0L7expiresS4297);
  moonbit_decref(_M0L7expiresS4297);
  while (1) {
    moonbit_string_t _M0L3keyS2078;
    struct _M0TUsiE* _M0L7_2abindS2080;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4294;
    int32_t _result_5394;
    #line 1572 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS2080 = _M0MPB5Iter24nextGsiE(_M0L5_2aitS2076);
    if (_M0L7_2abindS2080 == 0) {
      if (_M0L7_2abindS2080) {
        moonbit_decref(_M0L7_2abindS2080);
      }
      moonbit_decref(_M0L5_2aitS2076);
    } else {
      struct _M0TUsiE* _M0L7_2aSomeS2081 = _M0L7_2abindS2080;
      struct _M0TUsiE* _M0L4_2axS2082 = _M0L7_2aSomeS2081;
      moonbit_string_t _M0L8_2afieldS4683 = _M0L4_2axS2082->$0;
      int32_t _M0L6_2acntS5148 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS2082));
      moonbit_string_t _M0L6_2akeyS2083;
      if (_M0L6_2acntS5148 > 1) {
        int32_t _M0L11_2anew__cntS5149 = _M0L6_2acntS5148 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS2082), _M0L11_2anew__cntS5149);
        moonbit_incref(_M0L8_2afieldS4683);
      } else if (_M0L6_2acntS5148 == 1) {
        #line 1572 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS2082);
      }
      _M0L6_2akeyS2083 = _M0L8_2afieldS4683;
      _M0L3keyS2078 = _M0L6_2akeyS2083;
      goto join_2077;
    }
    goto joinlet_5393;
    join_2077:;
    _M0L4dataS4294 = _M0L4selfS2068->$0;
    moonbit_incref(_M0L4dataS4294);
    #line 1573 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_5394
    = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS4294, _M0L3keyS2078);
    moonbit_decref(_M0L4dataS4294);
    moonbit_decref(_M0L3keyS2078);
    if (_result_5394) {
      int32_t _M0L3valS4296 = _M0L13expire__countS2066->$0;
      int32_t _M0L6_2atmpS4295 = _M0L3valS4296 + 1;
      _M0L13expire__countS2066->$0 = _M0L6_2atmpS4295;
    }
    continue;
    joinlet_5393:;
    break;
  }
  #line 1580 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L18_2astring__builderS2085
  = _M0MPB13StringBuilder21StringBuilder_2einner(13);
  #line 1580 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2085, (moonbit_string_t)moonbit_string_literal_114.data);
  _M0L13current__timeS4305 = _M0L4selfS2068->$2;
  #line 1580 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2085, _M0L13current__timeS4305);
  #line 1580 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4301
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2085);
  moonbit_decref(_M0L18_2astring__builderS2085);
  #line 1583 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L18_2astring__builderS2086
  = _M0MPB13StringBuilder21StringBuilder_2einner(28);
  #line 1583 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2086, (moonbit_string_t)moonbit_string_literal_115.data);
  _M0L3valS4303 = _M0L10key__countS2065->$0;
  moonbit_decref(_M0L10key__countS2065);
  #line 1583 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2086, _M0L3valS4303);
  #line 1583 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2086, (moonbit_string_t)moonbit_string_literal_116.data);
  _M0L3valS4304 = _M0L13expire__countS2066->$0;
  moonbit_decref(_M0L13expire__countS2066);
  #line 1583 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2086, _M0L3valS4304);
  #line 1583 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2086, (moonbit_string_t)moonbit_string_literal_117.data);
  #line 1583 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4302
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2086);
  moonbit_decref(_M0L18_2astring__builderS2086);
  _M0L6_2atmpS4300 = (moonbit_string_t*)moonbit_make_ref_array_raw(6);
  _M0L6_2atmpS4300[0] = (moonbit_string_t)moonbit_string_literal_118.data;
  _M0L6_2atmpS4300[1] = (moonbit_string_t)moonbit_string_literal_119.data;
  _M0L6_2atmpS4300[2] = _M0L6_2atmpS4301;
  _M0L6_2atmpS4300[3] = (moonbit_string_t)moonbit_string_literal_75.data;
  _M0L6_2atmpS4300[4] = (moonbit_string_t)moonbit_string_literal_120.data;
  _M0L6_2atmpS4300[5] = _M0L6_2atmpS4302;
  _M0L9info__arrS2084
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L9info__arrS2084)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L9info__arrS2084->$0 = _M0L6_2atmpS4300;
  _M0L9info__arrS2084->$1 = 6;
  _M0L7_2abindS2087 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS4299 = Moonbit_array_length(_M0L7_2abindS2087);
  _M0L6_2atmpS4298
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS2087, .$1 = 0, .$2 = _M0L6_2atmpS4299
  };
  #line 1585 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_5395
  = _M0MPC15array5Array4joinGsE(_M0L9info__arrS2084, _M0L6_2atmpS4298);
  moonbit_decref(_M0L9info__arrS2084);
  moonbit_decref(_M0L6_2atmpS4298.$0);
  return _result_5395;
}

moonbit_string_t _M0MP19moonbitDB8Database4ping() {
  #line 1556 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  return (moonbit_string_t)moonbit_string_literal_121.data;
}

moonbit_string_t _M0MP19moonbitDB8Database9randomkey(
  struct _M0TP19moonbitDB8Database* _M0L4selfS2055
) {
  moonbit_string_t* _M0L6_2atmpS4289;
  struct _M0TPB5ArrayGsE* _M0L9all__keysS2053;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4284;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L5_2aitS2054;
  int32_t _M0L6_2atmpS4285;
  #line 1503 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4289 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L9all__keysS2053
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L9all__keysS2053)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L9all__keysS2053->$0 = _M0L6_2atmpS4289;
  _M0L9all__keysS2053->$1 = 0;
  _M0L4dataS4284 = _M0L4selfS2055->$0;
  moonbit_incref(_M0L4dataS4284);
  #line 1504 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS2054
  = _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(_M0L4dataS4284);
  moonbit_decref(_M0L4dataS4284);
  while (1) {
    moonbit_string_t _M0L3keyS2057;
    struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2abindS2059;
    int32_t _M0L6_2atmpS4283;
    #line 1505 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS2059
    = _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(_M0L5_2aitS2054);
    if (_M0L7_2abindS2059 == 0) {
      if (_M0L7_2abindS2059) {
        moonbit_decref(_M0L7_2abindS2059);
      }
      moonbit_decref(_M0L5_2aitS2054);
    } else {
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS2060 =
        _M0L7_2abindS2059;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L4_2axS2061 =
        _M0L7_2aSomeS2060;
      moonbit_string_t _M0L8_2afieldS4687 = _M0L4_2axS2061->$0;
      int32_t _M0L6_2acntS5150 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS2061));
      moonbit_string_t _M0L6_2akeyS2062;
      if (_M0L6_2acntS5150 > 1) {
        int32_t _M0L11_2anew__cntS5152 = _M0L6_2acntS5150 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS2061), _M0L11_2anew__cntS5152);
        moonbit_incref(_M0L8_2afieldS4687);
      } else if (_M0L6_2acntS5150 == 1) {
        void* _M0L8_2afieldS5151 = _M0L4_2axS2061->$1;
        moonbit_decref(_M0L8_2afieldS5151);
        #line 1505 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS2061);
      }
      _M0L6_2akeyS2062 = _M0L8_2afieldS4687;
      _M0L3keyS2057 = _M0L6_2akeyS2062;
      goto join_2056;
    }
    goto joinlet_5397;
    join_2056:;
    #line 1506 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS4283
    = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS2055, _M0L3keyS2057);
    if (!_M0L6_2atmpS4283) {
      #line 1507 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L9all__keysS2053, _M0L3keyS2057);
      moonbit_decref(_M0L3keyS2057);
    } else {
      moonbit_decref(_M0L3keyS2057);
    }
    continue;
    joinlet_5397:;
    break;
  }
  #line 1510 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4285 = _M0MPC15array5Array6lengthGsE(_M0L9all__keysS2053);
  if (_M0L6_2atmpS4285 == 0) {
    moonbit_decref(_M0L9all__keysS2053);
    return 0;
  } else {
    int32_t _M0L13current__timeS4287 = _M0L4selfS2055->$2;
    int32_t _M0L6_2atmpS4288;
    int32_t _M0L3idxS2063;
    int32_t _M0L9safe__idxS2064;
    moonbit_string_t _M0L6_2atmpS4286;
    #line 1513 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS4288 = _M0MPC15array5Array6lengthGsE(_M0L9all__keysS2053);
    _M0L3idxS2063 = _M0L13current__timeS4287 % _M0L6_2atmpS4288;
    if (_M0L3idxS2063 < 0) {
      _M0L9safe__idxS2064 = -_M0L3idxS2063;
    } else {
      _M0L9safe__idxS2064 = _M0L3idxS2063;
    }
    #line 1515 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS4286
    = _M0MPC15array5Array2atGsE(_M0L9all__keysS2053, _M0L9safe__idxS2064);
    moonbit_decref(_M0L9all__keysS2053);
    return _M0L6_2atmpS4286;
  }
}

int32_t _M0MP19moonbitDB8Database7flushdb(
  struct _M0TP19moonbitDB8Database* _M0L4selfS2050
) {
  struct _M0TUsRP19moonbitDB10RedisValueE** _M0L7_2abindS2051;
  struct _M0TUsRP19moonbitDB10RedisValueE** _M0L6_2atmpS4279;
  struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE _M0L6_2atmpS4278;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS4277;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4690;
  struct _M0TUsiE** _M0L7_2abindS2052;
  struct _M0TUsiE** _M0L6_2atmpS4282;
  struct _M0TPB9ArrayViewGUsiEE _M0L6_2atmpS4281;
  struct _M0TPB3MapGsiE* _M0L6_2atmpS4280;
  struct _M0TPB3MapGsiE* _M0L6_2aoldS4689;
  #line 1494 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS2051
  = (struct _M0TUsRP19moonbitDB10RedisValueE**)moonbit_empty_ref_array;
  _M0L6_2atmpS4279 = _M0L7_2abindS2051;
  _M0L6_2atmpS4278
  = (struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE){
    .$0 = _M0L6_2atmpS4279, .$1 = 0, .$2 = 0
  };
  #line 1495 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4277
  = _M0MPB3Map3MapGsRP19moonbitDB10RedisValueE(_M0L6_2atmpS4278, 1000ll);
  moonbit_decref(_M0L6_2atmpS4278.$0);
  _M0L6_2aoldS4690 = _M0L4selfS2050->$0;
  moonbit_decref(_M0L6_2aoldS4690);
  _M0L4selfS2050->$0 = _M0L6_2atmpS4277;
  _M0L7_2abindS2052 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L6_2atmpS4282 = _M0L7_2abindS2052;
  _M0L6_2atmpS4281
  = (struct _M0TPB9ArrayViewGUsiEE){
    .$0 = _M0L6_2atmpS4282, .$1 = 0, .$2 = 0
  };
  #line 1496 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4280 = _M0MPB3Map3MapGsiE(_M0L6_2atmpS4281, 1000ll);
  moonbit_decref(_M0L6_2atmpS4281.$0);
  _M0L6_2aoldS4689 = _M0L4selfS2050->$1;
  moonbit_decref(_M0L6_2aoldS4689);
  _M0L4selfS2050->$1 = _M0L6_2atmpS4280;
  return 0;
}

int32_t _M0MP19moonbitDB8Database6dbsize(
  struct _M0TP19moonbitDB8Database* _M0L4selfS2042
) {
  struct _M0TPB8MutLocalGiE* _M0L5countS2040;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4276;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L5_2aitS2041;
  int32_t _result_5400;
  #line 1484 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5countS2040
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5countS2040)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5countS2040->$0 = 0;
  _M0L4dataS4276 = _M0L4selfS2042->$0;
  moonbit_incref(_M0L4dataS4276);
  #line 1485 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS2041
  = _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(_M0L4dataS4276);
  moonbit_decref(_M0L4dataS4276);
  while (1) {
    moonbit_string_t _M0L3keyS2044;
    struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2abindS2046;
    int32_t _M0L6_2atmpS4273;
    #line 1486 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS2046
    = _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(_M0L5_2aitS2041);
    if (_M0L7_2abindS2046 == 0) {
      if (_M0L7_2abindS2046) {
        moonbit_decref(_M0L7_2abindS2046);
      }
      moonbit_decref(_M0L5_2aitS2041);
    } else {
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS2047 =
        _M0L7_2abindS2046;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L4_2axS2048 =
        _M0L7_2aSomeS2047;
      moonbit_string_t _M0L8_2afieldS4691 = _M0L4_2axS2048->$0;
      int32_t _M0L6_2acntS5153 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS2048));
      moonbit_string_t _M0L6_2akeyS2049;
      if (_M0L6_2acntS5153 > 1) {
        int32_t _M0L11_2anew__cntS5155 = _M0L6_2acntS5153 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS2048), _M0L11_2anew__cntS5155);
        moonbit_incref(_M0L8_2afieldS4691);
      } else if (_M0L6_2acntS5153 == 1) {
        void* _M0L8_2afieldS5154 = _M0L4_2axS2048->$1;
        moonbit_decref(_M0L8_2afieldS5154);
        #line 1486 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS2048);
      }
      _M0L6_2akeyS2049 = _M0L8_2afieldS4691;
      _M0L3keyS2044 = _M0L6_2akeyS2049;
      goto join_2043;
    }
    goto joinlet_5399;
    join_2043:;
    #line 1487 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS4273
    = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS2042, _M0L3keyS2044);
    moonbit_decref(_M0L3keyS2044);
    if (!_M0L6_2atmpS4273) {
      int32_t _M0L3valS4275 = _M0L5countS2040->$0;
      int32_t _M0L6_2atmpS4274 = _M0L3valS4275 + 1;
      _M0L5countS2040->$0 = _M0L6_2atmpS4274;
    }
    continue;
    joinlet_5399:;
    break;
  }
  _result_5400 = _M0L5countS2040->$0;
  moonbit_decref(_M0L5countS2040);
  return _result_5400;
}

float _M0MP19moonbitDB8Database7zincrby(
  struct _M0TP19moonbitDB8Database* _M0L4selfS2019,
  moonbit_string_t _M0L3keyS2020,
  float _M0L9incrementS2039,
  moonbit_string_t _M0L11member__valS2035
) {
  int32_t _M0L6_2atmpS4267;
  struct _M0TPB3MapGsfE* _M0L4zsetS2021;
  struct _M0TPB3MapGsfE* _M0L1zS2025;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4272;
  void* _M0L7_2abindS2026;
  struct _M0TUsfE** _M0L7_2abindS2023;
  struct _M0TUsfE** _M0L6_2atmpS4271;
  struct _M0TPB9ArrayViewGUsfEE _M0L6_2atmpS4270;
  float _M0L1sS2033;
  float _M0L14current__scoreS2031;
  void* _M0L7_2abindS2034;
  float _M0L10new__scoreS2038;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4268;
  void* _M0L4ZSetS4269;
  #line 1379 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1380 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4267
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS2019, _M0L3keyS2020);
  _M0L4dataS4272 = _M0L4selfS2019->$0;
  moonbit_incref(_M0L4dataS4272);
  #line 1381 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS2026
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4272, _M0L3keyS2020);
  moonbit_decref(_M0L4dataS4272);
  if (_M0L7_2abindS2026 == 0) {
    if (_M0L7_2abindS2026) {
      moonbit_decref(_M0L7_2abindS2026);
    }
    goto join_2022;
  } else {
    void* _M0L7_2aSomeS2027 = _M0L7_2abindS2026;
    void* _M0L4_2axS2028 = _M0L7_2aSomeS2027;
    switch (Moonbit_object_tag(_M0L4_2axS2028)) {
      case 4: {
        struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS2029 =
          (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS2028;
        struct _M0TPB3MapGsfE* _M0L8_2afieldS4694 = _M0L7_2aZSetS2029->$0;
        int32_t _M0L6_2acntS5156 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS2029));
        struct _M0TPB3MapGsfE* _M0L4_2azS2030;
        if (_M0L6_2acntS5156 > 1) {
          int32_t _M0L11_2anew__cntS5157 = _M0L6_2acntS5156 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS2029), _M0L11_2anew__cntS5157);
          moonbit_incref(_M0L8_2afieldS4694);
        } else if (_M0L6_2acntS5156 == 1) {
          #line 1381 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aZSetS2029);
        }
        _M0L4_2azS2030 = _M0L8_2afieldS4694;
        _M0L1zS2025 = _M0L4_2azS2030;
        goto join_2024;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS2028);
        goto join_2022;
        break;
      }
    }
  }
  goto joinlet_5402;
  join_2024:;
  _M0L4zsetS2021 = _M0L1zS2025;
  joinlet_5402:;
  goto joinlet_5401;
  join_2022:;
  _M0L7_2abindS2023 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6_2atmpS4271 = _M0L7_2abindS2023;
  _M0L6_2atmpS4270
  = (struct _M0TPB9ArrayViewGUsfEE){
    .$0 = _M0L6_2atmpS4271, .$1 = 0, .$2 = 0
  };
  #line 1383 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4zsetS2021 = _M0MPB3Map3MapGsfE(_M0L6_2atmpS4270, 10ll);
  moonbit_decref(_M0L6_2atmpS4270.$0);
  joinlet_5401:;
  #line 1385 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS2034
  = _M0MPB3Map3getGsfE(_M0L4zsetS2021, _M0L11member__valS2035);
  switch (Moonbit_object_tag(_M0L7_2abindS2034)) {
    case 1: {
      struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS2036 =
        (struct _M0DTPC16option6OptionGfE4Some*)_M0L7_2abindS2034;
      float _M0L4_2asS2037 = _M0L7_2aSomeS2036->$0;
      moonbit_decref(_M0L7_2aSomeS2036);
      _M0L1sS2033 = _M0L4_2asS2037;
      goto join_2032;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2abindS2034);
      _M0L14current__scoreS2031 = 0x0p+0f;
      break;
    }
  }
  goto joinlet_5403;
  join_2032:;
  _M0L14current__scoreS2031 = _M0L1sS2033;
  joinlet_5403:;
  _M0L10new__scoreS2038 = _M0L14current__scoreS2031 + _M0L9incrementS2039;
  #line 1390 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsfE(_M0L4zsetS2021, _M0L11member__valS2035, _M0L10new__scoreS2038);
  _M0L4dataS4268 = _M0L4selfS2019->$0;
  _M0L4ZSetS4269
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4ZSet));
  Moonbit_object_header(_M0L4ZSetS4269)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 4);
  ((struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4ZSetS4269)->$0
  = _M0L4zsetS2021;
  moonbit_incref(_M0L4dataS4268);
  #line 1391 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4268, _M0L3keyS2020, _M0L4ZSetS4269);
  moonbit_decref(_M0L4dataS4268);
  moonbit_decref(_M0L4ZSetS4269);
  return _M0L10new__scoreS2038;
}

int64_t _M0MP19moonbitDB8Database5zrank(
  struct _M0TP19moonbitDB8Database* _M0L4selfS2004,
  moonbit_string_t _M0L3keyS2005,
  moonbit_string_t _M0L11member__valS2009
) {
  #line 1328 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1329 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS2004, _M0L3keyS2005)
  ) {
    return 4294967296ll;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS2008;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4266 =
      _M0L4selfS2004->$0;
    void* _M0L7_2abindS2014;
    int32_t _M0L6_2atmpS4260;
    moonbit_incref(_M0L4dataS4266);
    #line 1332 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS2014
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4266, _M0L3keyS2005);
    moonbit_decref(_M0L4dataS4266);
    if (_M0L7_2abindS2014 == 0) {
      if (_M0L7_2abindS2014) {
        moonbit_decref(_M0L7_2abindS2014);
      }
      goto join_2006;
    } else {
      void* _M0L7_2aSomeS2015 = _M0L7_2abindS2014;
      void* _M0L4_2axS2016 = _M0L7_2aSomeS2015;
      switch (Moonbit_object_tag(_M0L4_2axS2016)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS2017 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS2016;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS4697 = _M0L7_2aZSetS2017->$0;
          int32_t _M0L6_2acntS5160 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS2017));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS2018;
          if (_M0L6_2acntS5160 > 1) {
            int32_t _M0L11_2anew__cntS5161 = _M0L6_2acntS5160 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS2017), _M0L11_2anew__cntS5161);
            moonbit_incref(_M0L8_2afieldS4697);
          } else if (_M0L6_2acntS5160 == 1) {
            #line 1332 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS2017);
          }
          _M0L7_2azsetS2018 = _M0L8_2afieldS4697;
          _M0L4zsetS2008 = _M0L7_2azsetS2018;
          goto join_2007;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS2016);
          goto join_2006;
          break;
        }
      }
    }
    join_2007:;
    #line 1334 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS4260
    = _M0MPB3Map8containsGsfE(_M0L4zsetS2008, _M0L11member__valS2009);
    moonbit_decref(_M0L4zsetS2008);
    if (!_M0L6_2atmpS4260) {
      return 4294967296ll;
    } else {
      struct _M0TPB5ArrayGUsfEE* _M0L6sortedS2010;
      struct _M0TPB8MutLocalGiE* _M0L4rankS2011;
      int32_t _M0L1iS2012;
      int32_t _M0L3valS4265;
      #line 1337 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6sortedS2010
      = _M0MP19moonbitDB8Database17get__sorted__zset(_M0L4selfS2004, _M0L3keyS2005);
      _M0L4rankS2011
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L4rankS2011)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L4rankS2011->$0 = 0;
      _M0L1iS2012 = 0;
      while (1) {
        int32_t _M0L6_2atmpS4261;
        #line 1339 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS4261 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS2010);
        if (_M0L1iS2012 < _M0L6_2atmpS4261) {
          struct _M0TUsfE* _M0L6_2atmpS4263;
          moonbit_string_t _M0L8_2afieldS4696;
          int32_t _M0L6_2acntS5158;
          moonbit_string_t _M0L6_2atmpS4262;
          int32_t _result_5407;
          int32_t _M0L6_2atmpS4264;
          #line 1340 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0L6_2atmpS4263
          = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS2010, _M0L1iS2012);
          _M0L8_2afieldS4696 = _M0L6_2atmpS4263->$0;
          _M0L6_2acntS5158
          = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS4263));
          if (_M0L6_2acntS5158 > 1) {
            int32_t _M0L11_2anew__cntS5159 = _M0L6_2acntS5158 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS4263), _M0L11_2anew__cntS5159);
            moonbit_incref(_M0L8_2afieldS4696);
          } else if (_M0L6_2acntS5158 == 1) {
            #line 1340 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2atmpS4263);
          }
          _M0L6_2atmpS4262 = _M0L8_2afieldS4696;
          #line 1340 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _result_5407
          = _M0L6_2atmpS4262 == _M0L11member__valS2009
            || Moonbit_array_length(_M0L6_2atmpS4262)
               == Moonbit_array_length(_M0L11member__valS2009)
               && 0
                  == memcmp(_M0L6_2atmpS4262, _M0L11member__valS2009, Moonbit_array_length(_M0L6_2atmpS4262) * 2);
          moonbit_decref(_M0L6_2atmpS4262);
          if (_result_5407) {
            moonbit_decref(_M0L6sortedS2010);
            _M0L4rankS2011->$0 = _M0L1iS2012;
            break;
          }
          _M0L6_2atmpS4264 = _M0L1iS2012 + 1;
          _M0L1iS2012 = _M0L6_2atmpS4264;
          continue;
        } else {
          moonbit_decref(_M0L6sortedS2010);
        }
        break;
      }
      _M0L3valS4265 = _M0L4rankS2011->$0;
      moonbit_decref(_M0L4rankS2011);
      return (int64_t)_M0L3valS4265;
    }
    join_2006:;
    return 4294967296ll;
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database9zrevrange(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1993,
  moonbit_string_t _M0L3keyS1994,
  int32_t _M0L5startS1998,
  int32_t _M0L3endS2000
) {
  struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1992;
  int32_t _M0L3lenS1995;
  moonbit_string_t* _M0L6_2atmpS4259;
  struct _M0TPB5ArrayGsE* _M0L6resultS1996;
  int32_t _M0L10start__idxS1997;
  int32_t _M0L8end__idxS1999;
  int32_t _M0L1iS2001;
  #line 1302 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1303 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6sortedS1992
  = _M0MP19moonbitDB8Database17get__sorted__zset(_M0L4selfS1993, _M0L3keyS1994);
  #line 1304 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L3lenS1995 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1992);
  _M0L6_2atmpS4259 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1996
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1996)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6resultS1996->$0 = _M0L6_2atmpS4259;
  _M0L6resultS1996->$1 = 0;
  if (_M0L5startS1998 < 0) {
    _M0L10start__idxS1997 = _M0L3lenS1995 + _M0L5startS1998;
  } else {
    _M0L10start__idxS1997 = _M0L5startS1998;
  }
  if (_M0L3endS2000 < 0) {
    _M0L8end__idxS1999 = _M0L3lenS1995 + _M0L3endS2000;
  } else {
    _M0L8end__idxS1999 = _M0L3endS2000;
  }
  _M0L1iS2001 = _M0L10start__idxS1997;
  while (1) {
    int32_t _if__result_5409;
    if (_M0L1iS2001 <= _M0L8end__idxS1999) {
      _if__result_5409 = _M0L1iS2001 < _M0L3lenS1995;
    } else {
      _if__result_5409 = 0;
    }
    if (_if__result_5409) {
      int32_t _M0L6_2atmpS4257 = _M0L3lenS1995 - 1;
      int32_t _M0L8rev__idxS2002 = _M0L6_2atmpS4257 - _M0L1iS2001;
      int32_t _M0L6_2atmpS4258;
      if (_M0L8rev__idxS2002 >= 0) {
        struct _M0TUsfE* _M0L6_2atmpS4256;
        moonbit_string_t _M0L8_2afieldS4699;
        int32_t _M0L6_2acntS5162;
        moonbit_string_t _M0L6_2atmpS4255;
        #line 1311 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS4256
        = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1992, _M0L8rev__idxS2002);
        _M0L8_2afieldS4699 = _M0L6_2atmpS4256->$0;
        _M0L6_2acntS5162
        = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS4256));
        if (_M0L6_2acntS5162 > 1) {
          int32_t _M0L11_2anew__cntS5163 = _M0L6_2acntS5162 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS4256), _M0L11_2anew__cntS5163);
          moonbit_incref(_M0L8_2afieldS4699);
        } else if (_M0L6_2acntS5162 == 1) {
          #line 1311 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L6_2atmpS4256);
        }
        _M0L6_2atmpS4255 = _M0L8_2afieldS4699;
        #line 1311 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1996, _M0L6_2atmpS4255);
        moonbit_decref(_M0L6_2atmpS4255);
      }
      _M0L6_2atmpS4258 = _M0L1iS2001 + 1;
      _M0L1iS2001 = _M0L6_2atmpS4258;
      continue;
    } else {
      moonbit_decref(_M0L6sortedS1992);
    }
    break;
  }
  return _M0L6resultS1996;
}

struct _M0TPB5ArrayGUsfEE* _M0MP19moonbitDB8Database17get__sorted__zset(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1971,
  moonbit_string_t _M0L3keyS1972
) {
  #line 1263 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1264 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1971, _M0L3keyS1972)
  ) {
    struct _M0TUsfE** _M0L6_2atmpS4250 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _block_5410 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_5410)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _block_5410->$0 = _M0L6_2atmpS4250;
    _block_5410->$1 = 0;
    return _block_5410;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1975;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4254 =
      _M0L4selfS1971->$0;
    void* _M0L7_2abindS1987;
    struct _M0TUsfE** _M0L6_2atmpS4253;
    struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1976;
    struct _M0TPB4IterGUsfEE* _M0L5_2aitS1977;
    struct _M0TPB5ArrayGUsfEE* _result_5415;
    struct _M0TUsfE** _M0L6_2atmpS4251;
    struct _M0TPB5ArrayGUsfEE* _block_5416;
    moonbit_incref(_M0L4dataS4254);
    #line 1267 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1987
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4254, _M0L3keyS1972);
    moonbit_decref(_M0L4dataS4254);
    if (_M0L7_2abindS1987 == 0) {
      if (_M0L7_2abindS1987) {
        moonbit_decref(_M0L7_2abindS1987);
      }
      goto join_1973;
    } else {
      void* _M0L7_2aSomeS1988 = _M0L7_2abindS1987;
      void* _M0L4_2axS1989 = _M0L7_2aSomeS1988;
      switch (Moonbit_object_tag(_M0L4_2axS1989)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1990 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1989;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS4701 = _M0L7_2aZSetS1990->$0;
          int32_t _M0L6_2acntS5166 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1990));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1991;
          if (_M0L6_2acntS5166 > 1) {
            int32_t _M0L11_2anew__cntS5167 = _M0L6_2acntS5166 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1990), _M0L11_2anew__cntS5167);
            moonbit_incref(_M0L8_2afieldS4701);
          } else if (_M0L6_2acntS5166 == 1) {
            #line 1267 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1990);
          }
          _M0L7_2azsetS1991 = _M0L8_2afieldS4701;
          _M0L4zsetS1975 = _M0L7_2azsetS1991;
          goto join_1974;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1989);
          goto join_1973;
          break;
        }
      }
    }
    join_1974:;
    _M0L6_2atmpS4253 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5itemsS1976
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5itemsS1976)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _M0L5itemsS1976->$0 = _M0L6_2atmpS4253;
    _M0L5itemsS1976->$1 = 0;
    #line 1269 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L5_2aitS1977 = _M0MPB3Map5iter2GsfE(_M0L4zsetS1975);
    moonbit_decref(_M0L4zsetS1975);
    while (1) {
      moonbit_string_t _M0L1mS1979;
      float _M0L1sS1980;
      struct _M0TUsfE* _M0L7_2abindS1982;
      struct _M0TUsfE* _M0L8_2atupleS4252;
      #line 1270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L7_2abindS1982 = _M0MPB5Iter24nextGsfE(_M0L5_2aitS1977);
      if (_M0L7_2abindS1982 == 0) {
        if (_M0L7_2abindS1982) {
          moonbit_decref(_M0L7_2abindS1982);
        }
        moonbit_decref(_M0L5_2aitS1977);
      } else {
        struct _M0TUsfE* _M0L7_2aSomeS1983 = _M0L7_2abindS1982;
        struct _M0TUsfE* _M0L4_2axS1984 = _M0L7_2aSomeS1983;
        moonbit_string_t _M0L4_2amS1985 = _M0L4_2axS1984->$0;
        float _M0L4_2asS1986 = _M0L4_2axS1984->$1;
        int32_t _M0L6_2acntS5164 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1984));
        if (_M0L6_2acntS5164 > 1) {
          int32_t _M0L11_2anew__cntS5165 = _M0L6_2acntS5164 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1984), _M0L11_2anew__cntS5165);
          moonbit_incref(_M0L4_2amS1985);
        } else if (_M0L6_2acntS5164 == 1) {
          #line 1270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L4_2axS1984);
        }
        _M0L1mS1979 = _M0L4_2amS1985;
        _M0L1sS1980 = _M0L4_2asS1986;
        goto join_1978;
      }
      goto joinlet_5414;
      join_1978:;
      _M0L8_2atupleS4252
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS4252)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 9, 0);
      _M0L8_2atupleS4252->$0 = _M0L1mS1979;
      _M0L8_2atupleS4252->$1 = _M0L1sS1980;
      #line 1271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L5itemsS1976, _M0L8_2atupleS4252);
      moonbit_decref(_M0L8_2atupleS4252);
      continue;
      joinlet_5414:;
      break;
    }
    #line 1273 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_5415 = _M0FP19moonbitDB15sort__by__score(_M0L5itemsS1976);
    moonbit_decref(_M0L5itemsS1976);
    return _result_5415;
    join_1973:;
    _M0L6_2atmpS4251 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _block_5416
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_5416)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _block_5416->$0 = _M0L6_2atmpS4251;
    _block_5416->$1 = 0;
    return _block_5416;
  }
}

void* _M0MP19moonbitDB8Database6zscore(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1960,
  moonbit_string_t _M0L3keyS1961,
  moonbit_string_t _M0L11member__valS1965
) {
  #line 1234 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1235 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1960, _M0L3keyS1961)
  ) {
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1964;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4249 =
      _M0L4selfS1960->$0;
    void* _M0L7_2abindS1966;
    void* _result_5419;
    moonbit_incref(_M0L4dataS4249);
    #line 1238 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1966
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4249, _M0L3keyS1961);
    moonbit_decref(_M0L4dataS4249);
    if (_M0L7_2abindS1966 == 0) {
      if (_M0L7_2abindS1966) {
        moonbit_decref(_M0L7_2abindS1966);
      }
      goto join_1962;
    } else {
      void* _M0L7_2aSomeS1967 = _M0L7_2abindS1966;
      void* _M0L4_2axS1968 = _M0L7_2aSomeS1967;
      switch (Moonbit_object_tag(_M0L4_2axS1968)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1969 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1968;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS4703 = _M0L7_2aZSetS1969->$0;
          int32_t _M0L6_2acntS5168 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1969));
          struct _M0TPB3MapGsfE* _M0L4_2azS1970;
          if (_M0L6_2acntS5168 > 1) {
            int32_t _M0L11_2anew__cntS5169 = _M0L6_2acntS5168 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1969), _M0L11_2anew__cntS5169);
            moonbit_incref(_M0L8_2afieldS4703);
          } else if (_M0L6_2acntS5168 == 1) {
            #line 1238 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1969);
          }
          _M0L4_2azS1970 = _M0L8_2afieldS4703;
          _M0L1zS1964 = _M0L4_2azS1970;
          goto join_1963;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1968);
          goto join_1962;
          break;
        }
      }
    }
    join_1963:;
    #line 1239 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_5419 = _M0MPB3Map3getGsfE(_M0L1zS1964, _M0L11member__valS1965);
    moonbit_decref(_M0L1zS1964);
    return _result_5419;
    join_1962:;
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
}

int32_t _M0MP19moonbitDB8Database5zcard(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1951,
  moonbit_string_t _M0L3keyS1952
) {
  #line 1223 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1224 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1951, _M0L3keyS1952)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1954;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4248 =
      _M0L4selfS1951->$0;
    void* _M0L7_2abindS1955;
    int32_t _result_5421;
    moonbit_incref(_M0L4dataS4248);
    #line 1227 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1955
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4248, _M0L3keyS1952);
    moonbit_decref(_M0L4dataS4248);
    if (_M0L7_2abindS1955 == 0) {
      if (_M0L7_2abindS1955) {
        moonbit_decref(_M0L7_2abindS1955);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1956 = _M0L7_2abindS1955;
      void* _M0L4_2axS1957 = _M0L7_2aSomeS1956;
      switch (Moonbit_object_tag(_M0L4_2axS1957)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1958 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1957;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS4705 = _M0L7_2aZSetS1958->$0;
          int32_t _M0L6_2acntS5170 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1958));
          struct _M0TPB3MapGsfE* _M0L4_2azS1959;
          if (_M0L6_2acntS5170 > 1) {
            int32_t _M0L11_2anew__cntS5171 = _M0L6_2acntS5170 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1958), _M0L11_2anew__cntS5171);
            moonbit_incref(_M0L8_2afieldS4705);
          } else if (_M0L6_2acntS5170 == 1) {
            #line 1227 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1958);
          }
          _M0L4_2azS1959 = _M0L8_2afieldS4705;
          _M0L1zS1954 = _M0L4_2azS1959;
          goto join_1953;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1957);
          return 0;
          break;
        }
      }
    }
    join_1953:;
    #line 1228 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_5421 = _M0MPB3Map6lengthGsfE(_M0L1zS1954);
    moonbit_decref(_M0L1zS1954);
    return _result_5421;
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database6zrange(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1921,
  moonbit_string_t _M0L3keyS1922,
  int32_t _M0L5startS1941,
  int32_t _M0L3endS1943
) {
  #line 1152 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1153 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1921, _M0L3keyS1922)
  ) {
    moonbit_string_t* _M0L6_2atmpS4239 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_5422 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5422)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5422->$0 = _M0L6_2atmpS4239;
    _block_5422->$1 = 0;
    return _block_5422;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1925;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4247 =
      _M0L4selfS1921->$0;
    void* _M0L7_2abindS1946;
    struct _M0TUsfE** _M0L6_2atmpS4246;
    struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1926;
    struct _M0TPB4IterGUsfEE* _M0L5_2aitS1927;
    struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1937;
    int32_t _M0L3lenS1938;
    moonbit_string_t* _M0L6_2atmpS4245;
    struct _M0TPB5ArrayGsE* _M0L6resultS1939;
    int32_t _M0L10start__idxS1940;
    int32_t _M0L8end__idxS1942;
    int32_t _M0L1iS1944;
    moonbit_string_t* _M0L6_2atmpS4240;
    struct _M0TPB5ArrayGsE* _block_5429;
    moonbit_incref(_M0L4dataS4247);
    #line 1156 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1946
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4247, _M0L3keyS1922);
    moonbit_decref(_M0L4dataS4247);
    if (_M0L7_2abindS1946 == 0) {
      if (_M0L7_2abindS1946) {
        moonbit_decref(_M0L7_2abindS1946);
      }
      goto join_1923;
    } else {
      void* _M0L7_2aSomeS1947 = _M0L7_2abindS1946;
      void* _M0L4_2axS1948 = _M0L7_2aSomeS1947;
      switch (Moonbit_object_tag(_M0L4_2axS1948)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1949 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1948;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS4709 = _M0L7_2aZSetS1949->$0;
          int32_t _M0L6_2acntS5176 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1949));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1950;
          if (_M0L6_2acntS5176 > 1) {
            int32_t _M0L11_2anew__cntS5177 = _M0L6_2acntS5176 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1949), _M0L11_2anew__cntS5177);
            moonbit_incref(_M0L8_2afieldS4709);
          } else if (_M0L6_2acntS5176 == 1) {
            #line 1156 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1949);
          }
          _M0L7_2azsetS1950 = _M0L8_2afieldS4709;
          _M0L4zsetS1925 = _M0L7_2azsetS1950;
          goto join_1924;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1948);
          goto join_1923;
          break;
        }
      }
    }
    join_1924:;
    _M0L6_2atmpS4246 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5itemsS1926
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5itemsS1926)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _M0L5itemsS1926->$0 = _M0L6_2atmpS4246;
    _M0L5itemsS1926->$1 = 0;
    #line 1158 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L5_2aitS1927 = _M0MPB3Map5iter2GsfE(_M0L4zsetS1925);
    moonbit_decref(_M0L4zsetS1925);
    while (1) {
      moonbit_string_t _M0L1mS1929;
      float _M0L1sS1930;
      struct _M0TUsfE* _M0L7_2abindS1932;
      struct _M0TUsfE* _M0L8_2atupleS4241;
      #line 1159 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L7_2abindS1932 = _M0MPB5Iter24nextGsfE(_M0L5_2aitS1927);
      if (_M0L7_2abindS1932 == 0) {
        if (_M0L7_2abindS1932) {
          moonbit_decref(_M0L7_2abindS1932);
        }
        moonbit_decref(_M0L5_2aitS1927);
      } else {
        struct _M0TUsfE* _M0L7_2aSomeS1933 = _M0L7_2abindS1932;
        struct _M0TUsfE* _M0L4_2axS1934 = _M0L7_2aSomeS1933;
        moonbit_string_t _M0L4_2amS1935 = _M0L4_2axS1934->$0;
        float _M0L4_2asS1936 = _M0L4_2axS1934->$1;
        int32_t _M0L6_2acntS5172 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1934));
        if (_M0L6_2acntS5172 > 1) {
          int32_t _M0L11_2anew__cntS5173 = _M0L6_2acntS5172 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1934), _M0L11_2anew__cntS5173);
          moonbit_incref(_M0L4_2amS1935);
        } else if (_M0L6_2acntS5172 == 1) {
          #line 1159 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L4_2axS1934);
        }
        _M0L1mS1929 = _M0L4_2amS1935;
        _M0L1sS1930 = _M0L4_2asS1936;
        goto join_1928;
      }
      goto joinlet_5426;
      join_1928:;
      _M0L8_2atupleS4241
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS4241)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 9, 0);
      _M0L8_2atupleS4241->$0 = _M0L1mS1929;
      _M0L8_2atupleS4241->$1 = _M0L1sS1930;
      #line 1160 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L5itemsS1926, _M0L8_2atupleS4241);
      moonbit_decref(_M0L8_2atupleS4241);
      continue;
      joinlet_5426:;
      break;
    }
    #line 1162 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6sortedS1937 = _M0FP19moonbitDB15sort__by__score(_M0L5itemsS1926);
    moonbit_decref(_M0L5itemsS1926);
    #line 1163 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3lenS1938 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1937);
    _M0L6_2atmpS4245 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1939
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1939)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS1939->$0 = _M0L6_2atmpS4245;
    _M0L6resultS1939->$1 = 0;
    if (_M0L5startS1941 < 0) {
      _M0L10start__idxS1940 = _M0L3lenS1938 + _M0L5startS1941;
    } else {
      _M0L10start__idxS1940 = _M0L5startS1941;
    }
    if (_M0L3endS1943 < 0) {
      _M0L8end__idxS1942 = _M0L3lenS1938 + _M0L3endS1943;
    } else {
      _M0L8end__idxS1942 = _M0L3endS1943;
    }
    _M0L1iS1944 = _M0L10start__idxS1940;
    while (1) {
      int32_t _if__result_5428;
      if (_M0L1iS1944 <= _M0L8end__idxS1942) {
        _if__result_5428 = _M0L1iS1944 < _M0L3lenS1938;
      } else {
        _if__result_5428 = 0;
      }
      if (_if__result_5428) {
        struct _M0TUsfE* _M0L6_2atmpS4243;
        moonbit_string_t _M0L8_2afieldS4707;
        int32_t _M0L6_2acntS5174;
        moonbit_string_t _M0L6_2atmpS4242;
        int32_t _M0L6_2atmpS4244;
        #line 1168 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS4243
        = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1937, _M0L1iS1944);
        _M0L8_2afieldS4707 = _M0L6_2atmpS4243->$0;
        _M0L6_2acntS5174
        = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS4243));
        if (_M0L6_2acntS5174 > 1) {
          int32_t _M0L11_2anew__cntS5175 = _M0L6_2acntS5174 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS4243), _M0L11_2anew__cntS5175);
          moonbit_incref(_M0L8_2afieldS4707);
        } else if (_M0L6_2acntS5174 == 1) {
          #line 1168 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L6_2atmpS4243);
        }
        _M0L6_2atmpS4242 = _M0L8_2afieldS4707;
        #line 1168 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1939, _M0L6_2atmpS4242);
        moonbit_decref(_M0L6_2atmpS4242);
        _M0L6_2atmpS4244 = _M0L1iS1944 + 1;
        _M0L1iS1944 = _M0L6_2atmpS4244;
        continue;
      } else {
        moonbit_decref(_M0L6sortedS1937);
      }
      break;
    }
    return _M0L6resultS1939;
    join_1923:;
    _M0L6_2atmpS4240 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_5429
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5429)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5429->$0 = _M0L6_2atmpS4240;
    _block_5429->$1 = 0;
    return _block_5429;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP19moonbitDB15sort__by__score(
  struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1920
) {
  #line 1177 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1178 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  return _M0FP19moonbitDB11merge__sort(_M0L5itemsS1920);
}

struct _M0TPB5ArrayGUsfEE* _M0FP19moonbitDB11merge__sort(
  struct _M0TPB5ArrayGUsfEE* _M0L3arrS1912
) {
  int32_t _M0L3lenS1911;
  #line 1181 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1182 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L3lenS1911 = _M0MPC15array5Array6lengthGUsfEE(_M0L3arrS1912);
  if (_M0L3lenS1911 <= 1) {
    moonbit_incref(_M0L3arrS1912);
    return _M0L3arrS1912;
  } else {
    int32_t _M0L3midS1913 = _M0L3lenS1911 / 2;
    struct _M0TUsfE** _M0L6_2atmpS4238 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _M0L4leftS1914 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    struct _M0TUsfE** _M0L6_2atmpS4237;
    struct _M0TPB5ArrayGUsfEE* _M0L5rightS1915;
    int32_t _M0L1iS1916;
    int32_t _M0L1iS1918;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS4235;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS4236;
    struct _M0TPB5ArrayGUsfEE* _result_5432;
    Moonbit_object_header(_M0L4leftS1914)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _M0L4leftS1914->$0 = _M0L6_2atmpS4238;
    _M0L4leftS1914->$1 = 0;
    _M0L6_2atmpS4237 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5rightS1915
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5rightS1915)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
    _M0L5rightS1915->$0 = _M0L6_2atmpS4237;
    _M0L5rightS1915->$1 = 0;
    _M0L1iS1916 = 0;
    while (1) {
      if (_M0L1iS1916 < _M0L3midS1913) {
        struct _M0TUsfE* _M0L6_2atmpS4231;
        int32_t _M0L6_2atmpS4232;
        #line 1190 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS4231
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1912, _M0L1iS1916);
        #line 1190 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L4leftS1914, _M0L6_2atmpS4231);
        moonbit_decref(_M0L6_2atmpS4231);
        _M0L6_2atmpS4232 = _M0L1iS1916 + 1;
        _M0L1iS1916 = _M0L6_2atmpS4232;
        continue;
      }
      break;
    }
    _M0L1iS1918 = _M0L3midS1913;
    while (1) {
      if (_M0L1iS1918 < _M0L3lenS1911) {
        struct _M0TUsfE* _M0L6_2atmpS4233;
        int32_t _M0L6_2atmpS4234;
        #line 1193 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS4233
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1912, _M0L1iS1918);
        #line 1193 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L5rightS1915, _M0L6_2atmpS4233);
        moonbit_decref(_M0L6_2atmpS4233);
        _M0L6_2atmpS4234 = _M0L1iS1918 + 1;
        _M0L1iS1918 = _M0L6_2atmpS4234;
        continue;
      }
      break;
    }
    #line 1195 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS4235 = _M0FP19moonbitDB11merge__sort(_M0L4leftS1914);
    moonbit_decref(_M0L4leftS1914);
    #line 1195 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS4236 = _M0FP19moonbitDB11merge__sort(_M0L5rightS1915);
    moonbit_decref(_M0L5rightS1915);
    #line 1195 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_5432 = _M0FP19moonbitDB5merge(_M0L6_2atmpS4235, _M0L6_2atmpS4236);
    moonbit_decref(_M0L6_2atmpS4235);
    moonbit_decref(_M0L6_2atmpS4236);
    return _result_5432;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP19moonbitDB5merge(
  struct _M0TPB5ArrayGUsfEE* _M0L4leftS1906,
  struct _M0TPB5ArrayGUsfEE* _M0L5rightS1907
) {
  struct _M0TUsfE** _M0L6_2atmpS4230;
  struct _M0TPB5ArrayGUsfEE* _M0L6resultS1903;
  struct _M0TPB8MutLocalGiE* _M0L1iS1904;
  struct _M0TPB8MutLocalGiE* _M0L1jS1905;
  #line 1199 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4230 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6resultS1903
  = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
  Moonbit_object_header(_M0L6resultS1903)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
  _M0L6resultS1903->$0 = _M0L6_2atmpS4230;
  _M0L6resultS1903->$1 = 0;
  _M0L1iS1904
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1904)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS1904->$0 = 0;
  _M0L1jS1905
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1jS1905)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1jS1905->$0 = 0;
  while (1) {
    int32_t _M0L3valS4202 = _M0L1iS1904->$0;
    int32_t _M0L6_2atmpS4203;
    int32_t _if__result_5434;
    #line 1203 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS4203 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1906);
    if (_M0L3valS4202 < _M0L6_2atmpS4203) {
      int32_t _M0L3valS4200 = _M0L1jS1905->$0;
      int32_t _M0L6_2atmpS4201;
      #line 1203 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS4201 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1907);
      _if__result_5434 = _M0L3valS4200 < _M0L6_2atmpS4201;
    } else {
      _if__result_5434 = 0;
    }
    if (_if__result_5434) {
      int32_t _M0L3valS4209 = _M0L1iS1904->$0;
      struct _M0TUsfE* _M0L6_2atmpS4208;
      float _M0L6_2atmpS4204;
      int32_t _M0L3valS4207;
      struct _M0TUsfE* _M0L6_2atmpS4206;
      float _M0L6_2atmpS4205;
      #line 1204 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS4208
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1906, _M0L3valS4209);
      _M0L6_2atmpS4204 = _M0L6_2atmpS4208->$1;
      moonbit_decref(_M0L6_2atmpS4208);
      _M0L3valS4207 = _M0L1jS1905->$0;
      #line 1204 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS4206
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1907, _M0L3valS4207);
      _M0L6_2atmpS4205 = _M0L6_2atmpS4206->$1;
      moonbit_decref(_M0L6_2atmpS4206);
      if (_M0L6_2atmpS4204 <= _M0L6_2atmpS4205) {
        int32_t _M0L3valS4211 = _M0L1iS1904->$0;
        struct _M0TUsfE* _M0L6_2atmpS4210;
        int32_t _M0L3valS4213;
        int32_t _M0L6_2atmpS4212;
        #line 1205 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS4210
        = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1906, _M0L3valS4211);
        #line 1205 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1903, _M0L6_2atmpS4210);
        moonbit_decref(_M0L6_2atmpS4210);
        _M0L3valS4213 = _M0L1iS1904->$0;
        _M0L6_2atmpS4212 = _M0L3valS4213 + 1;
        _M0L1iS1904->$0 = _M0L6_2atmpS4212;
      } else {
        int32_t _M0L3valS4215 = _M0L1jS1905->$0;
        struct _M0TUsfE* _M0L6_2atmpS4214;
        int32_t _M0L3valS4217;
        int32_t _M0L6_2atmpS4216;
        #line 1208 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS4214
        = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1907, _M0L3valS4215);
        #line 1208 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1903, _M0L6_2atmpS4214);
        moonbit_decref(_M0L6_2atmpS4214);
        _M0L3valS4217 = _M0L1jS1905->$0;
        _M0L6_2atmpS4216 = _M0L3valS4217 + 1;
        _M0L1jS1905->$0 = _M0L6_2atmpS4216;
      }
      continue;
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS4218 = _M0L1iS1904->$0;
    int32_t _M0L6_2atmpS4219;
    #line 1212 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS4219 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1906);
    if (_M0L3valS4218 < _M0L6_2atmpS4219) {
      int32_t _M0L3valS4221 = _M0L1iS1904->$0;
      struct _M0TUsfE* _M0L6_2atmpS4220;
      int32_t _M0L3valS4223;
      int32_t _M0L6_2atmpS4222;
      #line 1213 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS4220
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1906, _M0L3valS4221);
      #line 1213 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1903, _M0L6_2atmpS4220);
      moonbit_decref(_M0L6_2atmpS4220);
      _M0L3valS4223 = _M0L1iS1904->$0;
      _M0L6_2atmpS4222 = _M0L3valS4223 + 1;
      _M0L1iS1904->$0 = _M0L6_2atmpS4222;
      continue;
    } else {
      moonbit_decref(_M0L1iS1904);
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS4224 = _M0L1jS1905->$0;
    int32_t _M0L6_2atmpS4225;
    #line 1216 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS4225 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1907);
    if (_M0L3valS4224 < _M0L6_2atmpS4225) {
      int32_t _M0L3valS4227 = _M0L1jS1905->$0;
      struct _M0TUsfE* _M0L6_2atmpS4226;
      int32_t _M0L3valS4229;
      int32_t _M0L6_2atmpS4228;
      #line 1217 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS4226
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1907, _M0L3valS4227);
      #line 1217 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1903, _M0L6_2atmpS4226);
      moonbit_decref(_M0L6_2atmpS4226);
      _M0L3valS4229 = _M0L1jS1905->$0;
      _M0L6_2atmpS4228 = _M0L3valS4229 + 1;
      _M0L1jS1905->$0 = _M0L6_2atmpS4228;
      continue;
    } else {
      moonbit_decref(_M0L1jS1905);
    }
    break;
  }
  return _M0L6resultS1903;
}

int32_t _M0MP19moonbitDB8Database4zadd(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1888,
  moonbit_string_t _M0L3keyS1889,
  float _M0L5scoreS1902,
  moonbit_string_t _M0L11member__valS1901
) {
  int32_t _M0L6_2atmpS4194;
  struct _M0TPB3MapGsfE* _M0L4zsetS1890;
  struct _M0TPB3MapGsfE* _M0L1zS1894;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4199;
  void* _M0L7_2abindS1895;
  struct _M0TUsfE** _M0L7_2abindS1892;
  struct _M0TUsfE** _M0L6_2atmpS4198;
  struct _M0TPB9ArrayViewGUsfEE _M0L6_2atmpS4197;
  int32_t _M0L7existedS1900;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4195;
  void* _M0L4ZSetS4196;
  #line 1140 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1141 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4194
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1888, _M0L3keyS1889);
  _M0L4dataS4199 = _M0L4selfS1888->$0;
  moonbit_incref(_M0L4dataS4199);
  #line 1142 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1895
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4199, _M0L3keyS1889);
  moonbit_decref(_M0L4dataS4199);
  if (_M0L7_2abindS1895 == 0) {
    if (_M0L7_2abindS1895) {
      moonbit_decref(_M0L7_2abindS1895);
    }
    goto join_1891;
  } else {
    void* _M0L7_2aSomeS1896 = _M0L7_2abindS1895;
    void* _M0L4_2axS1897 = _M0L7_2aSomeS1896;
    switch (Moonbit_object_tag(_M0L4_2axS1897)) {
      case 4: {
        struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1898 =
          (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1897;
        struct _M0TPB3MapGsfE* _M0L8_2afieldS4712 = _M0L7_2aZSetS1898->$0;
        int32_t _M0L6_2acntS5178 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1898));
        struct _M0TPB3MapGsfE* _M0L4_2azS1899;
        if (_M0L6_2acntS5178 > 1) {
          int32_t _M0L11_2anew__cntS5179 = _M0L6_2acntS5178 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1898), _M0L11_2anew__cntS5179);
          moonbit_incref(_M0L8_2afieldS4712);
        } else if (_M0L6_2acntS5178 == 1) {
          #line 1142 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aZSetS1898);
        }
        _M0L4_2azS1899 = _M0L8_2afieldS4712;
        _M0L1zS1894 = _M0L4_2azS1899;
        goto join_1893;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1897);
        goto join_1891;
        break;
      }
    }
  }
  goto joinlet_5438;
  join_1893:;
  _M0L4zsetS1890 = _M0L1zS1894;
  joinlet_5438:;
  goto joinlet_5437;
  join_1891:;
  _M0L7_2abindS1892 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6_2atmpS4198 = _M0L7_2abindS1892;
  _M0L6_2atmpS4197
  = (struct _M0TPB9ArrayViewGUsfEE){
    .$0 = _M0L6_2atmpS4198, .$1 = 0, .$2 = 0
  };
  #line 1144 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4zsetS1890 = _M0MPB3Map3MapGsfE(_M0L6_2atmpS4197, 10ll);
  moonbit_decref(_M0L6_2atmpS4197.$0);
  joinlet_5437:;
  #line 1146 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7existedS1900
  = _M0MPB3Map8containsGsfE(_M0L4zsetS1890, _M0L11member__valS1901);
  #line 1147 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsfE(_M0L4zsetS1890, _M0L11member__valS1901, _M0L5scoreS1902);
  _M0L4dataS4195 = _M0L4selfS1888->$0;
  _M0L4ZSetS4196
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4ZSet));
  Moonbit_object_header(_M0L4ZSetS4196)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 4);
  ((struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4ZSetS4196)->$0
  = _M0L4zsetS1890;
  moonbit_incref(_M0L4dataS4195);
  #line 1148 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4195, _M0L3keyS1889, _M0L4ZSetS4196);
  moonbit_decref(_M0L4dataS4195);
  moonbit_decref(_M0L4ZSetS4196);
  return !_M0L7existedS1900;
}

int32_t _M0MP19moonbitDB8Database9sismember(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1878,
  moonbit_string_t _M0L3keyS1879,
  moonbit_string_t _M0L5valueS1882
) {
  #line 963 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 964 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1878, _M0L3keyS1879)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1881;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4193 =
      _M0L4selfS1878->$0;
    void* _M0L7_2abindS1883;
    int32_t _result_5440;
    moonbit_incref(_M0L4dataS4193);
    #line 967 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1883
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4193, _M0L3keyS1879);
    moonbit_decref(_M0L4dataS4193);
    if (_M0L7_2abindS1883 == 0) {
      if (_M0L7_2abindS1883) {
        moonbit_decref(_M0L7_2abindS1883);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1884 = _M0L7_2abindS1883;
      void* _M0L4_2axS1885 = _M0L7_2aSomeS1884;
      switch (Moonbit_object_tag(_M0L4_2axS1885)) {
        case 3: {
          struct _M0DTP19moonbitDB10RedisValue3Set* _M0L6_2aSetS1886 =
            (struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L4_2axS1885;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS4714 = _M0L6_2aSetS1886->$0;
          int32_t _M0L6_2acntS5180 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1886));
          struct _M0TPB3MapGsbE* _M0L4_2asS1887;
          if (_M0L6_2acntS5180 > 1) {
            int32_t _M0L11_2anew__cntS5181 = _M0L6_2acntS5180 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1886), _M0L11_2anew__cntS5181);
            moonbit_incref(_M0L8_2afieldS4714);
          } else if (_M0L6_2acntS5180 == 1) {
            #line 967 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2aSetS1886);
          }
          _M0L4_2asS1887 = _M0L8_2afieldS4714;
          _M0L1sS1881 = _M0L4_2asS1887;
          goto join_1880;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1885);
          return 0;
          break;
        }
      }
    }
    join_1880:;
    #line 968 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_5440 = _M0MPB3Map8containsGsbE(_M0L1sS1881, _M0L5valueS1882);
    moonbit_decref(_M0L1sS1881);
    return _result_5440;
  }
}

int32_t _M0MP19moonbitDB8Database5scard(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1869,
  moonbit_string_t _M0L3keyS1870
) {
  #line 952 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 953 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1869, _M0L3keyS1870)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1872;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4192 =
      _M0L4selfS1869->$0;
    void* _M0L7_2abindS1873;
    int32_t _result_5442;
    moonbit_incref(_M0L4dataS4192);
    #line 956 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1873
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4192, _M0L3keyS1870);
    moonbit_decref(_M0L4dataS4192);
    if (_M0L7_2abindS1873 == 0) {
      if (_M0L7_2abindS1873) {
        moonbit_decref(_M0L7_2abindS1873);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1874 = _M0L7_2abindS1873;
      void* _M0L4_2axS1875 = _M0L7_2aSomeS1874;
      switch (Moonbit_object_tag(_M0L4_2axS1875)) {
        case 3: {
          struct _M0DTP19moonbitDB10RedisValue3Set* _M0L6_2aSetS1876 =
            (struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L4_2axS1875;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS4716 = _M0L6_2aSetS1876->$0;
          int32_t _M0L6_2acntS5182 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1876));
          struct _M0TPB3MapGsbE* _M0L4_2asS1877;
          if (_M0L6_2acntS5182 > 1) {
            int32_t _M0L11_2anew__cntS5183 = _M0L6_2acntS5182 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1876), _M0L11_2anew__cntS5183);
            moonbit_incref(_M0L8_2afieldS4716);
          } else if (_M0L6_2acntS5182 == 1) {
            #line 956 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2aSetS1876);
          }
          _M0L4_2asS1877 = _M0L8_2afieldS4716;
          _M0L1sS1872 = _M0L4_2asS1877;
          goto join_1871;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1875);
          return 0;
          break;
        }
      }
    }
    join_1871:;
    #line 957 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_5442 = _M0MPB3Map6lengthGsbE(_M0L1sS1872);
    moonbit_decref(_M0L1sS1872);
    return _result_5442;
  }
}

int32_t _M0MP19moonbitDB8Database4srem(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1858,
  moonbit_string_t _M0L3keyS1859,
  moonbit_string_t _M0L5valueS1863
) {
  #line 934 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 935 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1858, _M0L3keyS1859)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsbE* _M0L3setS1861;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4191 =
      _M0L4selfS1858->$0;
    void* _M0L7_2abindS1864;
    int32_t _M0L7existedS1862;
    moonbit_incref(_M0L4dataS4191);
    #line 938 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1864
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4191, _M0L3keyS1859);
    moonbit_decref(_M0L4dataS4191);
    if (_M0L7_2abindS1864 == 0) {
      if (_M0L7_2abindS1864) {
        moonbit_decref(_M0L7_2abindS1864);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1865 = _M0L7_2abindS1864;
      void* _M0L4_2axS1866 = _M0L7_2aSomeS1865;
      switch (Moonbit_object_tag(_M0L4_2axS1866)) {
        case 3: {
          struct _M0DTP19moonbitDB10RedisValue3Set* _M0L6_2aSetS1867 =
            (struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L4_2axS1866;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS4719 = _M0L6_2aSetS1867->$0;
          int32_t _M0L6_2acntS5184 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1867));
          struct _M0TPB3MapGsbE* _M0L6_2asetS1868;
          if (_M0L6_2acntS5184 > 1) {
            int32_t _M0L11_2anew__cntS5185 = _M0L6_2acntS5184 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1867), _M0L11_2anew__cntS5185);
            moonbit_incref(_M0L8_2afieldS4719);
          } else if (_M0L6_2acntS5184 == 1) {
            #line 938 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2aSetS1867);
          }
          _M0L6_2asetS1868 = _M0L8_2afieldS4719;
          _M0L3setS1861 = _M0L6_2asetS1868;
          goto join_1860;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1866);
          return 0;
          break;
        }
      }
    }
    join_1860:;
    #line 940 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7existedS1862
    = _M0MPB3Map8containsGsbE(_M0L3setS1861, _M0L5valueS1863);
    if (_M0L7existedS1862) {
      struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4189;
      void* _M0L3SetS4190;
      #line 942 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map6removeGsbE(_M0L3setS1861, _M0L5valueS1863);
      _M0L4dataS4189 = _M0L4selfS1858->$0;
      _M0L3SetS4190
      = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue3Set));
      Moonbit_object_header(_M0L3SetS4190)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 12, 3);
      ((struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L3SetS4190)->$0
      = _M0L3setS1861;
      moonbit_incref(_M0L4dataS4189);
      #line 943 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4189, _M0L3keyS1859, _M0L3SetS4190);
      moonbit_decref(_M0L4dataS4189);
      moonbit_decref(_M0L3SetS4190);
    } else {
      moonbit_decref(_M0L3setS1861);
    }
    return _M0L7existedS1862;
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database8smembers(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1839,
  moonbit_string_t _M0L3keyS1840
) {
  #line 917 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 918 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1839, _M0L3keyS1840)
  ) {
    moonbit_string_t* _M0L6_2atmpS4185 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_5444 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5444)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5444->$0 = _M0L6_2atmpS4185;
    _block_5444->$1 = 0;
    return _block_5444;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1843;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4188 =
      _M0L4selfS1839->$0;
    void* _M0L7_2abindS1853;
    moonbit_string_t* _M0L6_2atmpS4187;
    struct _M0TPB5ArrayGsE* _M0L6resultS1844;
    struct _M0TPB4IterGUsbEE* _M0L5_2aitS1845;
    moonbit_string_t* _M0L6_2atmpS4186;
    struct _M0TPB5ArrayGsE* _block_5449;
    moonbit_incref(_M0L4dataS4188);
    #line 921 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1853
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4188, _M0L3keyS1840);
    moonbit_decref(_M0L4dataS4188);
    if (_M0L7_2abindS1853 == 0) {
      if (_M0L7_2abindS1853) {
        moonbit_decref(_M0L7_2abindS1853);
      }
      goto join_1841;
    } else {
      void* _M0L7_2aSomeS1854 = _M0L7_2abindS1853;
      void* _M0L4_2axS1855 = _M0L7_2aSomeS1854;
      switch (Moonbit_object_tag(_M0L4_2axS1855)) {
        case 3: {
          struct _M0DTP19moonbitDB10RedisValue3Set* _M0L6_2aSetS1856 =
            (struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L4_2axS1855;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS4722 = _M0L6_2aSetS1856->$0;
          int32_t _M0L6_2acntS5188 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1856));
          struct _M0TPB3MapGsbE* _M0L4_2asS1857;
          if (_M0L6_2acntS5188 > 1) {
            int32_t _M0L11_2anew__cntS5189 = _M0L6_2acntS5188 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1856), _M0L11_2anew__cntS5189);
            moonbit_incref(_M0L8_2afieldS4722);
          } else if (_M0L6_2acntS5188 == 1) {
            #line 921 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2aSetS1856);
          }
          _M0L4_2asS1857 = _M0L8_2afieldS4722;
          _M0L1sS1843 = _M0L4_2asS1857;
          goto join_1842;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1855);
          goto join_1841;
          break;
        }
      }
    }
    join_1842:;
    _M0L6_2atmpS4187 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1844
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1844)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS1844->$0 = _M0L6_2atmpS4187;
    _M0L6resultS1844->$1 = 0;
    #line 923 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L5_2aitS1845 = _M0MPB3Map5iter2GsbE(_M0L1sS1843);
    moonbit_decref(_M0L1sS1843);
    while (1) {
      moonbit_string_t _M0L1mS1847;
      struct _M0TUsbE* _M0L7_2abindS1849;
      #line 924 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L7_2abindS1849 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1845);
      if (_M0L7_2abindS1849 == 0) {
        if (_M0L7_2abindS1849) {
          moonbit_decref(_M0L7_2abindS1849);
        }
        moonbit_decref(_M0L5_2aitS1845);
      } else {
        struct _M0TUsbE* _M0L7_2aSomeS1850 = _M0L7_2abindS1849;
        struct _M0TUsbE* _M0L4_2axS1851 = _M0L7_2aSomeS1850;
        moonbit_string_t _M0L8_2afieldS4721 = _M0L4_2axS1851->$0;
        int32_t _M0L6_2acntS5186 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1851));
        moonbit_string_t _M0L4_2amS1852;
        if (_M0L6_2acntS5186 > 1) {
          int32_t _M0L11_2anew__cntS5187 = _M0L6_2acntS5186 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1851), _M0L11_2anew__cntS5187);
          moonbit_incref(_M0L8_2afieldS4721);
        } else if (_M0L6_2acntS5186 == 1) {
          #line 924 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L4_2axS1851);
        }
        _M0L4_2amS1852 = _M0L8_2afieldS4721;
        _M0L1mS1847 = _M0L4_2amS1852;
        goto join_1846;
      }
      goto joinlet_5448;
      join_1846:;
      #line 925 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1844, _M0L1mS1847);
      moonbit_decref(_M0L1mS1847);
      continue;
      joinlet_5448:;
      break;
    }
    return _M0L6resultS1844;
    join_1841:;
    _M0L6_2atmpS4186 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_5449
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5449)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5449->$0 = _M0L6_2atmpS4186;
    _block_5449->$1 = 0;
    return _block_5449;
  }
}

int32_t _M0MP19moonbitDB8Database4sadd(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1826,
  moonbit_string_t _M0L3keyS1827,
  moonbit_string_t _M0L5valueS1838
) {
  int32_t _M0L6_2atmpS4179;
  struct _M0TPB3MapGsbE* _M0L3setS1828;
  struct _M0TPB3MapGsbE* _M0L1sS1832;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4184;
  void* _M0L7_2abindS1833;
  struct _M0TUsbE** _M0L7_2abindS1830;
  struct _M0TUsbE** _M0L6_2atmpS4183;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS4182;
  #line 902 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 903 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4179
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1826, _M0L3keyS1827);
  _M0L4dataS4184 = _M0L4selfS1826->$0;
  moonbit_incref(_M0L4dataS4184);
  #line 904 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1833
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4184, _M0L3keyS1827);
  moonbit_decref(_M0L4dataS4184);
  if (_M0L7_2abindS1833 == 0) {
    if (_M0L7_2abindS1833) {
      moonbit_decref(_M0L7_2abindS1833);
    }
    goto join_1829;
  } else {
    void* _M0L7_2aSomeS1834 = _M0L7_2abindS1833;
    void* _M0L4_2axS1835 = _M0L7_2aSomeS1834;
    switch (Moonbit_object_tag(_M0L4_2axS1835)) {
      case 3: {
        struct _M0DTP19moonbitDB10RedisValue3Set* _M0L6_2aSetS1836 =
          (struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L4_2axS1835;
        struct _M0TPB3MapGsbE* _M0L8_2afieldS4725 = _M0L6_2aSetS1836->$0;
        int32_t _M0L6_2acntS5190 =
          Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1836));
        struct _M0TPB3MapGsbE* _M0L4_2asS1837;
        if (_M0L6_2acntS5190 > 1) {
          int32_t _M0L11_2anew__cntS5191 = _M0L6_2acntS5190 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1836), _M0L11_2anew__cntS5191);
          moonbit_incref(_M0L8_2afieldS4725);
        } else if (_M0L6_2acntS5190 == 1) {
          #line 904 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L6_2aSetS1836);
        }
        _M0L4_2asS1837 = _M0L8_2afieldS4725;
        _M0L1sS1832 = _M0L4_2asS1837;
        goto join_1831;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1835);
        goto join_1829;
        break;
      }
    }
  }
  goto joinlet_5451;
  join_1831:;
  _M0L3setS1828 = _M0L1sS1832;
  joinlet_5451:;
  goto joinlet_5450;
  join_1829:;
  _M0L7_2abindS1830 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS4183 = _M0L7_2abindS1830;
  _M0L6_2atmpS4182
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS4183, .$1 = 0, .$2 = 0
  };
  #line 906 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L3setS1828 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS4182, 10ll);
  moonbit_decref(_M0L6_2atmpS4182.$0);
  joinlet_5450:;
  #line 908 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (_M0MPB3Map8containsGsbE(_M0L3setS1828, _M0L5valueS1838)) {
    moonbit_decref(_M0L3setS1828);
    return 0;
  } else {
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4180;
    void* _M0L3SetS4181;
    #line 911 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsbE(_M0L3setS1828, _M0L5valueS1838, 1);
    _M0L4dataS4180 = _M0L4selfS1826->$0;
    _M0L3SetS4181
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue3Set));
    Moonbit_object_header(_M0L3SetS4181)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 12, 3);
    ((struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L3SetS4181)->$0
    = _M0L3setS1828;
    moonbit_incref(_M0L4dataS4180);
    #line 912 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4180, _M0L3keyS1827, _M0L3SetS4181);
    moonbit_decref(_M0L4dataS4180);
    moonbit_decref(_M0L3SetS4181);
    return 1;
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database6lrange(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1807,
  moonbit_string_t _M0L3keyS1808,
  int32_t _M0L5startS1815,
  int32_t _M0L3endS1817
) {
  #line 720 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 721 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1807, _M0L3keyS1808)
  ) {
    moonbit_string_t* _M0L6_2atmpS4173 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_5452 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5452)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5452->$0 = _M0L6_2atmpS4173;
    _block_5452->$1 = 0;
    return _block_5452;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L5dequeS1811;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4178 =
      _M0L4selfS1807->$0;
    void* _M0L7_2abindS1821;
    struct _M0TPB5ArrayGsE* _M0L3arrS1812;
    int32_t _M0L3lenS1813;
    int32_t _M0L10start__idxS1814;
    int32_t _M0L8end__idxS1816;
    moonbit_string_t* _M0L6_2atmpS4177;
    struct _M0TPB5ArrayGsE* _M0L6resultS1818;
    int32_t _M0L1iS1819;
    moonbit_string_t* _M0L6_2atmpS4174;
    struct _M0TPB5ArrayGsE* _block_5457;
    moonbit_incref(_M0L4dataS4178);
    #line 724 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1821
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4178, _M0L3keyS1808);
    moonbit_decref(_M0L4dataS4178);
    if (_M0L7_2abindS1821 == 0) {
      if (_M0L7_2abindS1821) {
        moonbit_decref(_M0L7_2abindS1821);
      }
      goto join_1809;
    } else {
      void* _M0L7_2aSomeS1822 = _M0L7_2abindS1821;
      void* _M0L4_2axS1823 = _M0L7_2aSomeS1822;
      switch (Moonbit_object_tag(_M0L4_2axS1823)) {
        case 2: {
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1824 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1823;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS4727 =
            _M0L7_2aListS1824->$0;
          int32_t _M0L6_2acntS5192 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1824));
          struct _M0TP19moonbitDB5Deque* _M0L8_2adequeS1825;
          if (_M0L6_2acntS5192 > 1) {
            int32_t _M0L11_2anew__cntS5193 = _M0L6_2acntS5192 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1824), _M0L11_2anew__cntS5193);
            moonbit_incref(_M0L8_2afieldS4727);
          } else if (_M0L6_2acntS5192 == 1) {
            #line 724 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1824);
          }
          _M0L8_2adequeS1825 = _M0L8_2afieldS4727;
          _M0L5dequeS1811 = _M0L8_2adequeS1825;
          goto join_1810;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1823);
          goto join_1809;
          break;
        }
      }
    }
    join_1810:;
    #line 726 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3arrS1812 = _M0MP19moonbitDB5Deque9to__array(_M0L5dequeS1811);
    moonbit_decref(_M0L5dequeS1811);
    #line 727 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3lenS1813 = _M0MPC15array5Array6lengthGsE(_M0L3arrS1812);
    if (_M0L5startS1815 < 0) {
      _M0L10start__idxS1814 = _M0L3lenS1813 + _M0L5startS1815;
    } else {
      _M0L10start__idxS1814 = _M0L5startS1815;
    }
    if (_M0L3endS1817 < 0) {
      _M0L8end__idxS1816 = _M0L3lenS1813 + _M0L3endS1817;
    } else {
      _M0L8end__idxS1816 = _M0L3endS1817;
    }
    _M0L6_2atmpS4177 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1818
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1818)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS1818->$0 = _M0L6_2atmpS4177;
    _M0L6resultS1818->$1 = 0;
    _M0L1iS1819 = _M0L10start__idxS1814;
    while (1) {
      int32_t _if__result_5456;
      if (_M0L1iS1819 <= _M0L8end__idxS1816) {
        if (_M0L1iS1819 >= 0) {
          _if__result_5456 = _M0L1iS1819 < _M0L3lenS1813;
        } else {
          _if__result_5456 = 0;
        }
      } else {
        _if__result_5456 = 0;
      }
      if (_if__result_5456) {
        moonbit_string_t _M0L6_2atmpS4175;
        int32_t _M0L6_2atmpS4176;
        #line 732 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS4175
        = _M0MPC15array5Array2atGsE(_M0L3arrS1812, _M0L1iS1819);
        #line 732 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1818, _M0L6_2atmpS4175);
        moonbit_decref(_M0L6_2atmpS4175);
        _M0L6_2atmpS4176 = _M0L1iS1819 + 1;
        _M0L1iS1819 = _M0L6_2atmpS4176;
        continue;
      } else {
        moonbit_decref(_M0L3arrS1812);
      }
      break;
    }
    return _M0L6resultS1818;
    join_1809:;
    _M0L6_2atmpS4174 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_5457
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_5457)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_5457->$0 = _M0L6_2atmpS4174;
    _block_5457->$1 = 0;
    return _block_5457;
  }
}

int32_t _M0MP19moonbitDB8Database4llen(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1798,
  moonbit_string_t _M0L3keyS1799
) {
  #line 709 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 710 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1798, _M0L3keyS1799)
  ) {
    return 0;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L1dS1801;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4172 =
      _M0L4selfS1798->$0;
    void* _M0L7_2abindS1802;
    int32_t _result_5459;
    moonbit_incref(_M0L4dataS4172);
    #line 713 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1802
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4172, _M0L3keyS1799);
    moonbit_decref(_M0L4dataS4172);
    if (_M0L7_2abindS1802 == 0) {
      if (_M0L7_2abindS1802) {
        moonbit_decref(_M0L7_2abindS1802);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1803 = _M0L7_2abindS1802;
      void* _M0L4_2axS1804 = _M0L7_2aSomeS1803;
      switch (Moonbit_object_tag(_M0L4_2axS1804)) {
        case 2: {
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1805 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1804;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS4729 =
            _M0L7_2aListS1805->$0;
          int32_t _M0L6_2acntS5194 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1805));
          struct _M0TP19moonbitDB5Deque* _M0L4_2adS1806;
          if (_M0L6_2acntS5194 > 1) {
            int32_t _M0L11_2anew__cntS5195 = _M0L6_2acntS5194 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1805), _M0L11_2anew__cntS5195);
            moonbit_incref(_M0L8_2afieldS4729);
          } else if (_M0L6_2acntS5194 == 1) {
            #line 713 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1805);
          }
          _M0L4_2adS1806 = _M0L8_2afieldS4729;
          _M0L1dS1801 = _M0L4_2adS1806;
          goto join_1800;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1804);
          return 0;
          break;
        }
      }
    }
    join_1800:;
    #line 714 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_5459 = _M0MP19moonbitDB5Deque6length(_M0L1dS1801);
    moonbit_decref(_M0L1dS1801);
    return _result_5459;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database4rpop(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1787,
  moonbit_string_t _M0L3keyS1788
) {
  #line 694 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 695 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1787, _M0L3keyS1788)
  ) {
    return 0;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L5dequeS1791;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4171 =
      _M0L4selfS1787->$0;
    void* _M0L7_2abindS1793;
    moonbit_string_t _M0L3valS1792;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4169;
    void* _M0L4ListS4170;
    moonbit_incref(_M0L4dataS4171);
    #line 698 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1793
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4171, _M0L3keyS1788);
    moonbit_decref(_M0L4dataS4171);
    if (_M0L7_2abindS1793 == 0) {
      if (_M0L7_2abindS1793) {
        moonbit_decref(_M0L7_2abindS1793);
      }
      goto join_1789;
    } else {
      void* _M0L7_2aSomeS1794 = _M0L7_2abindS1793;
      void* _M0L4_2axS1795 = _M0L7_2aSomeS1794;
      switch (Moonbit_object_tag(_M0L4_2axS1795)) {
        case 2: {
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1796 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1795;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS4732 =
            _M0L7_2aListS1796->$0;
          int32_t _M0L6_2acntS5196 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1796));
          struct _M0TP19moonbitDB5Deque* _M0L8_2adequeS1797;
          if (_M0L6_2acntS5196 > 1) {
            int32_t _M0L11_2anew__cntS5197 = _M0L6_2acntS5196 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1796), _M0L11_2anew__cntS5197);
            moonbit_incref(_M0L8_2afieldS4732);
          } else if (_M0L6_2acntS5196 == 1) {
            #line 698 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1796);
          }
          _M0L8_2adequeS1797 = _M0L8_2afieldS4732;
          _M0L5dequeS1791 = _M0L8_2adequeS1797;
          goto join_1790;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1795);
          goto join_1789;
          break;
        }
      }
    }
    join_1790:;
    #line 700 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3valS1792 = _M0MP19moonbitDB5Deque9pop__back(_M0L5dequeS1791);
    _M0L4dataS4169 = _M0L4selfS1787->$0;
    _M0L4ListS4170
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4List));
    Moonbit_object_header(_M0L4ListS4170)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
    ((struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4ListS4170)->$0
    = _M0L5dequeS1791;
    moonbit_incref(_M0L4dataS4169);
    #line 701 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4169, _M0L3keyS1788, _M0L4ListS4170);
    moonbit_decref(_M0L4dataS4169);
    moonbit_decref(_M0L4ListS4170);
    return _M0L3valS1792;
    join_1789:;
    return 0;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database4lpop(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1776,
  moonbit_string_t _M0L3keyS1777
) {
  #line 679 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 680 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1776, _M0L3keyS1777)
  ) {
    return 0;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L5dequeS1780;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4168 =
      _M0L4selfS1776->$0;
    void* _M0L7_2abindS1782;
    moonbit_string_t _M0L3valS1781;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4166;
    void* _M0L4ListS4167;
    moonbit_incref(_M0L4dataS4168);
    #line 683 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1782
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4168, _M0L3keyS1777);
    moonbit_decref(_M0L4dataS4168);
    if (_M0L7_2abindS1782 == 0) {
      if (_M0L7_2abindS1782) {
        moonbit_decref(_M0L7_2abindS1782);
      }
      goto join_1778;
    } else {
      void* _M0L7_2aSomeS1783 = _M0L7_2abindS1782;
      void* _M0L4_2axS1784 = _M0L7_2aSomeS1783;
      switch (Moonbit_object_tag(_M0L4_2axS1784)) {
        case 2: {
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1785 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1784;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS4735 =
            _M0L7_2aListS1785->$0;
          int32_t _M0L6_2acntS5198 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1785));
          struct _M0TP19moonbitDB5Deque* _M0L8_2adequeS1786;
          if (_M0L6_2acntS5198 > 1) {
            int32_t _M0L11_2anew__cntS5199 = _M0L6_2acntS5198 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1785), _M0L11_2anew__cntS5199);
            moonbit_incref(_M0L8_2afieldS4735);
          } else if (_M0L6_2acntS5198 == 1) {
            #line 683 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1785);
          }
          _M0L8_2adequeS1786 = _M0L8_2afieldS4735;
          _M0L5dequeS1780 = _M0L8_2adequeS1786;
          goto join_1779;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1784);
          goto join_1778;
          break;
        }
      }
    }
    join_1779:;
    #line 685 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3valS1781 = _M0MP19moonbitDB5Deque10pop__front(_M0L5dequeS1780);
    _M0L4dataS4166 = _M0L4selfS1776->$0;
    _M0L4ListS4167
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4List));
    Moonbit_object_header(_M0L4ListS4167)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
    ((struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4ListS4167)->$0
    = _M0L5dequeS1780;
    moonbit_incref(_M0L4dataS4166);
    #line 686 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4166, _M0L3keyS1777, _M0L4ListS4167);
    moonbit_decref(_M0L4dataS4166);
    moonbit_decref(_M0L4ListS4167);
    return _M0L3valS1781;
    join_1778:;
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database5rpush(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1764,
  moonbit_string_t _M0L3keyS1765,
  moonbit_string_t _M0L5valueS1775
) {
  int32_t _M0L6_2atmpS4162;
  struct _M0TP19moonbitDB5Deque* _M0L5dequeS1766;
  struct _M0TP19moonbitDB5Deque* _M0L1dS1769;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4165;
  void* _M0L7_2abindS1770;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4163;
  void* _M0L4ListS4164;
  int32_t _result_5466;
  #line 668 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 669 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4162
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1764, _M0L3keyS1765);
  _M0L4dataS4165 = _M0L4selfS1764->$0;
  moonbit_incref(_M0L4dataS4165);
  #line 670 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1770
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4165, _M0L3keyS1765);
  moonbit_decref(_M0L4dataS4165);
  if (_M0L7_2abindS1770 == 0) {
    if (_M0L7_2abindS1770) {
      moonbit_decref(_M0L7_2abindS1770);
    }
    goto join_1767;
  } else {
    void* _M0L7_2aSomeS1771 = _M0L7_2abindS1770;
    void* _M0L4_2axS1772 = _M0L7_2aSomeS1771;
    switch (Moonbit_object_tag(_M0L4_2axS1772)) {
      case 2: {
        struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1773 =
          (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1772;
        struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS4738 =
          _M0L7_2aListS1773->$0;
        int32_t _M0L6_2acntS5200 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1773));
        struct _M0TP19moonbitDB5Deque* _M0L4_2adS1774;
        if (_M0L6_2acntS5200 > 1) {
          int32_t _M0L11_2anew__cntS5201 = _M0L6_2acntS5200 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1773), _M0L11_2anew__cntS5201);
          moonbit_incref(_M0L8_2afieldS4738);
        } else if (_M0L6_2acntS5200 == 1) {
          #line 670 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aListS1773);
        }
        _M0L4_2adS1774 = _M0L8_2afieldS4738;
        _M0L1dS1769 = _M0L4_2adS1774;
        goto join_1768;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1772);
        goto join_1767;
        break;
      }
    }
  }
  goto joinlet_5465;
  join_1768:;
  _M0L5dequeS1766 = _M0L1dS1769;
  joinlet_5465:;
  goto joinlet_5464;
  join_1767:;
  #line 672 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5dequeS1766 = _M0MP19moonbitDB5Deque3new();
  joinlet_5464:;
  #line 674 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MP19moonbitDB5Deque10push__back(_M0L5dequeS1766, _M0L5valueS1775);
  _M0L4dataS4163 = _M0L4selfS1764->$0;
  moonbit_incref(_M0L5dequeS1766);
  _M0L4ListS4164
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4List));
  Moonbit_object_header(_M0L4ListS4164)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
  ((struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4ListS4164)->$0
  = _M0L5dequeS1766;
  moonbit_incref(_M0L4dataS4163);
  #line 675 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4163, _M0L3keyS1765, _M0L4ListS4164);
  moonbit_decref(_M0L4dataS4163);
  moonbit_decref(_M0L4ListS4164);
  #line 676 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_5466 = _M0MP19moonbitDB5Deque6length(_M0L5dequeS1766);
  moonbit_decref(_M0L5dequeS1766);
  return _result_5466;
}

int32_t _M0MP19moonbitDB8Database5lpush(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1752,
  moonbit_string_t _M0L3keyS1753,
  moonbit_string_t _M0L5valueS1763
) {
  int32_t _M0L6_2atmpS4158;
  struct _M0TP19moonbitDB5Deque* _M0L5dequeS1754;
  struct _M0TP19moonbitDB5Deque* _M0L1dS1757;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4161;
  void* _M0L7_2abindS1758;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4159;
  void* _M0L4ListS4160;
  int32_t _result_5469;
  #line 657 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 658 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4158
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1752, _M0L3keyS1753);
  _M0L4dataS4161 = _M0L4selfS1752->$0;
  moonbit_incref(_M0L4dataS4161);
  #line 659 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1758
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4161, _M0L3keyS1753);
  moonbit_decref(_M0L4dataS4161);
  if (_M0L7_2abindS1758 == 0) {
    if (_M0L7_2abindS1758) {
      moonbit_decref(_M0L7_2abindS1758);
    }
    goto join_1755;
  } else {
    void* _M0L7_2aSomeS1759 = _M0L7_2abindS1758;
    void* _M0L4_2axS1760 = _M0L7_2aSomeS1759;
    switch (Moonbit_object_tag(_M0L4_2axS1760)) {
      case 2: {
        struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1761 =
          (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1760;
        struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS4741 =
          _M0L7_2aListS1761->$0;
        int32_t _M0L6_2acntS5202 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1761));
        struct _M0TP19moonbitDB5Deque* _M0L4_2adS1762;
        if (_M0L6_2acntS5202 > 1) {
          int32_t _M0L11_2anew__cntS5203 = _M0L6_2acntS5202 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1761), _M0L11_2anew__cntS5203);
          moonbit_incref(_M0L8_2afieldS4741);
        } else if (_M0L6_2acntS5202 == 1) {
          #line 659 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aListS1761);
        }
        _M0L4_2adS1762 = _M0L8_2afieldS4741;
        _M0L1dS1757 = _M0L4_2adS1762;
        goto join_1756;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1760);
        goto join_1755;
        break;
      }
    }
  }
  goto joinlet_5468;
  join_1756:;
  _M0L5dequeS1754 = _M0L1dS1757;
  joinlet_5468:;
  goto joinlet_5467;
  join_1755:;
  #line 661 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5dequeS1754 = _M0MP19moonbitDB5Deque3new();
  joinlet_5467:;
  #line 663 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MP19moonbitDB5Deque11push__front(_M0L5dequeS1754, _M0L5valueS1763);
  _M0L4dataS4159 = _M0L4selfS1752->$0;
  moonbit_incref(_M0L5dequeS1754);
  _M0L4ListS4160
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4List));
  Moonbit_object_header(_M0L4ListS4160)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
  ((struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4ListS4160)->$0
  = _M0L5dequeS1754;
  moonbit_incref(_M0L4dataS4159);
  #line 664 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4159, _M0L3keyS1753, _M0L4ListS4160);
  moonbit_decref(_M0L4dataS4159);
  moonbit_decref(_M0L4ListS4160);
  #line 665 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_5469 = _M0MP19moonbitDB5Deque6length(_M0L5dequeS1754);
  moonbit_decref(_M0L5dequeS1754);
  return _result_5469;
}

int32_t _M0MP19moonbitDB8Database4hlen(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1743,
  moonbit_string_t _M0L3keyS1744
) {
  #line 646 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 647 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1743, _M0L3keyS1744)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1746;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4157 =
      _M0L4selfS1743->$0;
    void* _M0L7_2abindS1747;
    int32_t _result_5471;
    moonbit_incref(_M0L4dataS4157);
    #line 650 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1747
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4157, _M0L3keyS1744);
    moonbit_decref(_M0L4dataS4157);
    if (_M0L7_2abindS1747 == 0) {
      if (_M0L7_2abindS1747) {
        moonbit_decref(_M0L7_2abindS1747);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1748 = _M0L7_2abindS1747;
      void* _M0L4_2axS1749 = _M0L7_2aSomeS1748;
      switch (Moonbit_object_tag(_M0L4_2axS1749)) {
        case 1: {
          struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1750 =
            (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1749;
          struct _M0TPB3MapGssE* _M0L8_2afieldS4743 = _M0L7_2aHashS1750->$0;
          int32_t _M0L6_2acntS5204 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1750));
          struct _M0TPB3MapGssE* _M0L4_2ahS1751;
          if (_M0L6_2acntS5204 > 1) {
            int32_t _M0L11_2anew__cntS5205 = _M0L6_2acntS5204 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1750), _M0L11_2anew__cntS5205);
            moonbit_incref(_M0L8_2afieldS4743);
          } else if (_M0L6_2acntS5204 == 1) {
            #line 650 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aHashS1750);
          }
          _M0L4_2ahS1751 = _M0L8_2afieldS4743;
          _M0L1hS1746 = _M0L4_2ahS1751;
          goto join_1745;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1749);
          return 0;
          break;
        }
      }
    }
    join_1745:;
    #line 651 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_5471 = _M0MPB3Map6lengthGssE(_M0L1hS1746);
    moonbit_decref(_M0L1hS1746);
    return _result_5471;
  }
}

struct _M0TPB3MapGssE* _M0MP19moonbitDB8Database7hgetall(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1731,
  moonbit_string_t _M0L3keyS1732
) {
  #line 635 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 636 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1731, _M0L3keyS1732)
  ) {
    struct _M0TUssE** _M0L7_2abindS1733 =
      (struct _M0TUssE**)moonbit_empty_ref_array;
    struct _M0TUssE** _M0L6_2atmpS4153 = _M0L7_2abindS1733;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS4152 =
      (struct _M0TPB9ArrayViewGUssEE){.$0 = _M0L6_2atmpS4153,
                                        .$1 = 0,
                                        .$2 = 0};
    struct _M0TPB3MapGssE* _result_5472;
    #line 637 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_5472 = _M0MPB3Map3MapGssE(_M0L6_2atmpS4152, 0ll);
    moonbit_decref(_M0L6_2atmpS4152.$0);
    return _result_5472;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1737;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4156 =
      _M0L4selfS1731->$0;
    void* _M0L7_2abindS1738;
    struct _M0TUssE** _M0L7_2abindS1735;
    struct _M0TUssE** _M0L6_2atmpS4155;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS4154;
    struct _M0TPB3MapGssE* _result_5475;
    moonbit_incref(_M0L4dataS4156);
    #line 639 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1738
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4156, _M0L3keyS1732);
    moonbit_decref(_M0L4dataS4156);
    if (_M0L7_2abindS1738 == 0) {
      if (_M0L7_2abindS1738) {
        moonbit_decref(_M0L7_2abindS1738);
      }
      goto join_1734;
    } else {
      void* _M0L7_2aSomeS1739 = _M0L7_2abindS1738;
      void* _M0L4_2axS1740 = _M0L7_2aSomeS1739;
      switch (Moonbit_object_tag(_M0L4_2axS1740)) {
        case 1: {
          struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1741 =
            (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1740;
          struct _M0TPB3MapGssE* _M0L8_2afieldS4745 = _M0L7_2aHashS1741->$0;
          int32_t _M0L6_2acntS5206 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1741));
          struct _M0TPB3MapGssE* _M0L4_2ahS1742;
          if (_M0L6_2acntS5206 > 1) {
            int32_t _M0L11_2anew__cntS5207 = _M0L6_2acntS5206 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1741), _M0L11_2anew__cntS5207);
            moonbit_incref(_M0L8_2afieldS4745);
          } else if (_M0L6_2acntS5206 == 1) {
            #line 639 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aHashS1741);
          }
          _M0L4_2ahS1742 = _M0L8_2afieldS4745;
          _M0L1hS1737 = _M0L4_2ahS1742;
          goto join_1736;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1740);
          goto join_1734;
          break;
        }
      }
    }
    join_1736:;
    return _M0L1hS1737;
    join_1734:;
    _M0L7_2abindS1735 = (struct _M0TUssE**)moonbit_empty_ref_array;
    _M0L6_2atmpS4155 = _M0L7_2abindS1735;
    _M0L6_2atmpS4154
    = (struct _M0TPB9ArrayViewGUssEE){
      .$0 = _M0L6_2atmpS4155, .$1 = 0, .$2 = 0
    };
    #line 641 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_5475 = _M0MPB3Map3MapGssE(_M0L6_2atmpS4154, 0ll);
    moonbit_decref(_M0L6_2atmpS4154.$0);
    return _result_5475;
  }
}

int32_t _M0MP19moonbitDB8Database4hdel(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1720,
  moonbit_string_t _M0L3keyS1721,
  moonbit_string_t _M0L5fieldS1725
) {
  #line 617 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 618 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1720, _M0L3keyS1721)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1723;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4151 =
      _M0L4selfS1720->$0;
    void* _M0L7_2abindS1726;
    int32_t _M0L7existedS1724;
    moonbit_incref(_M0L4dataS4151);
    #line 621 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1726
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4151, _M0L3keyS1721);
    moonbit_decref(_M0L4dataS4151);
    if (_M0L7_2abindS1726 == 0) {
      if (_M0L7_2abindS1726) {
        moonbit_decref(_M0L7_2abindS1726);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1727 = _M0L7_2abindS1726;
      void* _M0L4_2axS1728 = _M0L7_2aSomeS1727;
      switch (Moonbit_object_tag(_M0L4_2axS1728)) {
        case 1: {
          struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1729 =
            (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1728;
          struct _M0TPB3MapGssE* _M0L8_2afieldS4748 = _M0L7_2aHashS1729->$0;
          int32_t _M0L6_2acntS5208 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1729));
          struct _M0TPB3MapGssE* _M0L4_2ahS1730;
          if (_M0L6_2acntS5208 > 1) {
            int32_t _M0L11_2anew__cntS5209 = _M0L6_2acntS5208 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1729), _M0L11_2anew__cntS5209);
            moonbit_incref(_M0L8_2afieldS4748);
          } else if (_M0L6_2acntS5208 == 1) {
            #line 621 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aHashS1729);
          }
          _M0L4_2ahS1730 = _M0L8_2afieldS4748;
          _M0L1hS1723 = _M0L4_2ahS1730;
          goto join_1722;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1728);
          return 0;
          break;
        }
      }
    }
    join_1722:;
    #line 623 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7existedS1724 = _M0MPB3Map8containsGssE(_M0L1hS1723, _M0L5fieldS1725);
    if (_M0L7existedS1724) {
      struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4149;
      void* _M0L4HashS4150;
      #line 625 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map6removeGssE(_M0L1hS1723, _M0L5fieldS1725);
      _M0L4dataS4149 = _M0L4selfS1720->$0;
      _M0L4HashS4150
      = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4Hash));
      Moonbit_object_header(_M0L4HashS4150)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 18, 1);
      ((struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4HashS4150)->$0
      = _M0L1hS1723;
      moonbit_incref(_M0L4dataS4149);
      #line 626 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4149, _M0L3keyS1721, _M0L4HashS4150);
      moonbit_decref(_M0L4dataS4149);
      moonbit_decref(_M0L4HashS4150);
    } else {
      moonbit_decref(_M0L1hS1723);
    }
    return _M0L7existedS1724;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database4hget(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1709,
  moonbit_string_t _M0L3keyS1710,
  moonbit_string_t _M0L5fieldS1714
) {
  #line 606 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 607 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1709, _M0L3keyS1710)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1713;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4148 =
      _M0L4selfS1709->$0;
    void* _M0L7_2abindS1715;
    moonbit_string_t _result_5479;
    moonbit_incref(_M0L4dataS4148);
    #line 610 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1715
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4148, _M0L3keyS1710);
    moonbit_decref(_M0L4dataS4148);
    if (_M0L7_2abindS1715 == 0) {
      if (_M0L7_2abindS1715) {
        moonbit_decref(_M0L7_2abindS1715);
      }
      goto join_1711;
    } else {
      void* _M0L7_2aSomeS1716 = _M0L7_2abindS1715;
      void* _M0L4_2axS1717 = _M0L7_2aSomeS1716;
      switch (Moonbit_object_tag(_M0L4_2axS1717)) {
        case 1: {
          struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1718 =
            (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1717;
          struct _M0TPB3MapGssE* _M0L8_2afieldS4750 = _M0L7_2aHashS1718->$0;
          int32_t _M0L6_2acntS5210 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1718));
          struct _M0TPB3MapGssE* _M0L4_2ahS1719;
          if (_M0L6_2acntS5210 > 1) {
            int32_t _M0L11_2anew__cntS5211 = _M0L6_2acntS5210 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1718), _M0L11_2anew__cntS5211);
            moonbit_incref(_M0L8_2afieldS4750);
          } else if (_M0L6_2acntS5210 == 1) {
            #line 610 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aHashS1718);
          }
          _M0L4_2ahS1719 = _M0L8_2afieldS4750;
          _M0L1hS1713 = _M0L4_2ahS1719;
          goto join_1712;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1717);
          goto join_1711;
          break;
        }
      }
    }
    join_1712:;
    #line 611 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_5479 = _M0MPB3Map3getGssE(_M0L1hS1713, _M0L5fieldS1714);
    moonbit_decref(_M0L1hS1713);
    return _result_5479;
    join_1711:;
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database4hset(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1695,
  moonbit_string_t _M0L3keyS1696,
  moonbit_string_t _M0L5fieldS1707,
  moonbit_string_t _M0L5valueS1708
) {
  int32_t _M0L6_2atmpS4142;
  struct _M0TPB3MapGssE* _M0L4hashS1697;
  struct _M0TPB3MapGssE* _M0L1hS1701;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4147;
  void* _M0L7_2abindS1702;
  struct _M0TUssE** _M0L7_2abindS1699;
  struct _M0TUssE** _M0L6_2atmpS4146;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS4145;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4143;
  void* _M0L4HashS4144;
  #line 596 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 597 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4142
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1695, _M0L3keyS1696);
  _M0L4dataS4147 = _M0L4selfS1695->$0;
  moonbit_incref(_M0L4dataS4147);
  #line 598 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1702
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4147, _M0L3keyS1696);
  moonbit_decref(_M0L4dataS4147);
  if (_M0L7_2abindS1702 == 0) {
    if (_M0L7_2abindS1702) {
      moonbit_decref(_M0L7_2abindS1702);
    }
    goto join_1698;
  } else {
    void* _M0L7_2aSomeS1703 = _M0L7_2abindS1702;
    void* _M0L4_2axS1704 = _M0L7_2aSomeS1703;
    switch (Moonbit_object_tag(_M0L4_2axS1704)) {
      case 1: {
        struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1705 =
          (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1704;
        struct _M0TPB3MapGssE* _M0L8_2afieldS4753 = _M0L7_2aHashS1705->$0;
        int32_t _M0L6_2acntS5212 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1705));
        struct _M0TPB3MapGssE* _M0L4_2ahS1706;
        if (_M0L6_2acntS5212 > 1) {
          int32_t _M0L11_2anew__cntS5213 = _M0L6_2acntS5212 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1705), _M0L11_2anew__cntS5213);
          moonbit_incref(_M0L8_2afieldS4753);
        } else if (_M0L6_2acntS5212 == 1) {
          #line 598 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aHashS1705);
        }
        _M0L4_2ahS1706 = _M0L8_2afieldS4753;
        _M0L1hS1701 = _M0L4_2ahS1706;
        goto join_1700;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1704);
        goto join_1698;
        break;
      }
    }
  }
  goto joinlet_5481;
  join_1700:;
  _M0L4hashS1697 = _M0L1hS1701;
  joinlet_5481:;
  goto joinlet_5480;
  join_1698:;
  _M0L7_2abindS1699 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS4146 = _M0L7_2abindS1699;
  _M0L6_2atmpS4145
  = (struct _M0TPB9ArrayViewGUssEE){
    .$0 = _M0L6_2atmpS4146, .$1 = 0, .$2 = 0
  };
  #line 600 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4hashS1697 = _M0MPB3Map3MapGssE(_M0L6_2atmpS4145, 10ll);
  moonbit_decref(_M0L6_2atmpS4145.$0);
  joinlet_5480:;
  #line 602 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGssE(_M0L4hashS1697, _M0L5fieldS1707, _M0L5valueS1708);
  _M0L4dataS4143 = _M0L4selfS1695->$0;
  _M0L4HashS4144
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4Hash));
  Moonbit_object_header(_M0L4HashS4144)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 18, 1);
  ((struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4HashS4144)->$0
  = _M0L4hashS1697;
  moonbit_incref(_M0L4dataS4143);
  #line 603 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4143, _M0L3keyS1696, _M0L4HashS4144);
  moonbit_decref(_M0L4dataS4143);
  moonbit_decref(_M0L4HashS4144);
  return 0;
}

int64_t _M0MP19moonbitDB8Database4decr(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1679,
  moonbit_string_t _M0L3keyS1680
) {
  int32_t _M0L6_2atmpS4135;
  moonbit_string_t _M0L1sS1683;
  moonbit_string_t _M0L7currentS1681;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4141;
  void* _M0L7_2abindS1684;
  int32_t _M0L1nS1690;
  int64_t _M0L7_2abindS1692;
  int32_t _M0L6_2atmpS4140;
  moonbit_string_t _M0L8new__valS1691;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4136;
  void* _M0L6StringS4137;
  struct _M0TPB3MapGsiE* _M0L7expiresS4138;
  int32_t _M0L6_2atmpS4139;
  #line 579 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 580 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4135
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1679, _M0L3keyS1680);
  _M0L4dataS4141 = _M0L4selfS1679->$0;
  moonbit_incref(_M0L4dataS4141);
  #line 581 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1684
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4141, _M0L3keyS1680);
  moonbit_decref(_M0L4dataS4141);
  if (_M0L7_2abindS1684 == 0) {
    if (_M0L7_2abindS1684) {
      moonbit_decref(_M0L7_2abindS1684);
    }
    _M0L7currentS1681 = (moonbit_string_t)moonbit_string_literal_122.data;
  } else {
    void* _M0L7_2aSomeS1685 = _M0L7_2abindS1684;
    void* _M0L4_2axS1686 = _M0L7_2aSomeS1685;
    switch (Moonbit_object_tag(_M0L4_2axS1686)) {
      case 0: {
        struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1687 =
          (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1686;
        moonbit_string_t _M0L8_2afieldS4757 = _M0L9_2aStringS1687->$0;
        int32_t _M0L6_2acntS5214 =
          Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1687));
        moonbit_string_t _M0L4_2asS1688;
        if (_M0L6_2acntS5214 > 1) {
          int32_t _M0L11_2anew__cntS5215 = _M0L6_2acntS5214 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1687), _M0L11_2anew__cntS5215);
          moonbit_incref(_M0L8_2afieldS4757);
        } else if (_M0L6_2acntS5214 == 1) {
          #line 581 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L9_2aStringS1687);
        }
        _M0L4_2asS1688 = _M0L8_2afieldS4757;
        _M0L1sS1683 = _M0L4_2asS1688;
        goto join_1682;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1686);
        _M0L7currentS1681 = (moonbit_string_t)moonbit_string_literal_122.data;
        break;
      }
    }
  }
  goto joinlet_5482;
  join_1682:;
  _M0L7currentS1681 = _M0L1sS1683;
  joinlet_5482:;
  #line 585 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1692 = _M0FP19moonbitDB10parse__int(_M0L7currentS1681);
  moonbit_decref(_M0L7currentS1681);
  if (_M0L7_2abindS1692 == 4294967296ll) {
    return 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS1693 = _M0L7_2abindS1692;
    int32_t _M0L4_2anS1694 = (int32_t)_M0L7_2aSomeS1693;
    _M0L1nS1690 = _M0L4_2anS1694;
    goto join_1689;
  }
  join_1689:;
  _M0L6_2atmpS4140 = _M0L1nS1690 - 1;
  #line 587 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L8new__valS1691 = _M0FP19moonbitDB15int__to__string(_M0L6_2atmpS4140);
  _M0L4dataS4136 = _M0L4selfS1679->$0;
  _M0L6StringS4137
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
  Moonbit_object_header(_M0L6StringS4137)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
  ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS4137)->$0
  = _M0L8new__valS1691;
  moonbit_incref(_M0L4dataS4136);
  #line 588 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4136, _M0L3keyS1680, _M0L6StringS4137);
  moonbit_decref(_M0L4dataS4136);
  moonbit_decref(_M0L6StringS4137);
  _M0L7expiresS4138 = _M0L4selfS1679->$1;
  moonbit_incref(_M0L7expiresS4138);
  #line 589 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS4138, _M0L3keyS1680);
  moonbit_decref(_M0L7expiresS4138);
  _M0L6_2atmpS4139 = _M0L1nS1690 - 1;
  return (int64_t)_M0L6_2atmpS4139;
}

int64_t _M0MP19moonbitDB8Database4incr(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1663,
  moonbit_string_t _M0L3keyS1664
) {
  int32_t _M0L6_2atmpS4128;
  moonbit_string_t _M0L1sS1667;
  moonbit_string_t _M0L7currentS1665;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4134;
  void* _M0L7_2abindS1668;
  int32_t _M0L1nS1674;
  int64_t _M0L7_2abindS1676;
  int32_t _M0L6_2atmpS4133;
  moonbit_string_t _M0L8new__valS1675;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4129;
  void* _M0L6StringS4130;
  struct _M0TPB3MapGsiE* _M0L7expiresS4131;
  int32_t _M0L6_2atmpS4132;
  #line 562 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 563 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4128
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1663, _M0L3keyS1664);
  _M0L4dataS4134 = _M0L4selfS1663->$0;
  moonbit_incref(_M0L4dataS4134);
  #line 564 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1668
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4134, _M0L3keyS1664);
  moonbit_decref(_M0L4dataS4134);
  if (_M0L7_2abindS1668 == 0) {
    if (_M0L7_2abindS1668) {
      moonbit_decref(_M0L7_2abindS1668);
    }
    _M0L7currentS1665 = (moonbit_string_t)moonbit_string_literal_122.data;
  } else {
    void* _M0L7_2aSomeS1669 = _M0L7_2abindS1668;
    void* _M0L4_2axS1670 = _M0L7_2aSomeS1669;
    switch (Moonbit_object_tag(_M0L4_2axS1670)) {
      case 0: {
        struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1671 =
          (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1670;
        moonbit_string_t _M0L8_2afieldS4761 = _M0L9_2aStringS1671->$0;
        int32_t _M0L6_2acntS5216 =
          Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1671));
        moonbit_string_t _M0L4_2asS1672;
        if (_M0L6_2acntS5216 > 1) {
          int32_t _M0L11_2anew__cntS5217 = _M0L6_2acntS5216 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1671), _M0L11_2anew__cntS5217);
          moonbit_incref(_M0L8_2afieldS4761);
        } else if (_M0L6_2acntS5216 == 1) {
          #line 564 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L9_2aStringS1671);
        }
        _M0L4_2asS1672 = _M0L8_2afieldS4761;
        _M0L1sS1667 = _M0L4_2asS1672;
        goto join_1666;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1670);
        _M0L7currentS1665 = (moonbit_string_t)moonbit_string_literal_122.data;
        break;
      }
    }
  }
  goto joinlet_5484;
  join_1666:;
  _M0L7currentS1665 = _M0L1sS1667;
  joinlet_5484:;
  #line 568 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1676 = _M0FP19moonbitDB10parse__int(_M0L7currentS1665);
  moonbit_decref(_M0L7currentS1665);
  if (_M0L7_2abindS1676 == 4294967296ll) {
    return 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS1677 = _M0L7_2abindS1676;
    int32_t _M0L4_2anS1678 = (int32_t)_M0L7_2aSomeS1677;
    _M0L1nS1674 = _M0L4_2anS1678;
    goto join_1673;
  }
  join_1673:;
  _M0L6_2atmpS4133 = _M0L1nS1674 + 1;
  #line 570 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L8new__valS1675 = _M0FP19moonbitDB15int__to__string(_M0L6_2atmpS4133);
  _M0L4dataS4129 = _M0L4selfS1663->$0;
  _M0L6StringS4130
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
  Moonbit_object_header(_M0L6StringS4130)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
  ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS4130)->$0
  = _M0L8new__valS1675;
  moonbit_incref(_M0L4dataS4129);
  #line 571 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4129, _M0L3keyS1664, _M0L6StringS4130);
  moonbit_decref(_M0L4dataS4129);
  moonbit_decref(_M0L6StringS4130);
  _M0L7expiresS4131 = _M0L4selfS1663->$1;
  moonbit_incref(_M0L7expiresS4131);
  #line 572 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS4131, _M0L3keyS1664);
  moonbit_decref(_M0L7expiresS4131);
  _M0L6_2atmpS4132 = _M0L1nS1674 + 1;
  return (int64_t)_M0L6_2atmpS4132;
}

moonbit_string_t _M0FP19moonbitDB15int__to__string(int32_t _M0L1nS1654) {
  #line 526 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (_M0L1nS1654 == 0) {
    return (moonbit_string_t)moonbit_string_literal_122.data;
  } else {
    struct _M0TPB8MutLocalGiE* _M0L3numS1655 =
      (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    moonbit_string_t* _M0L6_2atmpS4127;
    struct _M0TPB5ArrayGsE* _M0L5charsS1656;
    int32_t _M0L3valS4113;
    moonbit_string_t* _M0L6_2atmpS4126;
    struct _M0TPB5ArrayGsE* _M0L6resultS1659;
    int32_t _M0L6_2atmpS4123;
    int32_t _M0L6_2atmpS4122;
    int32_t _M0L1iS1660;
    moonbit_string_t _M0L7_2abindS1662;
    int32_t _M0L6_2atmpS4125;
    struct _M0TPC16string10StringView _M0L6_2atmpS4124;
    moonbit_string_t _result_5488;
    Moonbit_object_header(_M0L3numS1655)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L3numS1655->$0 = _M0L1nS1654;
    _M0L6_2atmpS4127 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L5charsS1656
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L5charsS1656)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L5charsS1656->$0 = _M0L6_2atmpS4127;
    _M0L5charsS1656->$1 = 0;
    _M0L3valS4113 = _M0L3numS1655->$0;
    if (_M0L3valS4113 < 0) {
      int32_t _M0L3valS4115 = _M0L3numS1655->$0;
      int32_t _M0L6_2atmpS4114 = -_M0L3valS4115;
      _M0L3numS1655->$0 = _M0L6_2atmpS4114;
    }
    while (1) {
      int32_t _M0L3valS4116 = _M0L3numS1655->$0;
      if (_M0L3valS4116 > 0) {
        int32_t _M0L3valS4117 = _M0L3numS1655->$0;
        int32_t _M0L7_2abindS1657 = _M0L3valS4117 % 10;
        int32_t _M0L3valS4119;
        int32_t _M0L6_2atmpS4118;
        switch (_M0L7_2abindS1657) {
          case 0: {
            #line 537 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1656, (moonbit_string_t)moonbit_string_literal_122.data);
            break;
          }
          
          case 1: {
            #line 538 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1656, (moonbit_string_t)moonbit_string_literal_123.data);
            break;
          }
          
          case 2: {
            #line 539 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1656, (moonbit_string_t)moonbit_string_literal_124.data);
            break;
          }
          
          case 3: {
            #line 540 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1656, (moonbit_string_t)moonbit_string_literal_125.data);
            break;
          }
          
          case 4: {
            #line 541 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1656, (moonbit_string_t)moonbit_string_literal_126.data);
            break;
          }
          
          case 5: {
            #line 542 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1656, (moonbit_string_t)moonbit_string_literal_127.data);
            break;
          }
          
          case 6: {
            #line 543 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1656, (moonbit_string_t)moonbit_string_literal_128.data);
            break;
          }
          
          case 7: {
            #line 544 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1656, (moonbit_string_t)moonbit_string_literal_129.data);
            break;
          }
          
          case 8: {
            #line 545 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1656, (moonbit_string_t)moonbit_string_literal_130.data);
            break;
          }
          
          case 9: {
            #line 546 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1656, (moonbit_string_t)moonbit_string_literal_131.data);
            break;
          }
          default: {
            #line 547 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1656, (moonbit_string_t)moonbit_string_literal_122.data);
            break;
          }
        }
        _M0L3valS4119 = _M0L3numS1655->$0;
        _M0L6_2atmpS4118 = _M0L3valS4119 / 10;
        _M0L3numS1655->$0 = _M0L6_2atmpS4118;
        continue;
      } else {
        moonbit_decref(_M0L3numS1655);
      }
      break;
    }
    if (_M0L1nS1654 < 0) {
      #line 552 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L5charsS1656, (moonbit_string_t)moonbit_string_literal_132.data);
    }
    _M0L6_2atmpS4126 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1659
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1659)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS1659->$0 = _M0L6_2atmpS4126;
    _M0L6resultS1659->$1 = 0;
    #line 555 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS4123 = _M0MPC15array5Array6lengthGsE(_M0L5charsS1656);
    _M0L6_2atmpS4122 = _M0L6_2atmpS4123 - 1;
    _M0L1iS1660 = _M0L6_2atmpS4122;
    while (1) {
      if (_M0L1iS1660 >= 0) {
        moonbit_string_t _M0L6_2atmpS4120;
        int32_t _M0L6_2atmpS4121;
        #line 556 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS4120
        = _M0MPC15array5Array2atGsE(_M0L5charsS1656, _M0L1iS1660);
        #line 556 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1659, _M0L6_2atmpS4120);
        moonbit_decref(_M0L6_2atmpS4120);
        _M0L6_2atmpS4121 = _M0L1iS1660 - 1;
        _M0L1iS1660 = _M0L6_2atmpS4121;
        continue;
      } else {
        moonbit_decref(_M0L5charsS1656);
      }
      break;
    }
    _M0L7_2abindS1662 = (moonbit_string_t)moonbit_string_literal_75.data;
    _M0L6_2atmpS4125 = Moonbit_array_length(_M0L7_2abindS1662);
    _M0L6_2atmpS4124
    = (struct _M0TPC16string10StringView){
      .$0 = _M0L7_2abindS1662, .$1 = 0, .$2 = _M0L6_2atmpS4125
    };
    #line 558 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_5488
    = _M0MPC15array5Array4joinGsE(_M0L6resultS1659, _M0L6_2atmpS4124);
    moonbit_decref(_M0L6resultS1659);
    moonbit_decref(_M0L6_2atmpS4124.$0);
    return _result_5488;
  }
}

int64_t _M0FP19moonbitDB10parse__int(moonbit_string_t _M0L1sS1643) {
  int32_t _M0L6_2atmpS4100;
  #line 503 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4100 = Moonbit_array_length(_M0L1sS1643);
  if (_M0L6_2atmpS4100 == 0) {
    return 4294967296ll;
  } else {
    struct _M0TPB8MutLocalGiE* _M0L6resultS1644 =
      (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    struct _M0TPB8MutLocalGiE* _M0L4signS1645;
    struct _M0TPB8MutLocalGiE* _M0L5startS1646;
    int32_t _M0L6_2atmpS4101;
    int32_t _M0L3valS4109;
    int32_t _M0L1iS1647;
    int32_t _M0L3valS4111;
    int32_t _M0L3valS4112;
    int32_t _M0L6_2atmpS4110;
    Moonbit_object_header(_M0L6resultS1644)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L6resultS1644->$0 = 0;
    _M0L4signS1645
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L4signS1645)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L4signS1645->$0 = 1;
    _M0L5startS1646
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L5startS1646)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L5startS1646->$0 = 0;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1643)) {
      #line 510 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4101 = _M0L1sS1643[0];
    if (_M0L6_2atmpS4101 == 45) {
      _M0L4signS1645->$0 = -1;
      _M0L5startS1646->$0 = 1;
    } else {
      int32_t _M0L6_2atmpS4102;
      if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1643)) {
        #line 513 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4102 = _M0L1sS1643[0];
      if (_M0L6_2atmpS4102 == 43) {
        _M0L5startS1646->$0 = 1;
      }
    }
    _M0L3valS4109 = _M0L5startS1646->$0;
    moonbit_decref(_M0L5startS1646);
    _M0L1iS1647 = _M0L3valS4109;
    while (1) {
      int32_t _M0L6_2atmpS4103 = Moonbit_array_length(_M0L1sS1643);
      if (_M0L1iS1647 < _M0L6_2atmpS4103) {
        int32_t _M0L5digitS1649;
        int32_t _M0L6_2atmpS4107;
        int64_t _M0L7_2abindS1650;
        int32_t _M0L3valS4106;
        int32_t _M0L6_2atmpS4105;
        int32_t _M0L6_2atmpS4104;
        int32_t _M0L6_2atmpS4108;
        if (
          _M0L1iS1647 < 0 || _M0L1iS1647 >= Moonbit_array_length(_M0L1sS1643)
        ) {
          #line 517 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4107 = _M0L1sS1643[_M0L1iS1647];
        #line 517 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L7_2abindS1650
        = _M0FP19moonbitDB17uint16__to__digit(_M0L6_2atmpS4107);
        if (_M0L7_2abindS1650 == 4294967296ll) {
          moonbit_decref(_M0L4signS1645);
          moonbit_decref(_M0L6resultS1644);
          return 4294967296ll;
        } else {
          int64_t _M0L7_2aSomeS1651 = _M0L7_2abindS1650;
          int32_t _M0L8_2adigitS1652 = (int32_t)_M0L7_2aSomeS1651;
          _M0L5digitS1649 = _M0L8_2adigitS1652;
          goto join_1648;
        }
        goto joinlet_5490;
        join_1648:;
        _M0L3valS4106 = _M0L6resultS1644->$0;
        _M0L6_2atmpS4105 = _M0L3valS4106 * 10;
        _M0L6_2atmpS4104 = _M0L6_2atmpS4105 + _M0L5digitS1649;
        _M0L6resultS1644->$0 = _M0L6_2atmpS4104;
        joinlet_5490:;
        _M0L6_2atmpS4108 = _M0L1iS1647 + 1;
        _M0L1iS1647 = _M0L6_2atmpS4108;
        continue;
      }
      break;
    }
    _M0L3valS4111 = _M0L6resultS1644->$0;
    moonbit_decref(_M0L6resultS1644);
    _M0L3valS4112 = _M0L4signS1645->$0;
    moonbit_decref(_M0L4signS1645);
    _M0L6_2atmpS4110 = _M0L3valS4111 * _M0L3valS4112;
    return (int64_t)_M0L6_2atmpS4110;
  }
}

int64_t _M0FP19moonbitDB17uint16__to__digit(int32_t _M0L1cS1642) {
  #line 471 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  switch (_M0L1cS1642) {
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

int32_t _M0MP19moonbitDB8Database6strlen(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1633,
  moonbit_string_t _M0L3keyS1634
) {
  #line 460 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 461 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1633, _M0L3keyS1634)
  ) {
    return 0;
  } else {
    moonbit_string_t _M0L1sS1636;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4099 =
      _M0L4selfS1633->$0;
    void* _M0L7_2abindS1637;
    int32_t _result_5492;
    moonbit_incref(_M0L4dataS4099);
    #line 464 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1637
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4099, _M0L3keyS1634);
    moonbit_decref(_M0L4dataS4099);
    if (_M0L7_2abindS1637 == 0) {
      if (_M0L7_2abindS1637) {
        moonbit_decref(_M0L7_2abindS1637);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1638 = _M0L7_2abindS1637;
      void* _M0L4_2axS1639 = _M0L7_2aSomeS1638;
      switch (Moonbit_object_tag(_M0L4_2axS1639)) {
        case 0: {
          struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1640 =
            (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1639;
          moonbit_string_t _M0L8_2afieldS4763 = _M0L9_2aStringS1640->$0;
          int32_t _M0L6_2acntS5218 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1640));
          moonbit_string_t _M0L4_2asS1641;
          if (_M0L6_2acntS5218 > 1) {
            int32_t _M0L11_2anew__cntS5219 = _M0L6_2acntS5218 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1640), _M0L11_2anew__cntS5219);
            moonbit_incref(_M0L8_2afieldS4763);
          } else if (_M0L6_2acntS5218 == 1) {
            #line 464 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L9_2aStringS1640);
          }
          _M0L4_2asS1641 = _M0L8_2afieldS4763;
          _M0L1sS1636 = _M0L4_2asS1641;
          goto join_1635;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1639);
          return 0;
          break;
        }
      }
    }
    join_1635:;
    _result_5492 = Moonbit_array_length(_M0L1sS1636);
    moonbit_decref(_M0L1sS1636);
    return _result_5492;
  }
}

int32_t _M0MP19moonbitDB8Database6append(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1620,
  moonbit_string_t _M0L3keyS1621,
  moonbit_string_t _M0L5valueS1623
) {
  #line 444 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 445 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1620, _M0L3keyS1621)
  ) {
    moonbit_string_t _M0L8new__valS1622 = _M0L5valueS1623;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4094 =
      _M0L4selfS1620->$0;
    void* _M0L6StringS4095;
    moonbit_incref(_M0L8new__valS1622);
    _M0L6StringS4095
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
    Moonbit_object_header(_M0L6StringS4095)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
    ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS4095)->$0
    = _M0L8new__valS1622;
    moonbit_incref(_M0L4dataS4094);
    #line 447 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4094, _M0L3keyS1621, _M0L6StringS4095);
    moonbit_decref(_M0L4dataS4094);
    moonbit_decref(_M0L6StringS4095);
    return Moonbit_array_length(_M0L8new__valS1622);
  } else {
    moonbit_string_t _M0L1sS1626;
    moonbit_string_t _M0L7currentS1624;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4098 =
      _M0L4selfS1620->$0;
    void* _M0L7_2abindS1627;
    moonbit_string_t _M0L8new__valS1632;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4096;
    void* _M0L6StringS4097;
    int32_t _result_5494;
    moonbit_incref(_M0L4dataS4098);
    #line 450 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1627
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4098, _M0L3keyS1621);
    moonbit_decref(_M0L4dataS4098);
    if (_M0L7_2abindS1627 == 0) {
      if (_M0L7_2abindS1627) {
        moonbit_decref(_M0L7_2abindS1627);
      }
      _M0L7currentS1624 = (moonbit_string_t)moonbit_string_literal_75.data;
    } else {
      void* _M0L7_2aSomeS1628 = _M0L7_2abindS1627;
      void* _M0L4_2axS1629 = _M0L7_2aSomeS1628;
      switch (Moonbit_object_tag(_M0L4_2axS1629)) {
        case 0: {
          struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1630 =
            (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1629;
          moonbit_string_t _M0L8_2afieldS4767 = _M0L9_2aStringS1630->$0;
          int32_t _M0L6_2acntS5220 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1630));
          moonbit_string_t _M0L4_2asS1631;
          if (_M0L6_2acntS5220 > 1) {
            int32_t _M0L11_2anew__cntS5221 = _M0L6_2acntS5220 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1630), _M0L11_2anew__cntS5221);
            moonbit_incref(_M0L8_2afieldS4767);
          } else if (_M0L6_2acntS5220 == 1) {
            #line 450 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L9_2aStringS1630);
          }
          _M0L4_2asS1631 = _M0L8_2afieldS4767;
          _M0L1sS1626 = _M0L4_2asS1631;
          goto join_1625;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1629);
          _M0L7currentS1624
          = (moonbit_string_t)moonbit_string_literal_75.data;
          break;
        }
      }
    }
    goto joinlet_5493;
    join_1625:;
    _M0L7currentS1624 = _M0L1sS1626;
    joinlet_5493:;
    #line 454 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L8new__valS1632
    = moonbit_add_string(_M0L7currentS1624, _M0L5valueS1623);
    moonbit_decref(_M0L7currentS1624);
    _M0L4dataS4096 = _M0L4selfS1620->$0;
    moonbit_incref(_M0L8new__valS1632);
    _M0L6StringS4097
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
    Moonbit_object_header(_M0L6StringS4097)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
    ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS4097)->$0
    = _M0L8new__valS1632;
    moonbit_incref(_M0L4dataS4096);
    #line 455 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4096, _M0L3keyS1621, _M0L6StringS4097);
    moonbit_decref(_M0L4dataS4096);
    moonbit_decref(_M0L6StringS4097);
    _result_5494 = Moonbit_array_length(_M0L8new__valS1632);
    moonbit_decref(_M0L8new__valS1632);
    return _result_5494;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database8type__of(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1615,
  moonbit_string_t _M0L3keyS1616
) {
  #line 429 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 430 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1615, _M0L3keyS1616)
  ) {
    return (moonbit_string_t)moonbit_string_literal_133.data;
  } else {
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4093 =
      _M0L4selfS1615->$0;
    void* _M0L7_2abindS1617;
    moonbit_incref(_M0L4dataS4093);
    #line 433 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1617
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4093, _M0L3keyS1616);
    moonbit_decref(_M0L4dataS4093);
    if (_M0L7_2abindS1617 == 0) {
      if (_M0L7_2abindS1617) {
        moonbit_decref(_M0L7_2abindS1617);
      }
      return (moonbit_string_t)moonbit_string_literal_133.data;
    } else {
      void* _M0L7_2aSomeS1618 = _M0L7_2abindS1617;
      void* _M0L4_2axS1619 = _M0L7_2aSomeS1618;
      switch (Moonbit_object_tag(_M0L4_2axS1619)) {
        case 0: {
          moonbit_decref(_M0L4_2axS1619);
          return (moonbit_string_t)moonbit_string_literal_134.data;
          break;
        }
        
        case 1: {
          moonbit_decref(_M0L4_2axS1619);
          return (moonbit_string_t)moonbit_string_literal_135.data;
          break;
        }
        
        case 2: {
          moonbit_decref(_M0L4_2axS1619);
          return (moonbit_string_t)moonbit_string_literal_136.data;
          break;
        }
        
        case 3: {
          moonbit_decref(_M0L4_2axS1619);
          return (moonbit_string_t)moonbit_string_literal_137.data;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1619);
          return (moonbit_string_t)moonbit_string_literal_138.data;
          break;
        }
      }
    }
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database4keys(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1607
) {
  moonbit_string_t* _M0L6_2atmpS4092;
  struct _M0TPB5ArrayGsE* _M0L6resultS1605;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4091;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L5_2aitS1606;
  #line 377 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4092 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1605
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1605)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6resultS1605->$0 = _M0L6_2atmpS4092;
  _M0L6resultS1605->$1 = 0;
  _M0L4dataS4091 = _M0L4selfS1607->$0;
  moonbit_incref(_M0L4dataS4091);
  #line 378 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1606
  = _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(_M0L4dataS4091);
  moonbit_decref(_M0L4dataS4091);
  while (1) {
    moonbit_string_t _M0L3keyS1609;
    struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2abindS1611;
    int32_t _M0L6_2atmpS4090;
    #line 379 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1611
    = _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(_M0L5_2aitS1606);
    if (_M0L7_2abindS1611 == 0) {
      if (_M0L7_2abindS1611) {
        moonbit_decref(_M0L7_2abindS1611);
      }
      moonbit_decref(_M0L5_2aitS1606);
    } else {
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1612 =
        _M0L7_2abindS1611;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L4_2axS1613 =
        _M0L7_2aSomeS1612;
      moonbit_string_t _M0L8_2afieldS4770 = _M0L4_2axS1613->$0;
      int32_t _M0L6_2acntS5222 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1613));
      moonbit_string_t _M0L6_2akeyS1614;
      if (_M0L6_2acntS5222 > 1) {
        int32_t _M0L11_2anew__cntS5224 = _M0L6_2acntS5222 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1613), _M0L11_2anew__cntS5224);
        moonbit_incref(_M0L8_2afieldS4770);
      } else if (_M0L6_2acntS5222 == 1) {
        void* _M0L8_2afieldS5223 = _M0L4_2axS1613->$1;
        moonbit_decref(_M0L8_2afieldS5223);
        #line 379 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1613);
      }
      _M0L6_2akeyS1614 = _M0L8_2afieldS4770;
      _M0L3keyS1609 = _M0L6_2akeyS1614;
      goto join_1608;
    }
    goto joinlet_5496;
    join_1608:;
    #line 380 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS4090
    = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1607, _M0L3keyS1609);
    if (!_M0L6_2atmpS4090) {
      #line 381 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1605, _M0L3keyS1609);
      moonbit_decref(_M0L3keyS1609);
    } else {
      moonbit_decref(_M0L3keyS1609);
    }
    continue;
    joinlet_5496:;
    break;
  }
  return _M0L6resultS1605;
}

int32_t _M0MP19moonbitDB8Database6exists(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1603,
  moonbit_string_t _M0L3keyS1604
) {
  #line 369 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 370 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1603, _M0L3keyS1604)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4089 =
      _M0L4selfS1603->$0;
    int32_t _result_5497;
    moonbit_incref(_M0L4dataS4089);
    #line 373 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_5497
    = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS4089, _M0L3keyS1604);
    moonbit_decref(_M0L4dataS4089);
    return _result_5497;
  }
}

int32_t _M0MP19moonbitDB8Database3del(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1600,
  moonbit_string_t _M0L3keyS1601
) {
  #line 356 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 357 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1600, _M0L3keyS1601)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4088 =
      _M0L4selfS1600->$0;
    int32_t _M0L7existedS1602;
    moonbit_incref(_M0L4dataS4088);
    #line 360 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7existedS1602
    = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS4088, _M0L3keyS1601);
    moonbit_decref(_M0L4dataS4088);
    if (_M0L7existedS1602) {
      struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4086 =
        _M0L4selfS1600->$0;
      struct _M0TPB3MapGsiE* _M0L7expiresS4087;
      moonbit_incref(_M0L4dataS4086);
      #line 362 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(_M0L4dataS4086, _M0L3keyS1601);
      moonbit_decref(_M0L4dataS4086);
      _M0L7expiresS4087 = _M0L4selfS1600->$1;
      moonbit_incref(_M0L7expiresS4087);
      #line 363 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map6removeGsiE(_M0L7expiresS4087, _M0L3keyS1601);
      moonbit_decref(_M0L7expiresS4087);
    }
    return _M0L7existedS1602;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database3get(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1590,
  moonbit_string_t _M0L3keyS1591
) {
  #line 345 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 346 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1590, _M0L3keyS1591)
  ) {
    return 0;
  } else {
    moonbit_string_t _M0L1sS1594;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4085 =
      _M0L4selfS1590->$0;
    void* _M0L7_2abindS1595;
    moonbit_incref(_M0L4dataS4085);
    #line 349 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1595
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4085, _M0L3keyS1591);
    moonbit_decref(_M0L4dataS4085);
    if (_M0L7_2abindS1595 == 0) {
      if (_M0L7_2abindS1595) {
        moonbit_decref(_M0L7_2abindS1595);
      }
      goto join_1592;
    } else {
      void* _M0L7_2aSomeS1596 = _M0L7_2abindS1595;
      void* _M0L4_2axS1597 = _M0L7_2aSomeS1596;
      switch (Moonbit_object_tag(_M0L4_2axS1597)) {
        case 0: {
          struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1598 =
            (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1597;
          moonbit_string_t _M0L8_2afieldS4776 = _M0L9_2aStringS1598->$0;
          int32_t _M0L6_2acntS5225 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1598));
          moonbit_string_t _M0L4_2asS1599;
          if (_M0L6_2acntS5225 > 1) {
            int32_t _M0L11_2anew__cntS5226 = _M0L6_2acntS5225 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1598), _M0L11_2anew__cntS5226);
            moonbit_incref(_M0L8_2afieldS4776);
          } else if (_M0L6_2acntS5225 == 1) {
            #line 349 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L9_2aStringS1598);
          }
          _M0L4_2asS1599 = _M0L8_2afieldS4776;
          _M0L1sS1594 = _M0L4_2asS1599;
          goto join_1593;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1597);
          goto join_1592;
          break;
        }
      }
    }
    join_1593:;
    return _M0L1sS1594;
    join_1592:;
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database4mdel(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1587,
  struct _M0TPB5ArrayGsE* _M0L4keysS1584
) {
  struct _M0TPB8MutLocalGiE* _M0L5countS1582;
  int32_t _M0L7_2abindS1583;
  int32_t _M0L2__S1585;
  int32_t _result_5501;
  #line 291 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5countS1582
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5countS1582)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5countS1582->$0 = 0;
  _M0L7_2abindS1583 = _M0L4keysS1584->$1;
  _M0L2__S1585 = 0;
  while (1) {
    if (_M0L2__S1585 < _M0L7_2abindS1583) {
      moonbit_string_t* _M0L3bufS4084 = _M0L4keysS1584->$0;
      moonbit_string_t _M0L3keyS1586 =
        (moonbit_string_t)_M0L3bufS4084[_M0L2__S1585];
      int32_t _M0L6_2atmpS4077;
      int32_t _M0L6_2atmpS4083;
      moonbit_incref(_M0L3keyS1586);
      #line 294 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS4077
      = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1587, _M0L3keyS1586);
      if (!_M0L6_2atmpS4077) {
        struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4082 =
          _M0L4selfS1587->$0;
        int32_t _M0L7existedS1588;
        moonbit_incref(_M0L4dataS4082);
        #line 295 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L7existedS1588
        = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS4082, _M0L3keyS1586);
        moonbit_decref(_M0L4dataS4082);
        if (_M0L7existedS1588) {
          struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4078 =
            _M0L4selfS1587->$0;
          struct _M0TPB3MapGsiE* _M0L7expiresS4079;
          int32_t _M0L3valS4081;
          int32_t _M0L6_2atmpS4080;
          moonbit_incref(_M0L4dataS4078);
          #line 297 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(_M0L4dataS4078, _M0L3keyS1586);
          moonbit_decref(_M0L4dataS4078);
          _M0L7expiresS4079 = _M0L4selfS1587->$1;
          moonbit_incref(_M0L7expiresS4079);
          #line 298 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0MPB3Map6removeGsiE(_M0L7expiresS4079, _M0L3keyS1586);
          moonbit_decref(_M0L7expiresS4079);
          moonbit_decref(_M0L3keyS1586);
          _M0L3valS4081 = _M0L5countS1582->$0;
          _M0L6_2atmpS4080 = _M0L3valS4081 + 1;
          _M0L5countS1582->$0 = _M0L6_2atmpS4080;
        } else {
          moonbit_decref(_M0L3keyS1586);
        }
      } else {
        moonbit_decref(_M0L3keyS1586);
      }
      _M0L6_2atmpS4083 = _M0L2__S1585 + 1;
      _M0L2__S1585 = _M0L6_2atmpS4083;
      continue;
    }
    break;
  }
  _result_5501 = _M0L5countS1582->$0;
  moonbit_decref(_M0L5countS1582);
  return _result_5501;
}

struct _M0TPB5ArrayGOsE* _M0MP19moonbitDB8Database4mget(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1572,
  struct _M0TPB5ArrayGsE* _M0L4keysS1569
) {
  moonbit_string_t* _M0L6_2atmpS4076;
  struct _M0TPB5ArrayGOsE* _M0L6resultS1567;
  int32_t _M0L7_2abindS1568;
  int32_t _M0L2__S1570;
  #line 276 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4076 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1567
  = (struct _M0TPB5ArrayGOsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGOsE));
  Moonbit_object_header(_M0L6resultS1567)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 24, 0);
  _M0L6resultS1567->$0 = _M0L6_2atmpS4076;
  _M0L6resultS1567->$1 = 0;
  _M0L7_2abindS1568 = _M0L4keysS1569->$1;
  _M0L2__S1570 = 0;
  while (1) {
    if (_M0L2__S1570 < _M0L7_2abindS1568) {
      moonbit_string_t* _M0L3bufS4075 = _M0L4keysS1569->$0;
      moonbit_string_t _M0L3keyS1571 =
        (moonbit_string_t)_M0L3bufS4075[_M0L2__S1570];
      int32_t _M0L6_2atmpS4074;
      moonbit_incref(_M0L3keyS1571);
      #line 279 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      if (
        _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1572, _M0L3keyS1571)
      ) {
        moonbit_string_t _M0L6_2atmpS4070;
        moonbit_decref(_M0L3keyS1571);
        _M0L6_2atmpS4070 = 0;
        #line 280 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1567, _M0L6_2atmpS4070);
        if (_M0L6_2atmpS4070) {
          moonbit_decref(_M0L6_2atmpS4070);
        }
      } else {
        moonbit_string_t _M0L1sS1575;
        struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4073 =
          _M0L4selfS1572->$0;
        void* _M0L7_2abindS1576;
        moonbit_string_t _M0L6_2atmpS4072;
        moonbit_string_t _M0L6_2atmpS4071;
        moonbit_incref(_M0L4dataS4073);
        #line 282 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L7_2abindS1576
        = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS4073, _M0L3keyS1571);
        moonbit_decref(_M0L4dataS4073);
        moonbit_decref(_M0L3keyS1571);
        if (_M0L7_2abindS1576 == 0) {
          if (_M0L7_2abindS1576) {
            moonbit_decref(_M0L7_2abindS1576);
          }
          goto join_1573;
        } else {
          void* _M0L7_2aSomeS1577 = _M0L7_2abindS1576;
          void* _M0L4_2axS1578 = _M0L7_2aSomeS1577;
          switch (Moonbit_object_tag(_M0L4_2axS1578)) {
            case 0: {
              struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1579 =
                (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1578;
              moonbit_string_t _M0L8_2afieldS4783 = _M0L9_2aStringS1579->$0;
              int32_t _M0L6_2acntS5227 =
                Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1579));
              moonbit_string_t _M0L4_2asS1580;
              if (_M0L6_2acntS5227 > 1) {
                int32_t _M0L11_2anew__cntS5228 = _M0L6_2acntS5227 - 1;
                Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1579), _M0L11_2anew__cntS5228);
                moonbit_incref(_M0L8_2afieldS4783);
              } else if (_M0L6_2acntS5227 == 1) {
                #line 282 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
                moonbit_free(_M0L9_2aStringS1579);
              }
              _M0L4_2asS1580 = _M0L8_2afieldS4783;
              _M0L1sS1575 = _M0L4_2asS1580;
              goto join_1574;
              break;
            }
            default: {
              moonbit_decref(_M0L4_2axS1578);
              goto join_1573;
              break;
            }
          }
        }
        goto joinlet_5504;
        join_1574:;
        _M0L6_2atmpS4072 = _M0L1sS1575;
        #line 283 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1567, _M0L6_2atmpS4072);
        if (_M0L6_2atmpS4072) {
          moonbit_decref(_M0L6_2atmpS4072);
        }
        joinlet_5504:;
        goto joinlet_5503;
        join_1573:;
        _M0L6_2atmpS4071 = 0;
        #line 284 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1567, _M0L6_2atmpS4071);
        if (_M0L6_2atmpS4071) {
          moonbit_decref(_M0L6_2atmpS4071);
        }
        joinlet_5503:;
      }
      _M0L6_2atmpS4074 = _M0L2__S1570 + 1;
      _M0L2__S1570 = _M0L6_2atmpS4074;
      continue;
    }
    break;
  }
  return _M0L6resultS1567;
}

int32_t _M0MP19moonbitDB8Database4mset(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1565,
  struct _M0TPB5ArrayGsE* _M0L4keysS1563,
  struct _M0TPB5ArrayGsE* _M0L6valuesS1564
) {
  int32_t _M0L1iS1562;
  #line 269 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L1iS1562 = 0;
  while (1) {
    int32_t _M0L6_2atmpS4062;
    int32_t _if__result_5506;
    #line 270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS4062 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1563);
    if (_M0L1iS1562 < _M0L6_2atmpS4062) {
      int32_t _M0L6_2atmpS4061;
      #line 270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS4061 = _M0MPC15array5Array6lengthGsE(_M0L6valuesS1564);
      _if__result_5506 = _M0L1iS1562 < _M0L6_2atmpS4061;
    } else {
      _if__result_5506 = 0;
    }
    if (_if__result_5506) {
      struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4063 =
        _M0L4selfS1565->$0;
      moonbit_string_t _M0L6_2atmpS4064;
      moonbit_string_t _M0L6_2atmpS4066;
      void* _M0L6StringS4065;
      struct _M0TPB3MapGsiE* _M0L7expiresS4067;
      moonbit_string_t _M0L6_2atmpS4068;
      int32_t _M0L6_2atmpS4069;
      moonbit_incref(_M0L4dataS4063);
      #line 271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS4064
      = _M0MPC15array5Array2atGsE(_M0L4keysS1563, _M0L1iS1562);
      #line 271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS4066
      = _M0MPC15array5Array2atGsE(_M0L6valuesS1564, _M0L1iS1562);
      _M0L6StringS4065
      = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
      Moonbit_object_header(_M0L6StringS4065)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
      ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS4065)->$0
      = _M0L6_2atmpS4066;
      #line 271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4063, _M0L6_2atmpS4064, _M0L6StringS4065);
      moonbit_decref(_M0L4dataS4063);
      moonbit_decref(_M0L6_2atmpS4064);
      moonbit_decref(_M0L6StringS4065);
      _M0L7expiresS4067 = _M0L4selfS1565->$1;
      moonbit_incref(_M0L7expiresS4067);
      #line 272 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS4068
      = _M0MPC15array5Array2atGsE(_M0L4keysS1563, _M0L1iS1562);
      #line 272 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map6removeGsiE(_M0L7expiresS4067, _M0L6_2atmpS4068);
      moonbit_decref(_M0L7expiresS4067);
      moonbit_decref(_M0L6_2atmpS4068);
      _M0L6_2atmpS4069 = _M0L1iS1562 + 1;
      _M0L1iS1562 = _M0L6_2atmpS4069;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MP19moonbitDB8Database7persist(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1560,
  moonbit_string_t _M0L3keyS1561
) {
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4059;
  int32_t _result_5507;
  int32_t _if__result_5508;
  #line 260 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4dataS4059 = _M0L4selfS1560->$0;
  moonbit_incref(_M0L4dataS4059);
  #line 261 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_5507
  = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS4059, _M0L3keyS1561);
  moonbit_decref(_M0L4dataS4059);
  if (_result_5507) {
    struct _M0TPB3MapGsiE* _M0L7expiresS4058 = _M0L4selfS1560->$1;
    moonbit_incref(_M0L7expiresS4058);
    #line 261 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _if__result_5508
    = _M0MPB3Map8containsGsiE(_M0L7expiresS4058, _M0L3keyS1561);
    moonbit_decref(_M0L7expiresS4058);
  } else {
    _if__result_5508 = 0;
  }
  if (_if__result_5508) {
    struct _M0TPB3MapGsiE* _M0L7expiresS4060 = _M0L4selfS1560->$1;
    moonbit_incref(_M0L7expiresS4060);
    #line 262 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map6removeGsiE(_M0L7expiresS4060, _M0L3keyS1561);
    moonbit_decref(_M0L7expiresS4060);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database3ttl(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1557,
  moonbit_string_t _M0L3keyS1558
) {
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4050;
  int32_t _M0L6_2atmpS4049;
  #line 226 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4dataS4050 = _M0L4selfS1557->$0;
  moonbit_incref(_M0L4dataS4050);
  #line 227 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4049
  = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS4050, _M0L3keyS1558);
  moonbit_decref(_M0L4dataS4050);
  if (!_M0L6_2atmpS4049) {
    return -2;
  } else {
    struct _M0TPB3MapGsiE* _M0L7expiresS4051 = _M0L4selfS1557->$1;
    int32_t _result_5509;
    moonbit_incref(_M0L7expiresS4051);
    #line 229 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_5509 = _M0MPB3Map8containsGsiE(_M0L7expiresS4051, _M0L3keyS1558);
    moonbit_decref(_M0L7expiresS4051);
    if (_result_5509) {
      struct _M0TPB3MapGsiE* _M0L7expiresS4057 = _M0L4selfS1557->$1;
      int64_t _M0L6_2atmpS4056;
      int32_t _M0L6_2atmpS4054;
      int32_t _M0L13current__timeS4055;
      int32_t _M0L9remainingS1559;
      moonbit_incref(_M0L7expiresS4057);
      #line 230 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS4056 = _M0MPB3Map3getGsiE(_M0L7expiresS4057, _M0L3keyS1558);
      moonbit_decref(_M0L7expiresS4057);
      #line 230 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS4054 = _M0MPC16option6Option6unwrapGiE(_M0L6_2atmpS4056);
      _M0L13current__timeS4055 = _M0L4selfS1557->$2;
      _M0L9remainingS1559 = _M0L6_2atmpS4054 - _M0L13current__timeS4055;
      if (_M0L9remainingS1559 <= 0) {
        struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4052 =
          _M0L4selfS1557->$0;
        struct _M0TPB3MapGsiE* _M0L7expiresS4053;
        moonbit_incref(_M0L4dataS4052);
        #line 232 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(_M0L4dataS4052, _M0L3keyS1558);
        moonbit_decref(_M0L4dataS4052);
        _M0L7expiresS4053 = _M0L4selfS1557->$1;
        moonbit_incref(_M0L7expiresS4053);
        #line 233 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPB3Map6removeGsiE(_M0L7expiresS4053, _M0L3keyS1558);
        moonbit_decref(_M0L7expiresS4053);
        return -2;
      } else {
        return _M0L9remainingS1559 / 1000;
      }
    } else {
      return -1;
    }
  }
}

int32_t _M0MP19moonbitDB8Database6expire(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1554,
  moonbit_string_t _M0L3keyS1555,
  int32_t _M0L7secondsS1556
) {
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4044;
  int32_t _result_5510;
  #line 208 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4dataS4044 = _M0L4selfS1554->$0;
  moonbit_incref(_M0L4dataS4044);
  #line 209 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_5510
  = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS4044, _M0L3keyS1555);
  moonbit_decref(_M0L4dataS4044);
  if (_result_5510) {
    struct _M0TPB3MapGsiE* _M0L7expiresS4045 = _M0L4selfS1554->$1;
    int32_t _M0L13current__timeS4047 = _M0L4selfS1554->$2;
    int32_t _M0L6_2atmpS4048 = _M0L7secondsS1556 * 1000;
    int32_t _M0L6_2atmpS4046 = _M0L13current__timeS4047 + _M0L6_2atmpS4048;
    moonbit_incref(_M0L7expiresS4045);
    #line 210 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsiE(_M0L7expiresS4045, _M0L3keyS1555, _M0L6_2atmpS4046);
    moonbit_decref(_M0L7expiresS4045);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database3set(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1551,
  moonbit_string_t _M0L3keyS1552,
  moonbit_string_t _M0L5valueS1553
) {
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4041;
  void* _M0L6StringS4042;
  struct _M0TPB3MapGsiE* _M0L7expiresS4043;
  #line 203 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4dataS4041 = _M0L4selfS1551->$0;
  moonbit_incref(_M0L5valueS1553);
  _M0L6StringS4042
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
  Moonbit_object_header(_M0L6StringS4042)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
  ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS4042)->$0
  = _M0L5valueS1553;
  moonbit_incref(_M0L4dataS4041);
  #line 204 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS4041, _M0L3keyS1552, _M0L6StringS4042);
  moonbit_decref(_M0L4dataS4041);
  moonbit_decref(_M0L6StringS4042);
  _M0L7expiresS4043 = _M0L4selfS1551->$1;
  moonbit_incref(_M0L7expiresS4043);
  #line 205 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS4043, _M0L3keyS1552);
  moonbit_decref(_M0L7expiresS4043);
  return 0;
}

int32_t _M0MP19moonbitDB8Database14check__expired(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1546,
  moonbit_string_t _M0L3keyS1547
) {
  int32_t _M0L12expire__timeS1545;
  struct _M0TPB3MapGsiE* _M0L7expiresS4040;
  int64_t _M0L7_2abindS1548;
  int32_t _M0L13current__timeS4037;
  #line 188 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7expiresS4040 = _M0L4selfS1546->$1;
  moonbit_incref(_M0L7expiresS4040);
  #line 189 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1548 = _M0MPB3Map3getGsiE(_M0L7expiresS4040, _M0L3keyS1547);
  moonbit_decref(_M0L7expiresS4040);
  if (_M0L7_2abindS1548 == 4294967296ll) {
    return 0;
  } else {
    int64_t _M0L7_2aSomeS1549 = _M0L7_2abindS1548;
    int32_t _M0L15_2aexpire__timeS1550 = (int32_t)_M0L7_2aSomeS1549;
    _M0L12expire__timeS1545 = _M0L15_2aexpire__timeS1550;
    goto join_1544;
  }
  join_1544:;
  _M0L13current__timeS4037 = _M0L4selfS1546->$2;
  if (_M0L12expire__timeS1545 <= _M0L13current__timeS4037) {
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS4038 =
      _M0L4selfS1546->$0;
    struct _M0TPB3MapGsiE* _M0L7expiresS4039;
    moonbit_incref(_M0L4dataS4038);
    #line 192 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(_M0L4dataS4038, _M0L3keyS1547);
    moonbit_decref(_M0L4dataS4038);
    _M0L7expiresS4039 = _M0L4selfS1546->$1;
    moonbit_incref(_M0L7expiresS4039);
    #line 193 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map6removeGsiE(_M0L7expiresS4039, _M0L3keyS1547);
    moonbit_decref(_M0L7expiresS4039);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database13advance__time(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1542,
  int32_t _M0L2msS1543
) {
  int32_t _M0L13current__timeS4036;
  int32_t _M0L6_2atmpS4035;
  #line 184 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L13current__timeS4036 = _M0L4selfS1542->$2;
  _M0L6_2atmpS4035 = _M0L13current__timeS4036 + _M0L2msS1543;
  _M0L4selfS1542->$2 = _M0L6_2atmpS4035;
  return 0;
}

struct _M0TP19moonbitDB8Database* _M0MP19moonbitDB8Database3new() {
  struct _M0TUsRP19moonbitDB10RedisValueE** _M0L7_2abindS1540;
  struct _M0TUsRP19moonbitDB10RedisValueE** _M0L6_2atmpS4034;
  struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE _M0L6_2atmpS4033;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS4029;
  struct _M0TUsiE** _M0L7_2abindS1541;
  struct _M0TUsiE** _M0L6_2atmpS4032;
  struct _M0TPB9ArrayViewGUsiEE _M0L6_2atmpS4031;
  struct _M0TPB3MapGsiE* _M0L6_2atmpS4030;
  struct _M0TP19moonbitDB8Database* _block_5512;
  #line 176 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1540
  = (struct _M0TUsRP19moonbitDB10RedisValueE**)moonbit_empty_ref_array;
  _M0L6_2atmpS4034 = _M0L7_2abindS1540;
  _M0L6_2atmpS4033
  = (struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE){
    .$0 = _M0L6_2atmpS4034, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4029
  = _M0MPB3Map3MapGsRP19moonbitDB10RedisValueE(_M0L6_2atmpS4033, 1000ll);
  moonbit_decref(_M0L6_2atmpS4033.$0);
  _M0L7_2abindS1541 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L6_2atmpS4032 = _M0L7_2abindS1541;
  _M0L6_2atmpS4031
  = (struct _M0TPB9ArrayViewGUsiEE){
    .$0 = _M0L6_2atmpS4032, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4030 = _M0MPB3Map3MapGsiE(_M0L6_2atmpS4031, 1000ll);
  moonbit_decref(_M0L6_2atmpS4031.$0);
  _block_5512
  = (struct _M0TP19moonbitDB8Database*)moonbit_malloc(sizeof(struct _M0TP19moonbitDB8Database));
  Moonbit_object_header(_block_5512)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 27, 0);
  _block_5512->$0 = _M0L6_2atmpS4029;
  _block_5512->$1 = _M0L6_2atmpS4030;
  _block_5512->$2 = 0;
  return _block_5512;
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB5Deque9to__array(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1533
) {
  moonbit_string_t* _M0L6_2atmpS4028;
  struct _M0TPB5ArrayGsE* _M0L6resultS1531;
  struct _M0TPB5ArrayGsE* _M0L5frontS4025;
  int32_t _M0L6_2atmpS4024;
  int32_t _M0L6_2atmpS4023;
  int32_t _M0L1iS1532;
  struct _M0TPB5ArrayGsE* _M0L7_2abindS1535;
  int32_t _M0L7_2abindS1536;
  int32_t _M0L2__S1537;
  #line 48 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4028 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1531
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1531)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6resultS1531->$0 = _M0L6_2atmpS4028;
  _M0L6resultS1531->$1 = 0;
  _M0L5frontS4025 = _M0L4selfS1533->$0;
  moonbit_incref(_M0L5frontS4025);
  #line 50 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4024 = _M0MPC15array5Array6lengthGsE(_M0L5frontS4025);
  moonbit_decref(_M0L5frontS4025);
  _M0L6_2atmpS4023 = _M0L6_2atmpS4024 - 1;
  _M0L1iS1532 = _M0L6_2atmpS4023;
  while (1) {
    if (_M0L1iS1532 >= 0) {
      struct _M0TPB5ArrayGsE* _M0L5frontS4021 = _M0L4selfS1533->$0;
      moonbit_string_t _M0L6_2atmpS4020;
      int32_t _M0L6_2atmpS4022;
      moonbit_incref(_M0L5frontS4021);
      #line 51 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS4020
      = _M0MPC15array5Array2atGsE(_M0L5frontS4021, _M0L1iS1532);
      moonbit_decref(_M0L5frontS4021);
      #line 51 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1531, _M0L6_2atmpS4020);
      moonbit_decref(_M0L6_2atmpS4020);
      _M0L6_2atmpS4022 = _M0L1iS1532 - 1;
      _M0L1iS1532 = _M0L6_2atmpS4022;
      continue;
    }
    break;
  }
  _M0L7_2abindS1535 = _M0L4selfS1533->$1;
  _M0L7_2abindS1536 = _M0L7_2abindS1535->$1;
  moonbit_incref(_M0L7_2abindS1535);
  _M0L2__S1537 = 0;
  while (1) {
    if (_M0L2__S1537 < _M0L7_2abindS1536) {
      moonbit_string_t* _M0L3bufS4027 = _M0L7_2abindS1535->$0;
      moonbit_string_t _M0L4itemS1538 =
        (moonbit_string_t)_M0L3bufS4027[_M0L2__S1537];
      int32_t _M0L6_2atmpS4026;
      moonbit_incref(_M0L4itemS1538);
      #line 54 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1531, _M0L4itemS1538);
      moonbit_decref(_M0L4itemS1538);
      _M0L6_2atmpS4026 = _M0L2__S1537 + 1;
      _M0L2__S1537 = _M0L6_2atmpS4026;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1535);
    }
    break;
  }
  return _M0L6resultS1531;
}

int32_t _M0MP19moonbitDB5Deque6length(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1530
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS4019;
  int32_t _M0L6_2atmpS4016;
  struct _M0TPB5ArrayGsE* _M0L4backS4018;
  int32_t _M0L6_2atmpS4017;
  #line 44 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5frontS4019 = _M0L4selfS1530->$0;
  moonbit_incref(_M0L5frontS4019);
  #line 45 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4016 = _M0MPC15array5Array6lengthGsE(_M0L5frontS4019);
  moonbit_decref(_M0L5frontS4019);
  _M0L4backS4018 = _M0L4selfS1530->$1;
  moonbit_incref(_M0L4backS4018);
  #line 45 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4017 = _M0MPC15array5Array6lengthGsE(_M0L4backS4018);
  moonbit_decref(_M0L4backS4018);
  return _M0L6_2atmpS4016 + _M0L6_2atmpS4017;
}

moonbit_string_t _M0MP19moonbitDB5Deque9pop__back(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1523
) {
  struct _M0TPB5ArrayGsE* _M0L4backS4010;
  int32_t _M0L6_2atmpS4009;
  struct _M0TPB5ArrayGsE* _M0L4backS4015;
  moonbit_string_t _result_5517;
  #line 31 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4backS4010 = _M0L4selfS1523->$1;
  moonbit_incref(_M0L4backS4010);
  #line 32 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4009 = _M0MPC15array5Array6lengthGsE(_M0L4backS4010);
  moonbit_decref(_M0L4backS4010);
  if (_M0L6_2atmpS4009 == 0) {
    while (1) {
      struct _M0TPB5ArrayGsE* _M0L5frontS4012 = _M0L4selfS1523->$0;
      int32_t _M0L6_2atmpS4011;
      moonbit_incref(_M0L5frontS4012);
      #line 33 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS4011 = _M0MPC15array5Array6lengthGsE(_M0L5frontS4012);
      moonbit_decref(_M0L5frontS4012);
      if (_M0L6_2atmpS4011 > 0) {
        struct _M0TPB5ArrayGsE* _M0L5frontS4014 = _M0L4selfS1523->$0;
        moonbit_string_t _M0L4itemS1524;
        moonbit_string_t _M0L1vS1526;
        struct _M0TPB5ArrayGsE* _M0L4backS4013;
        moonbit_incref(_M0L5frontS4014);
        #line 34 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L4itemS1524 = _M0MPC15array5Array3popGsE(_M0L5frontS4014);
        moonbit_decref(_M0L5frontS4014);
        if (_M0L4itemS1524 == 0) {
          if (_M0L4itemS1524) {
            moonbit_decref(_M0L4itemS1524);
          }
          break;
        } else {
          moonbit_string_t _M0L7_2aSomeS1527 = _M0L4itemS1524;
          moonbit_string_t _M0L4_2avS1528 = _M0L7_2aSomeS1527;
          _M0L1vS1526 = _M0L4_2avS1528;
          goto join_1525;
        }
        goto joinlet_5516;
        join_1525:;
        _M0L4backS4013 = _M0L4selfS1523->$1;
        moonbit_incref(_M0L4backS4013);
        #line 36 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L4backS4013, _M0L1vS1526);
        moonbit_decref(_M0L4backS4013);
        moonbit_decref(_M0L1vS1526);
        joinlet_5516:;
        continue;
      }
      break;
    }
  }
  _M0L4backS4015 = _M0L4selfS1523->$1;
  moonbit_incref(_M0L4backS4015);
  #line 41 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_5517 = _M0MPC15array5Array3popGsE(_M0L4backS4015);
  moonbit_decref(_M0L4backS4015);
  return _result_5517;
}

moonbit_string_t _M0MP19moonbitDB5Deque10pop__front(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1516
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS4003;
  int32_t _M0L6_2atmpS4002;
  struct _M0TPB5ArrayGsE* _M0L5frontS4008;
  moonbit_string_t _result_5520;
  #line 18 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5frontS4003 = _M0L4selfS1516->$0;
  moonbit_incref(_M0L5frontS4003);
  #line 19 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS4002 = _M0MPC15array5Array6lengthGsE(_M0L5frontS4003);
  moonbit_decref(_M0L5frontS4003);
  if (_M0L6_2atmpS4002 == 0) {
    while (1) {
      struct _M0TPB5ArrayGsE* _M0L4backS4005 = _M0L4selfS1516->$1;
      int32_t _M0L6_2atmpS4004;
      moonbit_incref(_M0L4backS4005);
      #line 20 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS4004 = _M0MPC15array5Array6lengthGsE(_M0L4backS4005);
      moonbit_decref(_M0L4backS4005);
      if (_M0L6_2atmpS4004 > 0) {
        struct _M0TPB5ArrayGsE* _M0L4backS4007 = _M0L4selfS1516->$1;
        moonbit_string_t _M0L4itemS1517;
        moonbit_string_t _M0L1vS1519;
        struct _M0TPB5ArrayGsE* _M0L5frontS4006;
        moonbit_incref(_M0L4backS4007);
        #line 21 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L4itemS1517 = _M0MPC15array5Array3popGsE(_M0L4backS4007);
        moonbit_decref(_M0L4backS4007);
        if (_M0L4itemS1517 == 0) {
          if (_M0L4itemS1517) {
            moonbit_decref(_M0L4itemS1517);
          }
          break;
        } else {
          moonbit_string_t _M0L7_2aSomeS1520 = _M0L4itemS1517;
          moonbit_string_t _M0L4_2avS1521 = _M0L7_2aSomeS1520;
          _M0L1vS1519 = _M0L4_2avS1521;
          goto join_1518;
        }
        goto joinlet_5519;
        join_1518:;
        _M0L5frontS4006 = _M0L4selfS1516->$0;
        moonbit_incref(_M0L5frontS4006);
        #line 23 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L5frontS4006, _M0L1vS1519);
        moonbit_decref(_M0L5frontS4006);
        moonbit_decref(_M0L1vS1519);
        joinlet_5519:;
        continue;
      }
      break;
    }
  }
  _M0L5frontS4008 = _M0L4selfS1516->$0;
  moonbit_incref(_M0L5frontS4008);
  #line 28 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_5520 = _M0MPC15array5Array3popGsE(_M0L5frontS4008);
  moonbit_decref(_M0L5frontS4008);
  return _result_5520;
}

int32_t _M0MP19moonbitDB5Deque10push__back(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1514,
  moonbit_string_t _M0L5valueS1515
) {
  struct _M0TPB5ArrayGsE* _M0L4backS4001;
  #line 14 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4backS4001 = _M0L4selfS1514->$1;
  moonbit_incref(_M0L4backS4001);
  #line 15 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPC15array5Array4pushGsE(_M0L4backS4001, _M0L5valueS1515);
  moonbit_decref(_M0L4backS4001);
  return 0;
}

int32_t _M0MP19moonbitDB5Deque11push__front(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1512,
  moonbit_string_t _M0L5valueS1513
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS4000;
  #line 10 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5frontS4000 = _M0L4selfS1512->$0;
  moonbit_incref(_M0L5frontS4000);
  #line 11 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPC15array5Array4pushGsE(_M0L5frontS4000, _M0L5valueS1513);
  moonbit_decref(_M0L5frontS4000);
  return 0;
}

struct _M0TP19moonbitDB5Deque* _M0MP19moonbitDB5Deque3new() {
  moonbit_string_t* _M0L6_2atmpS3999;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3996;
  moonbit_string_t* _M0L6_2atmpS3998;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3997;
  struct _M0TP19moonbitDB5Deque* _block_5521;
  #line 6 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3999 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3996
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3996)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6_2atmpS3996->$0 = _M0L6_2atmpS3999;
  _M0L6_2atmpS3996->$1 = 0;
  _M0L6_2atmpS3998 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3997
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3997)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6_2atmpS3997->$0 = _M0L6_2atmpS3998;
  _M0L6_2atmpS3997->$1 = 0;
  _block_5521
  = (struct _M0TP19moonbitDB5Deque*)moonbit_malloc(sizeof(struct _M0TP19moonbitDB5Deque));
  Moonbit_object_header(_block_5521)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 31, 0);
  _block_5521->$0 = _M0L6_2atmpS3996;
  _block_5521->$1 = _M0L6_2atmpS3997;
  return _block_5521;
}

moonbit_string_t _M0IPC15float5FloatPB4Show10to__string(float _M0L4selfS1511) {
  double _M0L6_2atmpS3995;
  #line 16 "/home/developer/.moon/lib/core/float/methods.mbt"
  _M0L6_2atmpS3995 = (double)_M0L4selfS1511;
  #line 17 "/home/developer/.moon/lib/core/float/methods.mbt"
  return _M0MPC16double6Double10to__string(_M0L6_2atmpS3995);
}

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1509,
  struct _M0TPC16string10StringView _M0L9separatorS1510
) {
  moonbit_string_t* _M0L3bufS3993;
  int32_t _M0L3lenS3994;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS3992;
  moonbit_string_t _result_5522;
  #line 2184 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3bufS3993 = _M0L4selfS1509->$0;
  _M0L3lenS3994 = _M0L4selfS1509->$1;
  moonbit_incref(_M0L3bufS3993);
  _M0L6_2atmpS3992
  = (struct _M0TPB9ArrayViewGsE){
    .$0 = _M0L3bufS3993, .$1 = 0, .$2 = _M0L3lenS3994
  };
  #line 2188 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _result_5522
  = _M0MPC15array9ArrayView4joinGsE(_M0L6_2atmpS3992, _M0L9separatorS1510);
  moonbit_decref(_M0L6_2atmpS3992.$0);
  return _result_5522;
}

moonbit_string_t _M0MPC15array5Array3popGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1506
) {
  int32_t _M0L3lenS1505;
  #line 325 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1505 = _M0L4selfS1506->$1;
  if (_M0L3lenS1505 == 0) {
    return 0;
  } else {
    int32_t _M0L5indexS1507 = _M0L3lenS1505 - 1;
    moonbit_string_t* _M0L3bufS3991 = _M0L4selfS1506->$0;
    moonbit_string_t _M0L1vS1508 =
      (moonbit_string_t)_M0L3bufS3991[_M0L5indexS1507];
    moonbit_string_t* _M0L3bufS3990 = _M0L4selfS1506->$0;
    moonbit_string_t _M0L6_2aoldS4824;
    if (
      _M0L5indexS1507 < 0
      || _M0L5indexS1507 >= Moonbit_array_length(_M0L3bufS3990)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4824 = (moonbit_string_t)_M0L3bufS3990[_M0L5indexS1507];
    moonbit_incref(_M0L1vS1508);
    moonbit_decref(_M0L6_2aoldS4824);
    if (
      _M0L5indexS1507 < 0
      || _M0L5indexS1507 >= Moonbit_array_length(_M0L3bufS3990)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS3990[_M0L5indexS1507]
    = (moonbit_string_t)moonbit_string_literal_75.data;
    _M0L4selfS1506->$1 = _M0L5indexS1507;
    return _M0L1vS1508;
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1497,
  int32_t _M0L5indexS1498
) {
  int32_t _M0L3lenS1496;
  int32_t _if__result_5523;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1496 = _M0L4selfS1497->$1;
  if (_M0L5indexS1498 >= 0) {
    _if__result_5523 = _M0L5indexS1498 < _M0L3lenS1496;
  } else {
    _if__result_5523 = 0;
  }
  if (_if__result_5523) {
    moonbit_string_t* _M0L6_2atmpS3987;
    moonbit_string_t _M0L6_2atmpS4828;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3987 = _M0MPC15array5Array6bufferGsE(_M0L4selfS1497);
    _M0L6_2atmpS4828 = (moonbit_string_t)_M0L6_2atmpS3987[_M0L5indexS1498];
    moonbit_incref(_M0L6_2atmpS4828);
    moonbit_decref(_M0L6_2atmpS3987);
    return _M0L6_2atmpS4828;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t _M0MPC15array5Array2atGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS1500,
  int32_t _M0L5indexS1501
) {
  int32_t _M0L3lenS1499;
  int32_t _if__result_5524;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1499 = _M0L4selfS1500->$1;
  if (_M0L5indexS1501 >= 0) {
    _if__result_5524 = _M0L5indexS1501 < _M0L3lenS1499;
  } else {
    _if__result_5524 = 0;
  }
  if (_if__result_5524) {
    moonbit_string_t* _M0L6_2atmpS3988;
    moonbit_string_t _M0L6_2atmpS4829;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3988 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS1500);
    _M0L6_2atmpS4829 = (moonbit_string_t)_M0L6_2atmpS3988[_M0L5indexS1501];
    if (_M0L6_2atmpS4829) {
      moonbit_incref(_M0L6_2atmpS4829);
    }
    moonbit_decref(_M0L6_2atmpS3988);
    return _M0L6_2atmpS4829;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

struct _M0TUsfE* _M0MPC15array5Array2atGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS1503,
  int32_t _M0L5indexS1504
) {
  int32_t _M0L3lenS1502;
  int32_t _if__result_5525;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1502 = _M0L4selfS1503->$1;
  if (_M0L5indexS1504 >= 0) {
    _if__result_5525 = _M0L5indexS1504 < _M0L3lenS1502;
  } else {
    _if__result_5525 = 0;
  }
  if (_if__result_5525) {
    struct _M0TUsfE** _M0L6_2atmpS3989;
    struct _M0TUsfE* _M0L6_2atmpS4830;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3989 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS1503);
    _M0L6_2atmpS4830 = (struct _M0TUsfE*)_M0L6_2atmpS3989[_M0L5indexS1504];
    if (_M0L6_2atmpS4830) {
      moonbit_incref(_M0L6_2atmpS4830);
    }
    moonbit_decref(_M0L6_2atmpS3989);
    return _M0L6_2atmpS4830;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1495) {
  moonbit_string_t _M0L6_2atmpS3986;
  #line 36 "/home/developer/.moon/lib/core/builtin/console.mbt"
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS3986
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1495);
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS3986);
  moonbit_decref(_M0L6_2atmpS3986);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1494) {
  #line 282 "/home/developer/.moon/lib/core/builtin/double.mbt"
  #line 284 "/home/developer/.moon/lib/core/builtin/double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1494);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1481) {
  uint64_t _M0L4bitsS1482;
  uint64_t _M0L6_2atmpS3985;
  uint64_t _M0L6_2atmpS3984;
  int32_t _M0L8ieeeSignS1483;
  uint64_t _M0L12ieeeMantissaS1484;
  uint64_t _M0L6_2atmpS3983;
  uint64_t _M0L6_2atmpS3982;
  int32_t _M0L12ieeeExponentS1485;
  int32_t _if__result_5526;
  struct _M0TPB17FloatingDecimal64* _M0L7_2abindS1486;
  struct _M0TPB17FloatingDecimal64* _M0L1vS1487;
  moonbit_string_t _result_5528;
  #line 659 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L3valS1481 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_122.data;
  }
  _M0L4bitsS1482 = *(int64_t*)&_M0L3valS1481;
  _M0L6_2atmpS3985 = _M0L4bitsS1482 >> 63;
  _M0L6_2atmpS3984 = _M0L6_2atmpS3985 & 1ull;
  _M0L8ieeeSignS1483 = _M0L6_2atmpS3984 != 0ull;
  _M0L12ieeeMantissaS1484 = _M0L4bitsS1482 & 4503599627370495ull;
  _M0L6_2atmpS3983 = _M0L4bitsS1482 >> 52;
  _M0L6_2atmpS3982 = _M0L6_2atmpS3983 & 2047ull;
  _M0L12ieeeExponentS1485 = (int32_t)_M0L6_2atmpS3982;
  if (_M0L12ieeeExponentS1485 == 2047) {
    _if__result_5526 = 1;
  } else if (_M0L12ieeeExponentS1485 == 0) {
    _if__result_5526 = _M0L12ieeeMantissaS1484 == 0ull;
  } else {
    _if__result_5526 = 0;
  }
  if (_if__result_5526) {
    int32_t _M0L6_2atmpS3973 = _M0L12ieeeExponentS1485 != 0;
    int32_t _M0L6_2atmpS3974 = _M0L12ieeeMantissaS1484 != 0ull;
    #line 676 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1483, _M0L6_2atmpS3973, _M0L6_2atmpS3974);
  }
  #line 678 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1486
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1484, _M0L12ieeeExponentS1485);
  if (_M0L7_2abindS1486 == 0) {
    uint32_t _M0L6_2atmpS3975;
    if (_M0L7_2abindS1486) {
      moonbit_decref(_M0L7_2abindS1486);
    }
    _M0L6_2atmpS3975 = *(uint32_t*)&_M0L12ieeeExponentS1485;
    #line 688 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L1vS1487 = _M0FPB3d2d(_M0L12ieeeMantissaS1484, _M0L6_2atmpS3975);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1488 = _M0L7_2abindS1486;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1489 = _M0L7_2aSomeS1488;
    struct _M0TPB17FloatingDecimal64* _M0L1xS1490 = _M0L4_2afS1489;
    while (1) {
      uint64_t _M0L8mantissaS3981 = _M0L1xS1490->$0;
      uint64_t _M0L1qS1491 = _M0L8mantissaS3981 / 10ull;
      uint64_t _M0L8mantissaS3979 = _M0L1xS1490->$0;
      uint64_t _M0L6_2atmpS3980 = 10ull * _M0L1qS1491;
      uint64_t _M0L1rS1492 = _M0L8mantissaS3979 - _M0L6_2atmpS3980;
      int32_t _M0L8exponentS3978;
      int32_t _M0L6_2atmpS3977;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3976;
      if (_M0L1rS1492 != 0ull) {
        _M0L1vS1487 = _M0L1xS1490;
        break;
      }
      _M0L8exponentS3978 = _M0L1xS1490->$1;
      moonbit_decref(_M0L1xS1490);
      _M0L6_2atmpS3977 = _M0L8exponentS3978 + 1;
      _M0L6_2atmpS3976
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0L6_2atmpS3976)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L6_2atmpS3976->$0 = _M0L1qS1491;
      _M0L6_2atmpS3976->$1 = _M0L6_2atmpS3977;
      _M0L1xS1490 = _M0L6_2atmpS3976;
      continue;
      break;
    }
  }
  #line 690 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_5528 = _M0FPB9to__chars(_M0L1vS1487, _M0L8ieeeSignS1483);
  moonbit_decref(_M0L1vS1487);
  return _result_5528;
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1476,
  int32_t _M0L12ieeeExponentS1478
) {
  uint64_t _M0L2m2S1475;
  int32_t _M0L6_2atmpS3972;
  int32_t _M0L2e2S1477;
  int32_t _M0L6_2atmpS3971;
  uint64_t _M0L6_2atmpS3970;
  uint64_t _M0L4maskS1479;
  uint64_t _M0L8fractionS1480;
  int32_t _M0L6_2atmpS3969;
  uint64_t _M0L6_2atmpS3968;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3967;
  #line 637 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2m2S1475 = 4503599627370496ull | _M0L12ieeeMantissaS1476;
  _M0L6_2atmpS3972 = _M0L12ieeeExponentS1478 - 1023;
  _M0L2e2S1477 = _M0L6_2atmpS3972 - 52;
  if (_M0L2e2S1477 > 0) {
    return 0;
  }
  if (_M0L2e2S1477 < -52) {
    return 0;
  }
  _M0L6_2atmpS3971 = -_M0L2e2S1477;
  _M0L6_2atmpS3970 = 1ull << (_M0L6_2atmpS3971 & 63);
  _M0L4maskS1479 = _M0L6_2atmpS3970 - 1ull;
  _M0L8fractionS1480 = _M0L2m2S1475 & _M0L4maskS1479;
  if (_M0L8fractionS1480 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3969 = -_M0L2e2S1477;
  _M0L6_2atmpS3968 = _M0L2m2S1475 >> (_M0L6_2atmpS3969 & 63);
  _M0L6_2atmpS3967
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS3967)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6_2atmpS3967->$0 = _M0L6_2atmpS3968;
  _M0L6_2atmpS3967->$1 = 0;
  return _M0L6_2atmpS3967;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1443,
  int32_t _M0L4signS1441
) {
  int32_t _M0L6_2atmpS3966;
  moonbit_bytes_t _M0L6resultS1439;
  int32_t _M0Lm5indexS1440;
  uint64_t _M0L6outputS1442;
  int32_t _M0L7olengthS1444;
  int32_t _M0L8exponentS3965;
  int32_t _M0L6_2atmpS3964;
  int32_t _M0Lm3expS1445;
  int32_t _M0L6_2atmpS3963;
  int32_t _M0L6_2atmpS3961;
  int32_t _M0L18scientificNotationS1446;
  #line 530 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 532 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3966 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1439
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS3966);
  _M0Lm5indexS1440 = 0;
  if (_M0L4signS1441) {
    int32_t _M0L6_2atmpS3835 = _M0Lm5indexS1440;
    int32_t _M0L6_2atmpS3836;
    if (
      _M0L6_2atmpS3835 < 0
      || _M0L6_2atmpS3835 >= Moonbit_array_length(_M0L6resultS1439)
    ) {
      #line 535 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1439[_M0L6_2atmpS3835] = 45;
    _M0L6_2atmpS3836 = _M0Lm5indexS1440;
    _M0Lm5indexS1440 = _M0L6_2atmpS3836 + 1;
  }
  _M0L6outputS1442 = _M0L1vS1443->$0;
  #line 539 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7olengthS1444 = _M0FPB17decimal__length17(_M0L6outputS1442);
  _M0L8exponentS3965 = _M0L1vS1443->$1;
  _M0L6_2atmpS3964 = _M0L8exponentS3965 + _M0L7olengthS1444;
  _M0Lm3expS1445 = _M0L6_2atmpS3964 - 1;
  _M0L6_2atmpS3963 = _M0Lm3expS1445;
  if (_M0L6_2atmpS3963 >= -6) {
    int32_t _M0L6_2atmpS3962 = _M0Lm3expS1445;
    _M0L6_2atmpS3961 = _M0L6_2atmpS3962 < 21;
  } else {
    _M0L6_2atmpS3961 = 0;
  }
  _M0L18scientificNotationS1446 = !_M0L6_2atmpS3961;
  if (_M0L18scientificNotationS1446) {
    int32_t _M0L7_2abindS1447 = _M0L7olengthS1444 - 1;
    uint64_t _M0L6outputS1448;
    int32_t _M0L1iS1449 = 0;
    uint64_t _M0L6outputS1450 = _M0L6outputS1442;
    int32_t _M0L6_2atmpS3837;
    int32_t _M0L6_2atmpS3841;
    int32_t _M0L6_2atmpS3840;
    int32_t _M0L6_2atmpS3839;
    int32_t _M0L6_2atmpS3838;
    int32_t _M0L6_2atmpS3845;
    int32_t _M0L6_2atmpS3846;
    int32_t _M0L6_2atmpS3847;
    int32_t _M0L6_2atmpS3848;
    int32_t _M0L6_2atmpS3849;
    int32_t _M0L6_2atmpS3855;
    int32_t _M0L6_2atmpS3888;
    moonbit_string_t _result_5530;
    while (1) {
      if (_M0L1iS1449 < _M0L7_2abindS1447) {
        uint64_t _M0L1cS1451 = _M0L6outputS1450 % 10ull;
        int32_t _M0L6_2atmpS3894 = _M0Lm5indexS1440;
        int32_t _M0L6_2atmpS3893 = _M0L6_2atmpS3894 + _M0L7olengthS1444;
        int32_t _M0L6_2atmpS3889 = _M0L6_2atmpS3893 - _M0L1iS1449;
        int32_t _M0L6_2atmpS3892 = (int32_t)_M0L1cS1451;
        int32_t _M0L6_2atmpS3891 = 48 + _M0L6_2atmpS3892;
        int32_t _M0L6_2atmpS3890 = _M0L6_2atmpS3891 & 0xff;
        int32_t _M0L6_2atmpS3895;
        uint64_t _M0L6_2atmpS3896;
        if (
          _M0L6_2atmpS3889 < 0
          || _M0L6_2atmpS3889 >= Moonbit_array_length(_M0L6resultS1439)
        ) {
          #line 547 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1439[_M0L6_2atmpS3889] = _M0L6_2atmpS3890;
        _M0L6_2atmpS3895 = _M0L1iS1449 + 1;
        _M0L6_2atmpS3896 = _M0L6outputS1450 / 10ull;
        _M0L1iS1449 = _M0L6_2atmpS3895;
        _M0L6outputS1450 = _M0L6_2atmpS3896;
        continue;
      } else {
        _M0L6outputS1448 = _M0L6outputS1450;
      }
      break;
    }
    _M0L6_2atmpS3837 = _M0Lm5indexS1440;
    _M0L6_2atmpS3841 = (int32_t)_M0L6outputS1448;
    _M0L6_2atmpS3840 = _M0L6_2atmpS3841 % 10;
    _M0L6_2atmpS3839 = 48 + _M0L6_2atmpS3840;
    _M0L6_2atmpS3838 = _M0L6_2atmpS3839 & 0xff;
    if (
      _M0L6_2atmpS3837 < 0
      || _M0L6_2atmpS3837 >= Moonbit_array_length(_M0L6resultS1439)
    ) {
      #line 552 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1439[_M0L6_2atmpS3837] = _M0L6_2atmpS3838;
    if (_M0L7olengthS1444 > 1) {
      int32_t _M0L6_2atmpS3843 = _M0Lm5indexS1440;
      int32_t _M0L6_2atmpS3842 = _M0L6_2atmpS3843 + 1;
      if (
        _M0L6_2atmpS3842 < 0
        || _M0L6_2atmpS3842 >= Moonbit_array_length(_M0L6resultS1439)
      ) {
        #line 554 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1439[_M0L6_2atmpS3842] = 46;
    } else {
      int32_t _M0L6_2atmpS3844 = _M0Lm5indexS1440;
      _M0Lm5indexS1440 = _M0L6_2atmpS3844 - 1;
    }
    _M0L6_2atmpS3845 = _M0Lm5indexS1440;
    _M0L6_2atmpS3846 = _M0L7olengthS1444 + 1;
    _M0Lm5indexS1440 = _M0L6_2atmpS3845 + _M0L6_2atmpS3846;
    _M0L6_2atmpS3847 = _M0Lm5indexS1440;
    if (
      _M0L6_2atmpS3847 < 0
      || _M0L6_2atmpS3847 >= Moonbit_array_length(_M0L6resultS1439)
    ) {
      #line 562 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1439[_M0L6_2atmpS3847] = 101;
    _M0L6_2atmpS3848 = _M0Lm5indexS1440;
    _M0Lm5indexS1440 = _M0L6_2atmpS3848 + 1;
    _M0L6_2atmpS3849 = _M0Lm3expS1445;
    if (_M0L6_2atmpS3849 < 0) {
      int32_t _M0L6_2atmpS3850 = _M0Lm5indexS1440;
      int32_t _M0L6_2atmpS3851;
      int32_t _M0L6_2atmpS3852;
      if (
        _M0L6_2atmpS3850 < 0
        || _M0L6_2atmpS3850 >= Moonbit_array_length(_M0L6resultS1439)
      ) {
        #line 565 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1439[_M0L6_2atmpS3850] = 45;
      _M0L6_2atmpS3851 = _M0Lm5indexS1440;
      _M0Lm5indexS1440 = _M0L6_2atmpS3851 + 1;
      _M0L6_2atmpS3852 = _M0Lm3expS1445;
      _M0Lm3expS1445 = -_M0L6_2atmpS3852;
    } else {
      int32_t _M0L6_2atmpS3853 = _M0Lm5indexS1440;
      int32_t _M0L6_2atmpS3854;
      if (
        _M0L6_2atmpS3853 < 0
        || _M0L6_2atmpS3853 >= Moonbit_array_length(_M0L6resultS1439)
      ) {
        #line 569 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1439[_M0L6_2atmpS3853] = 43;
      _M0L6_2atmpS3854 = _M0Lm5indexS1440;
      _M0Lm5indexS1440 = _M0L6_2atmpS3854 + 1;
    }
    _M0L6_2atmpS3855 = _M0Lm3expS1445;
    if (_M0L6_2atmpS3855 >= 100) {
      int32_t _M0L6_2atmpS3871 = _M0Lm3expS1445;
      int32_t _M0L1aS1453 = _M0L6_2atmpS3871 / 100;
      int32_t _M0L6_2atmpS3870 = _M0Lm3expS1445;
      int32_t _M0L6_2atmpS3869 = _M0L6_2atmpS3870 / 10;
      int32_t _M0L1bS1454 = _M0L6_2atmpS3869 % 10;
      int32_t _M0L6_2atmpS3868 = _M0Lm3expS1445;
      int32_t _M0L1cS1455 = _M0L6_2atmpS3868 % 10;
      int32_t _M0L6_2atmpS3856 = _M0Lm5indexS1440;
      int32_t _M0L6_2atmpS3858 = 48 + _M0L1aS1453;
      int32_t _M0L6_2atmpS3857 = _M0L6_2atmpS3858 & 0xff;
      int32_t _M0L6_2atmpS3862;
      int32_t _M0L6_2atmpS3859;
      int32_t _M0L6_2atmpS3861;
      int32_t _M0L6_2atmpS3860;
      int32_t _M0L6_2atmpS3866;
      int32_t _M0L6_2atmpS3863;
      int32_t _M0L6_2atmpS3865;
      int32_t _M0L6_2atmpS3864;
      int32_t _M0L6_2atmpS3867;
      if (
        _M0L6_2atmpS3856 < 0
        || _M0L6_2atmpS3856 >= Moonbit_array_length(_M0L6resultS1439)
      ) {
        #line 576 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1439[_M0L6_2atmpS3856] = _M0L6_2atmpS3857;
      _M0L6_2atmpS3862 = _M0Lm5indexS1440;
      _M0L6_2atmpS3859 = _M0L6_2atmpS3862 + 1;
      _M0L6_2atmpS3861 = 48 + _M0L1bS1454;
      _M0L6_2atmpS3860 = _M0L6_2atmpS3861 & 0xff;
      if (
        _M0L6_2atmpS3859 < 0
        || _M0L6_2atmpS3859 >= Moonbit_array_length(_M0L6resultS1439)
      ) {
        #line 577 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1439[_M0L6_2atmpS3859] = _M0L6_2atmpS3860;
      _M0L6_2atmpS3866 = _M0Lm5indexS1440;
      _M0L6_2atmpS3863 = _M0L6_2atmpS3866 + 2;
      _M0L6_2atmpS3865 = 48 + _M0L1cS1455;
      _M0L6_2atmpS3864 = _M0L6_2atmpS3865 & 0xff;
      if (
        _M0L6_2atmpS3863 < 0
        || _M0L6_2atmpS3863 >= Moonbit_array_length(_M0L6resultS1439)
      ) {
        #line 578 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1439[_M0L6_2atmpS3863] = _M0L6_2atmpS3864;
      _M0L6_2atmpS3867 = _M0Lm5indexS1440;
      _M0Lm5indexS1440 = _M0L6_2atmpS3867 + 3;
    } else {
      int32_t _M0L6_2atmpS3872 = _M0Lm3expS1445;
      if (_M0L6_2atmpS3872 >= 10) {
        int32_t _M0L6_2atmpS3882 = _M0Lm3expS1445;
        int32_t _M0L1aS1456 = _M0L6_2atmpS3882 / 10;
        int32_t _M0L6_2atmpS3881 = _M0Lm3expS1445;
        int32_t _M0L1bS1457 = _M0L6_2atmpS3881 % 10;
        int32_t _M0L6_2atmpS3873 = _M0Lm5indexS1440;
        int32_t _M0L6_2atmpS3875 = 48 + _M0L1aS1456;
        int32_t _M0L6_2atmpS3874 = _M0L6_2atmpS3875 & 0xff;
        int32_t _M0L6_2atmpS3879;
        int32_t _M0L6_2atmpS3876;
        int32_t _M0L6_2atmpS3878;
        int32_t _M0L6_2atmpS3877;
        int32_t _M0L6_2atmpS3880;
        if (
          _M0L6_2atmpS3873 < 0
          || _M0L6_2atmpS3873 >= Moonbit_array_length(_M0L6resultS1439)
        ) {
          #line 583 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1439[_M0L6_2atmpS3873] = _M0L6_2atmpS3874;
        _M0L6_2atmpS3879 = _M0Lm5indexS1440;
        _M0L6_2atmpS3876 = _M0L6_2atmpS3879 + 1;
        _M0L6_2atmpS3878 = 48 + _M0L1bS1457;
        _M0L6_2atmpS3877 = _M0L6_2atmpS3878 & 0xff;
        if (
          _M0L6_2atmpS3876 < 0
          || _M0L6_2atmpS3876 >= Moonbit_array_length(_M0L6resultS1439)
        ) {
          #line 584 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1439[_M0L6_2atmpS3876] = _M0L6_2atmpS3877;
        _M0L6_2atmpS3880 = _M0Lm5indexS1440;
        _M0Lm5indexS1440 = _M0L6_2atmpS3880 + 2;
      } else {
        int32_t _M0L6_2atmpS3883 = _M0Lm5indexS1440;
        int32_t _M0L6_2atmpS3886 = _M0Lm3expS1445;
        int32_t _M0L6_2atmpS3885 = 48 + _M0L6_2atmpS3886;
        int32_t _M0L6_2atmpS3884 = _M0L6_2atmpS3885 & 0xff;
        int32_t _M0L6_2atmpS3887;
        if (
          _M0L6_2atmpS3883 < 0
          || _M0L6_2atmpS3883 >= Moonbit_array_length(_M0L6resultS1439)
        ) {
          #line 587 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1439[_M0L6_2atmpS3883] = _M0L6_2atmpS3884;
        _M0L6_2atmpS3887 = _M0Lm5indexS1440;
        _M0Lm5indexS1440 = _M0L6_2atmpS3887 + 1;
      }
    }
    _M0L6_2atmpS3888 = _M0Lm5indexS1440;
    #line 590 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_5530
    = _M0FPB19string__from__bytes(_M0L6resultS1439, 0, _M0L6_2atmpS3888);
    moonbit_decref(_M0L6resultS1439);
    return _result_5530;
  } else {
    int32_t _M0L6_2atmpS3897 = _M0Lm3expS1445;
    int32_t _M0L6_2atmpS3960;
    moonbit_string_t _result_5536;
    if (_M0L6_2atmpS3897 < 0) {
      int32_t _M0L6_2atmpS3898 = _M0Lm5indexS1440;
      int32_t _M0L6_2atmpS3900;
      int32_t _M0L6_2atmpS3899;
      int32_t _M0L6_2atmpS3901;
      int32_t _M0L1iS1458;
      int32_t _M0L6_2atmpS3916;
      int32_t _M0L6_2atmpS3918;
      int32_t _M0L6_2atmpS3917;
      int32_t _M0L7currentS1460;
      int32_t _M0L1iS1461;
      uint64_t _M0L6outputS1462;
      if (
        _M0L6_2atmpS3898 < 0
        || _M0L6_2atmpS3898 >= Moonbit_array_length(_M0L6resultS1439)
      ) {
        #line 595 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1439[_M0L6_2atmpS3898] = 48;
      _M0L6_2atmpS3900 = _M0Lm5indexS1440;
      _M0L6_2atmpS3899 = _M0L6_2atmpS3900 + 1;
      if (
        _M0L6_2atmpS3899 < 0
        || _M0L6_2atmpS3899 >= Moonbit_array_length(_M0L6resultS1439)
      ) {
        #line 596 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1439[_M0L6_2atmpS3899] = 46;
      _M0L6_2atmpS3901 = _M0Lm5indexS1440;
      _M0Lm5indexS1440 = _M0L6_2atmpS3901 + 2;
      _M0L1iS1458 = -1;
      while (1) {
        int32_t _M0L6_2atmpS3902 = _M0Lm3expS1445;
        if (_M0L1iS1458 > _M0L6_2atmpS3902) {
          int32_t _M0L6_2atmpS3905 = _M0Lm5indexS1440;
          int32_t _M0L6_2atmpS3904 = _M0L6_2atmpS3905 - _M0L1iS1458;
          int32_t _M0L6_2atmpS3903 = _M0L6_2atmpS3904 - 1;
          int32_t _M0L6_2atmpS3906;
          if (
            _M0L6_2atmpS3903 < 0
            || _M0L6_2atmpS3903 >= Moonbit_array_length(_M0L6resultS1439)
          ) {
            #line 599 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1439[_M0L6_2atmpS3903] = 48;
          _M0L6_2atmpS3906 = _M0L1iS1458 - 1;
          _M0L1iS1458 = _M0L6_2atmpS3906;
          continue;
        }
        break;
      }
      _M0L6_2atmpS3916 = _M0Lm5indexS1440;
      _M0L6_2atmpS3918 = _M0Lm3expS1445;
      _M0L6_2atmpS3917 = -1 - _M0L6_2atmpS3918;
      _M0L7currentS1460 = _M0L6_2atmpS3916 + _M0L6_2atmpS3917;
      _M0L1iS1461 = 0;
      _M0L6outputS1462 = _M0L6outputS1442;
      while (1) {
        if (_M0L1iS1461 < _M0L7olengthS1444) {
          int32_t _M0L6_2atmpS3913 = _M0L7currentS1460 + _M0L7olengthS1444;
          int32_t _M0L6_2atmpS3912 = _M0L6_2atmpS3913 - _M0L1iS1461;
          int32_t _M0L6_2atmpS3907 = _M0L6_2atmpS3912 - 1;
          uint64_t _M0L6_2atmpS3911 = _M0L6outputS1462 % 10ull;
          int32_t _M0L6_2atmpS3910 = (int32_t)_M0L6_2atmpS3911;
          int32_t _M0L6_2atmpS3909 = 48 + _M0L6_2atmpS3910;
          int32_t _M0L6_2atmpS3908 = _M0L6_2atmpS3909 & 0xff;
          int32_t _M0L6_2atmpS3914;
          uint64_t _M0L6_2atmpS3915;
          if (
            _M0L6_2atmpS3907 < 0
            || _M0L6_2atmpS3907 >= Moonbit_array_length(_M0L6resultS1439)
          ) {
            #line 603 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1439[_M0L6_2atmpS3907] = _M0L6_2atmpS3908;
          _M0L6_2atmpS3914 = _M0L1iS1461 + 1;
          _M0L6_2atmpS3915 = _M0L6outputS1462 / 10ull;
          _M0L1iS1461 = _M0L6_2atmpS3914;
          _M0L6outputS1462 = _M0L6_2atmpS3915;
          continue;
        }
        break;
      }
      _M0Lm5indexS1440 = _M0L7currentS1460 + _M0L7olengthS1444;
    } else {
      int32_t _M0L6_2atmpS3920 = _M0Lm3expS1445;
      int32_t _M0L6_2atmpS3919 = _M0L6_2atmpS3920 + 1;
      if (_M0L6_2atmpS3919 >= _M0L7olengthS1444) {
        int32_t _M0L1iS1464 = 0;
        uint64_t _M0L6outputS1465 = _M0L6outputS1442;
        int32_t _M0L6_2atmpS3931;
        int32_t _M0L6_2atmpS3936;
        int32_t _M0L7_2abindS1467;
        int32_t _M0L1iS1468;
        int32_t _M0L6_2atmpS3937;
        int32_t _M0L6_2atmpS3940;
        int32_t _M0L6_2atmpS3939;
        int32_t _M0L6_2atmpS3938;
        while (1) {
          if (_M0L1iS1464 < _M0L7olengthS1444) {
            int32_t _M0L6_2atmpS3928 = _M0Lm5indexS1440;
            int32_t _M0L6_2atmpS3927 = _M0L6_2atmpS3928 + _M0L7olengthS1444;
            int32_t _M0L6_2atmpS3926 = _M0L6_2atmpS3927 - _M0L1iS1464;
            int32_t _M0L6_2atmpS3921 = _M0L6_2atmpS3926 - 1;
            uint64_t _M0L6_2atmpS3925 = _M0L6outputS1465 % 10ull;
            int32_t _M0L6_2atmpS3924 = (int32_t)_M0L6_2atmpS3925;
            int32_t _M0L6_2atmpS3923 = 48 + _M0L6_2atmpS3924;
            int32_t _M0L6_2atmpS3922 = _M0L6_2atmpS3923 & 0xff;
            int32_t _M0L6_2atmpS3929;
            uint64_t _M0L6_2atmpS3930;
            if (
              _M0L6_2atmpS3921 < 0
              || _M0L6_2atmpS3921 >= Moonbit_array_length(_M0L6resultS1439)
            ) {
              #line 610 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1439[_M0L6_2atmpS3921] = _M0L6_2atmpS3922;
            _M0L6_2atmpS3929 = _M0L1iS1464 + 1;
            _M0L6_2atmpS3930 = _M0L6outputS1465 / 10ull;
            _M0L1iS1464 = _M0L6_2atmpS3929;
            _M0L6outputS1465 = _M0L6_2atmpS3930;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3931 = _M0Lm5indexS1440;
        _M0Lm5indexS1440 = _M0L6_2atmpS3931 + _M0L7olengthS1444;
        _M0L6_2atmpS3936 = _M0Lm3expS1445;
        _M0L7_2abindS1467 = _M0L6_2atmpS3936 + 1;
        _M0L1iS1468 = _M0L7olengthS1444;
        while (1) {
          if (_M0L1iS1468 < _M0L7_2abindS1467) {
            int32_t _M0L6_2atmpS3934 = _M0Lm5indexS1440;
            int32_t _M0L6_2atmpS3933 = _M0L6_2atmpS3934 + _M0L1iS1468;
            int32_t _M0L6_2atmpS3932 = _M0L6_2atmpS3933 - _M0L7olengthS1444;
            int32_t _M0L6_2atmpS3935;
            if (
              _M0L6_2atmpS3932 < 0
              || _M0L6_2atmpS3932 >= Moonbit_array_length(_M0L6resultS1439)
            ) {
              #line 615 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1439[_M0L6_2atmpS3932] = 48;
            _M0L6_2atmpS3935 = _M0L1iS1468 + 1;
            _M0L1iS1468 = _M0L6_2atmpS3935;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3937 = _M0Lm5indexS1440;
        _M0L6_2atmpS3940 = _M0Lm3expS1445;
        _M0L6_2atmpS3939 = _M0L6_2atmpS3940 + 1;
        _M0L6_2atmpS3938 = _M0L6_2atmpS3939 - _M0L7olengthS1444;
        _M0Lm5indexS1440 = _M0L6_2atmpS3937 + _M0L6_2atmpS3938;
      } else {
        int32_t _M0L6_2atmpS3957 = _M0Lm5indexS1440;
        int32_t _M0L6_2atmpS3956 = _M0L6_2atmpS3957 + 1;
        int32_t _M0L1iS1470 = 0;
        int32_t _M0L7currentS1471 = _M0L6_2atmpS3956;
        uint64_t _M0L6outputS1472 = _M0L6outputS1442;
        int32_t _M0L6_2atmpS3958;
        int32_t _M0L6_2atmpS3959;
        while (1) {
          if (_M0L1iS1470 < _M0L7olengthS1444) {
            int32_t _M0L6_2atmpS3952 = _M0L7olengthS1444 - _M0L1iS1470;
            int32_t _M0L6_2atmpS3950 = _M0L6_2atmpS3952 - 1;
            int32_t _M0L6_2atmpS3951 = _M0Lm3expS1445;
            int32_t _M0L7currentS1473;
            int32_t _M0L6_2atmpS3947;
            int32_t _M0L6_2atmpS3946;
            int32_t _M0L6_2atmpS3941;
            uint64_t _M0L6_2atmpS3945;
            int32_t _M0L6_2atmpS3944;
            int32_t _M0L6_2atmpS3943;
            int32_t _M0L6_2atmpS3942;
            int32_t _M0L6_2atmpS3948;
            uint64_t _M0L6_2atmpS3949;
            if (_M0L6_2atmpS3950 == _M0L6_2atmpS3951) {
              int32_t _M0L6_2atmpS3955 =
                _M0L7currentS1471 + _M0L7olengthS1444;
              int32_t _M0L6_2atmpS3954 = _M0L6_2atmpS3955 - _M0L1iS1470;
              int32_t _M0L6_2atmpS3953 = _M0L6_2atmpS3954 - 1;
              if (
                _M0L6_2atmpS3953 < 0
                || _M0L6_2atmpS3953 >= Moonbit_array_length(_M0L6resultS1439)
              ) {
                #line 622 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1439[_M0L6_2atmpS3953] = 46;
              _M0L7currentS1473 = _M0L7currentS1471 - 1;
            } else {
              _M0L7currentS1473 = _M0L7currentS1471;
            }
            _M0L6_2atmpS3947 = _M0L7currentS1473 + _M0L7olengthS1444;
            _M0L6_2atmpS3946 = _M0L6_2atmpS3947 - _M0L1iS1470;
            _M0L6_2atmpS3941 = _M0L6_2atmpS3946 - 1;
            _M0L6_2atmpS3945 = _M0L6outputS1472 % 10ull;
            _M0L6_2atmpS3944 = (int32_t)_M0L6_2atmpS3945;
            _M0L6_2atmpS3943 = 48 + _M0L6_2atmpS3944;
            _M0L6_2atmpS3942 = _M0L6_2atmpS3943 & 0xff;
            if (
              _M0L6_2atmpS3941 < 0
              || _M0L6_2atmpS3941 >= Moonbit_array_length(_M0L6resultS1439)
            ) {
              #line 627 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1439[_M0L6_2atmpS3941] = _M0L6_2atmpS3942;
            _M0L6_2atmpS3948 = _M0L1iS1470 + 1;
            _M0L6_2atmpS3949 = _M0L6outputS1472 / 10ull;
            _M0L1iS1470 = _M0L6_2atmpS3948;
            _M0L7currentS1471 = _M0L7currentS1473;
            _M0L6outputS1472 = _M0L6_2atmpS3949;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3958 = _M0Lm5indexS1440;
        _M0L6_2atmpS3959 = _M0L7olengthS1444 + 1;
        _M0Lm5indexS1440 = _M0L6_2atmpS3958 + _M0L6_2atmpS3959;
      }
    }
    _M0L6_2atmpS3960 = _M0Lm5indexS1440;
    #line 632 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_5536
    = _M0FPB19string__from__bytes(_M0L6resultS1439, 0, _M0L6_2atmpS3960);
    moonbit_decref(_M0L6resultS1439);
    return _result_5536;
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1385,
  uint32_t _M0L12ieeeExponentS1384
) {
  int32_t _M0Lm2e2S1382;
  uint64_t _M0Lm2m2S1383;
  uint64_t _M0L6_2atmpS3834;
  uint64_t _M0L6_2atmpS3833;
  int32_t _M0L4evenS1386;
  uint64_t _M0L6_2atmpS3832;
  uint64_t _M0L2mvS1387;
  int32_t _M0L7mmShiftS1388;
  uint64_t _M0Lm2vrS1389;
  uint64_t _M0Lm2vpS1390;
  uint64_t _M0Lm2vmS1391;
  int32_t _M0Lm3e10S1392;
  int32_t _M0Lm17vmIsTrailingZerosS1393;
  int32_t _M0Lm17vrIsTrailingZerosS1394;
  int32_t _M0L6_2atmpS3734;
  int32_t _M0Lm7removedS1413;
  int32_t _M0Lm16lastRemovedDigitS1414;
  uint64_t _M0Lm6outputS1415;
  int32_t _M0L6_2atmpS3830;
  int32_t _M0L6_2atmpS3831;
  int32_t _M0L3expS1438;
  uint64_t _M0L6_2atmpS3829;
  struct _M0TPB17FloatingDecimal64* _block_5542;
  #line 347 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0Lm2e2S1382 = 0;
  _M0Lm2m2S1383 = 0ull;
  if (_M0L12ieeeExponentS1384 == 0u) {
    _M0Lm2e2S1382 = -1076;
    _M0Lm2m2S1383 = _M0L12ieeeMantissaS1385;
  } else {
    int32_t _M0L6_2atmpS3733 = *(int32_t*)&_M0L12ieeeExponentS1384;
    int32_t _M0L6_2atmpS3732 = _M0L6_2atmpS3733 - 1023;
    int32_t _M0L6_2atmpS3731 = _M0L6_2atmpS3732 - 52;
    _M0Lm2e2S1382 = _M0L6_2atmpS3731 - 2;
    _M0Lm2m2S1383 = 4503599627370496ull | _M0L12ieeeMantissaS1385;
  }
  _M0L6_2atmpS3834 = _M0Lm2m2S1383;
  _M0L6_2atmpS3833 = _M0L6_2atmpS3834 & 1ull;
  _M0L4evenS1386 = _M0L6_2atmpS3833 == 0ull;
  _M0L6_2atmpS3832 = _M0Lm2m2S1383;
  _M0L2mvS1387 = 4ull * _M0L6_2atmpS3832;
  if (_M0L12ieeeMantissaS1385 != 0ull) {
    _M0L7mmShiftS1388 = 1;
  } else {
    _M0L7mmShiftS1388 = _M0L12ieeeExponentS1384 <= 1u;
  }
  _M0Lm2vrS1389 = 0ull;
  _M0Lm2vpS1390 = 0ull;
  _M0Lm2vmS1391 = 0ull;
  _M0Lm3e10S1392 = 0;
  _M0Lm17vmIsTrailingZerosS1393 = 0;
  _M0Lm17vrIsTrailingZerosS1394 = 0;
  _M0L6_2atmpS3734 = _M0Lm2e2S1382;
  if (_M0L6_2atmpS3734 >= 0) {
    int32_t _M0L6_2atmpS3756 = _M0Lm2e2S1382;
    int32_t _M0L6_2atmpS3752;
    int32_t _M0L6_2atmpS3755;
    int32_t _M0L6_2atmpS3754;
    int32_t _M0L6_2atmpS3753;
    int32_t _M0L1qS1395;
    int32_t _M0L6_2atmpS3751;
    int32_t _M0L6_2atmpS3750;
    int32_t _M0L1kS1396;
    int32_t _M0L6_2atmpS3749;
    int32_t _M0L6_2atmpS3748;
    int32_t _M0L6_2atmpS3747;
    int32_t _M0L1iS1397;
    struct _M0TPB8Pow5Pair _M0L4pow5S1398;
    uint64_t _M0L6_2atmpS3746;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1399;
    uint64_t _M0L8_2avrOutS1400;
    uint64_t _M0L8_2avpOutS1401;
    uint64_t _M0L8_2avmOutS1402;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3752 = _M0FPB9log10Pow2(_M0L6_2atmpS3756);
    _M0L6_2atmpS3755 = _M0Lm2e2S1382;
    _M0L6_2atmpS3754 = _M0L6_2atmpS3755 > 3;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3753 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS3754);
    _M0L1qS1395 = _M0L6_2atmpS3752 - _M0L6_2atmpS3753;
    _M0Lm3e10S1392 = _M0L1qS1395;
    #line 385 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3751 = _M0FPB8pow5bits(_M0L1qS1395);
    _M0L6_2atmpS3750 = 125 + _M0L6_2atmpS3751;
    _M0L1kS1396 = _M0L6_2atmpS3750 - 1;
    _M0L6_2atmpS3749 = _M0Lm2e2S1382;
    _M0L6_2atmpS3748 = -_M0L6_2atmpS3749;
    _M0L6_2atmpS3747 = _M0L6_2atmpS3748 + _M0L1qS1395;
    _M0L1iS1397 = _M0L6_2atmpS3747 + _M0L1kS1396;
    #line 387 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1398 = _M0FPB22double__computeInvPow5(_M0L1qS1395);
    _M0L6_2atmpS3746 = _M0Lm2m2S1383;
    #line 388 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1399
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS3746, _M0L4pow5S1398, _M0L1iS1397, _M0L7mmShiftS1388);
    _M0L8_2avrOutS1400 = _M0L7_2abindS1399.$0;
    _M0L8_2avpOutS1401 = _M0L7_2abindS1399.$1;
    _M0L8_2avmOutS1402 = _M0L7_2abindS1399.$2;
    _M0Lm2vrS1389 = _M0L8_2avrOutS1400;
    _M0Lm2vpS1390 = _M0L8_2avpOutS1401;
    _M0Lm2vmS1391 = _M0L8_2avmOutS1402;
    if (_M0L1qS1395 <= 21) {
      int32_t _M0L6_2atmpS3742 = (int32_t)_M0L2mvS1387;
      uint64_t _M0L6_2atmpS3745 = _M0L2mvS1387 / 5ull;
      int32_t _M0L6_2atmpS3744 = (int32_t)_M0L6_2atmpS3745;
      int32_t _M0L6_2atmpS3743 = 5 * _M0L6_2atmpS3744;
      int32_t _M0L6mvMod5S1403 = _M0L6_2atmpS3742 - _M0L6_2atmpS3743;
      if (_M0L6mvMod5S1403 == 0) {
        #line 400 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1394
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1387, _M0L1qS1395);
      } else if (_M0L4evenS1386) {
        uint64_t _M0L6_2atmpS3736 = _M0L2mvS1387 - 1ull;
        uint64_t _M0L6_2atmpS3737;
        uint64_t _M0L6_2atmpS3735;
        #line 406 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3737 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1388);
        _M0L6_2atmpS3735 = _M0L6_2atmpS3736 - _M0L6_2atmpS3737;
        #line 405 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1393
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS3735, _M0L1qS1395);
      } else {
        uint64_t _M0L6_2atmpS3738 = _M0Lm2vpS1390;
        uint64_t _M0L6_2atmpS3741 = _M0L2mvS1387 + 2ull;
        int32_t _M0L6_2atmpS3740;
        uint64_t _M0L6_2atmpS3739;
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3740
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS3741, _M0L1qS1395);
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3739 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS3740);
        _M0Lm2vpS1390 = _M0L6_2atmpS3738 - _M0L6_2atmpS3739;
      }
    }
  } else {
    int32_t _M0L6_2atmpS3770 = _M0Lm2e2S1382;
    int32_t _M0L6_2atmpS3769 = -_M0L6_2atmpS3770;
    int32_t _M0L6_2atmpS3764;
    int32_t _M0L6_2atmpS3768;
    int32_t _M0L6_2atmpS3767;
    int32_t _M0L6_2atmpS3766;
    int32_t _M0L6_2atmpS3765;
    int32_t _M0L1qS1404;
    int32_t _M0L6_2atmpS3757;
    int32_t _M0L6_2atmpS3763;
    int32_t _M0L6_2atmpS3762;
    int32_t _M0L1iS1405;
    int32_t _M0L6_2atmpS3761;
    int32_t _M0L1kS1406;
    int32_t _M0L1jS1407;
    struct _M0TPB8Pow5Pair _M0L4pow5S1408;
    uint64_t _M0L6_2atmpS3760;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1409;
    uint64_t _M0L8_2avrOutS1410;
    uint64_t _M0L8_2avpOutS1411;
    uint64_t _M0L8_2avmOutS1412;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3764 = _M0FPB9log10Pow5(_M0L6_2atmpS3769);
    _M0L6_2atmpS3768 = _M0Lm2e2S1382;
    _M0L6_2atmpS3767 = -_M0L6_2atmpS3768;
    _M0L6_2atmpS3766 = _M0L6_2atmpS3767 > 1;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3765 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS3766);
    _M0L1qS1404 = _M0L6_2atmpS3764 - _M0L6_2atmpS3765;
    _M0L6_2atmpS3757 = _M0Lm2e2S1382;
    _M0Lm3e10S1392 = _M0L1qS1404 + _M0L6_2atmpS3757;
    _M0L6_2atmpS3763 = _M0Lm2e2S1382;
    _M0L6_2atmpS3762 = -_M0L6_2atmpS3763;
    _M0L1iS1405 = _M0L6_2atmpS3762 - _M0L1qS1404;
    #line 418 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3761 = _M0FPB8pow5bits(_M0L1iS1405);
    _M0L1kS1406 = _M0L6_2atmpS3761 - 125;
    _M0L1jS1407 = _M0L1qS1404 - _M0L1kS1406;
    #line 420 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1408 = _M0FPB19double__computePow5(_M0L1iS1405);
    _M0L6_2atmpS3760 = _M0Lm2m2S1383;
    #line 421 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1409
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS3760, _M0L4pow5S1408, _M0L1jS1407, _M0L7mmShiftS1388);
    _M0L8_2avrOutS1410 = _M0L7_2abindS1409.$0;
    _M0L8_2avpOutS1411 = _M0L7_2abindS1409.$1;
    _M0L8_2avmOutS1412 = _M0L7_2abindS1409.$2;
    _M0Lm2vrS1389 = _M0L8_2avrOutS1410;
    _M0Lm2vpS1390 = _M0L8_2avpOutS1411;
    _M0Lm2vmS1391 = _M0L8_2avmOutS1412;
    if (_M0L1qS1404 <= 1) {
      _M0Lm17vrIsTrailingZerosS1394 = 1;
      if (_M0L4evenS1386) {
        int32_t _M0L6_2atmpS3758;
        #line 432 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3758 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1388);
        _M0Lm17vmIsTrailingZerosS1393 = _M0L6_2atmpS3758 == 1;
      } else {
        uint64_t _M0L6_2atmpS3759 = _M0Lm2vpS1390;
        _M0Lm2vpS1390 = _M0L6_2atmpS3759 - 1ull;
      }
    } else if (_M0L1qS1404 < 63) {
      #line 437 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS1394
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS1387, _M0L1qS1404);
    }
  }
  _M0Lm7removedS1413 = 0;
  _M0Lm16lastRemovedDigitS1414 = 0;
  _M0Lm6outputS1415 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS1393 || _M0Lm17vrIsTrailingZerosS1394) {
    int32_t _if__result_5539;
    uint64_t _M0L6_2atmpS3800;
    uint64_t _M0L6_2atmpS3806;
    uint64_t _M0L6_2atmpS3807;
    int32_t _if__result_5540;
    int32_t _M0L6_2atmpS3803;
    int64_t _M0L6_2atmpS3802;
    uint64_t _M0L6_2atmpS3801;
    while (1) {
      uint64_t _M0L6_2atmpS3783 = _M0Lm2vpS1390;
      uint64_t _M0L7vpDiv10S1416 = _M0L6_2atmpS3783 / 10ull;
      uint64_t _M0L6_2atmpS3782 = _M0Lm2vmS1391;
      uint64_t _M0L7vmDiv10S1417 = _M0L6_2atmpS3782 / 10ull;
      uint64_t _M0L6_2atmpS3781;
      int32_t _M0L6_2atmpS3778;
      int32_t _M0L6_2atmpS3780;
      int32_t _M0L6_2atmpS3779;
      int32_t _M0L7vmMod10S1419;
      uint64_t _M0L6_2atmpS3777;
      uint64_t _M0L7vrDiv10S1420;
      uint64_t _M0L6_2atmpS3776;
      int32_t _M0L6_2atmpS3773;
      int32_t _M0L6_2atmpS3775;
      int32_t _M0L6_2atmpS3774;
      int32_t _M0L7vrMod10S1421;
      int32_t _M0L6_2atmpS3772;
      if (_M0L7vpDiv10S1416 <= _M0L7vmDiv10S1417) {
        break;
      }
      _M0L6_2atmpS3781 = _M0Lm2vmS1391;
      _M0L6_2atmpS3778 = (int32_t)_M0L6_2atmpS3781;
      _M0L6_2atmpS3780 = (int32_t)_M0L7vmDiv10S1417;
      _M0L6_2atmpS3779 = 10 * _M0L6_2atmpS3780;
      _M0L7vmMod10S1419 = _M0L6_2atmpS3778 - _M0L6_2atmpS3779;
      _M0L6_2atmpS3777 = _M0Lm2vrS1389;
      _M0L7vrDiv10S1420 = _M0L6_2atmpS3777 / 10ull;
      _M0L6_2atmpS3776 = _M0Lm2vrS1389;
      _M0L6_2atmpS3773 = (int32_t)_M0L6_2atmpS3776;
      _M0L6_2atmpS3775 = (int32_t)_M0L7vrDiv10S1420;
      _M0L6_2atmpS3774 = 10 * _M0L6_2atmpS3775;
      _M0L7vrMod10S1421 = _M0L6_2atmpS3773 - _M0L6_2atmpS3774;
      if (_M0Lm17vmIsTrailingZerosS1393) {
        _M0Lm17vmIsTrailingZerosS1393 = _M0L7vmMod10S1419 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1393 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1394) {
        int32_t _M0L6_2atmpS3771 = _M0Lm16lastRemovedDigitS1414;
        _M0Lm17vrIsTrailingZerosS1394 = _M0L6_2atmpS3771 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1394 = 0;
      }
      _M0Lm16lastRemovedDigitS1414 = _M0L7vrMod10S1421;
      _M0Lm2vrS1389 = _M0L7vrDiv10S1420;
      _M0Lm2vpS1390 = _M0L7vpDiv10S1416;
      _M0Lm2vmS1391 = _M0L7vmDiv10S1417;
      _M0L6_2atmpS3772 = _M0Lm7removedS1413;
      _M0Lm7removedS1413 = _M0L6_2atmpS3772 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1393) {
      while (1) {
        uint64_t _M0L6_2atmpS3796 = _M0Lm2vmS1391;
        uint64_t _M0L7vmDiv10S1422 = _M0L6_2atmpS3796 / 10ull;
        uint64_t _M0L6_2atmpS3795 = _M0Lm2vmS1391;
        int32_t _M0L6_2atmpS3792 = (int32_t)_M0L6_2atmpS3795;
        int32_t _M0L6_2atmpS3794 = (int32_t)_M0L7vmDiv10S1422;
        int32_t _M0L6_2atmpS3793 = 10 * _M0L6_2atmpS3794;
        int32_t _M0L7vmMod10S1423 = _M0L6_2atmpS3792 - _M0L6_2atmpS3793;
        uint64_t _M0L6_2atmpS3791;
        uint64_t _M0L7vpDiv10S1425;
        uint64_t _M0L6_2atmpS3790;
        uint64_t _M0L7vrDiv10S1426;
        uint64_t _M0L6_2atmpS3789;
        int32_t _M0L6_2atmpS3786;
        int32_t _M0L6_2atmpS3788;
        int32_t _M0L6_2atmpS3787;
        int32_t _M0L7vrMod10S1427;
        int32_t _M0L6_2atmpS3785;
        if (_M0L7vmMod10S1423 != 0) {
          break;
        }
        _M0L6_2atmpS3791 = _M0Lm2vpS1390;
        _M0L7vpDiv10S1425 = _M0L6_2atmpS3791 / 10ull;
        _M0L6_2atmpS3790 = _M0Lm2vrS1389;
        _M0L7vrDiv10S1426 = _M0L6_2atmpS3790 / 10ull;
        _M0L6_2atmpS3789 = _M0Lm2vrS1389;
        _M0L6_2atmpS3786 = (int32_t)_M0L6_2atmpS3789;
        _M0L6_2atmpS3788 = (int32_t)_M0L7vrDiv10S1426;
        _M0L6_2atmpS3787 = 10 * _M0L6_2atmpS3788;
        _M0L7vrMod10S1427 = _M0L6_2atmpS3786 - _M0L6_2atmpS3787;
        if (_M0Lm17vrIsTrailingZerosS1394) {
          int32_t _M0L6_2atmpS3784 = _M0Lm16lastRemovedDigitS1414;
          _M0Lm17vrIsTrailingZerosS1394 = _M0L6_2atmpS3784 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1394 = 0;
        }
        _M0Lm16lastRemovedDigitS1414 = _M0L7vrMod10S1427;
        _M0Lm2vrS1389 = _M0L7vrDiv10S1426;
        _M0Lm2vpS1390 = _M0L7vpDiv10S1425;
        _M0Lm2vmS1391 = _M0L7vmDiv10S1422;
        _M0L6_2atmpS3785 = _M0Lm7removedS1413;
        _M0Lm7removedS1413 = _M0L6_2atmpS3785 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1394) {
      int32_t _M0L6_2atmpS3799 = _M0Lm16lastRemovedDigitS1414;
      if (_M0L6_2atmpS3799 == 5) {
        uint64_t _M0L6_2atmpS3798 = _M0Lm2vrS1389;
        uint64_t _M0L6_2atmpS3797 = _M0L6_2atmpS3798 % 2ull;
        _if__result_5539 = _M0L6_2atmpS3797 == 0ull;
      } else {
        _if__result_5539 = 0;
      }
    } else {
      _if__result_5539 = 0;
    }
    if (_if__result_5539) {
      _M0Lm16lastRemovedDigitS1414 = 4;
    }
    _M0L6_2atmpS3800 = _M0Lm2vrS1389;
    _M0L6_2atmpS3806 = _M0Lm2vrS1389;
    _M0L6_2atmpS3807 = _M0Lm2vmS1391;
    if (_M0L6_2atmpS3806 == _M0L6_2atmpS3807) {
      if (!_M0L4evenS1386) {
        _if__result_5540 = 1;
      } else {
        int32_t _M0L6_2atmpS3805 = _M0Lm17vmIsTrailingZerosS1393;
        _if__result_5540 = !_M0L6_2atmpS3805;
      }
    } else {
      _if__result_5540 = 0;
    }
    if (_if__result_5540) {
      _M0L6_2atmpS3803 = 1;
    } else {
      int32_t _M0L6_2atmpS3804 = _M0Lm16lastRemovedDigitS1414;
      _M0L6_2atmpS3803 = _M0L6_2atmpS3804 >= 5;
    }
    #line 487 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3802 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS3803);
    _M0L6_2atmpS3801 = *(uint64_t*)&_M0L6_2atmpS3802;
    _M0Lm6outputS1415 = _M0L6_2atmpS3800 + _M0L6_2atmpS3801;
  } else {
    int32_t _M0Lm7roundUpS1428 = 0;
    uint64_t _M0L6_2atmpS3828 = _M0Lm2vpS1390;
    uint64_t _M0L8vpDiv100S1429 = _M0L6_2atmpS3828 / 100ull;
    uint64_t _M0L6_2atmpS3827 = _M0Lm2vmS1391;
    uint64_t _M0L8vmDiv100S1430 = _M0L6_2atmpS3827 / 100ull;
    uint64_t _M0L6_2atmpS3822;
    uint64_t _M0L6_2atmpS3825;
    uint64_t _M0L6_2atmpS3826;
    int32_t _M0L6_2atmpS3824;
    uint64_t _M0L6_2atmpS3823;
    if (_M0L8vpDiv100S1429 > _M0L8vmDiv100S1430) {
      uint64_t _M0L6_2atmpS3813 = _M0Lm2vrS1389;
      uint64_t _M0L8vrDiv100S1431 = _M0L6_2atmpS3813 / 100ull;
      uint64_t _M0L6_2atmpS3812 = _M0Lm2vrS1389;
      int32_t _M0L6_2atmpS3809 = (int32_t)_M0L6_2atmpS3812;
      int32_t _M0L6_2atmpS3811 = (int32_t)_M0L8vrDiv100S1431;
      int32_t _M0L6_2atmpS3810 = 100 * _M0L6_2atmpS3811;
      int32_t _M0L8vrMod100S1432 = _M0L6_2atmpS3809 - _M0L6_2atmpS3810;
      int32_t _M0L6_2atmpS3808;
      _M0Lm7roundUpS1428 = _M0L8vrMod100S1432 >= 50;
      _M0Lm2vrS1389 = _M0L8vrDiv100S1431;
      _M0Lm2vpS1390 = _M0L8vpDiv100S1429;
      _M0Lm2vmS1391 = _M0L8vmDiv100S1430;
      _M0L6_2atmpS3808 = _M0Lm7removedS1413;
      _M0Lm7removedS1413 = _M0L6_2atmpS3808 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS3821 = _M0Lm2vpS1390;
      uint64_t _M0L7vpDiv10S1433 = _M0L6_2atmpS3821 / 10ull;
      uint64_t _M0L6_2atmpS3820 = _M0Lm2vmS1391;
      uint64_t _M0L7vmDiv10S1434 = _M0L6_2atmpS3820 / 10ull;
      uint64_t _M0L6_2atmpS3819;
      uint64_t _M0L7vrDiv10S1436;
      uint64_t _M0L6_2atmpS3818;
      int32_t _M0L6_2atmpS3815;
      int32_t _M0L6_2atmpS3817;
      int32_t _M0L6_2atmpS3816;
      int32_t _M0L7vrMod10S1437;
      int32_t _M0L6_2atmpS3814;
      if (_M0L7vpDiv10S1433 <= _M0L7vmDiv10S1434) {
        break;
      }
      _M0L6_2atmpS3819 = _M0Lm2vrS1389;
      _M0L7vrDiv10S1436 = _M0L6_2atmpS3819 / 10ull;
      _M0L6_2atmpS3818 = _M0Lm2vrS1389;
      _M0L6_2atmpS3815 = (int32_t)_M0L6_2atmpS3818;
      _M0L6_2atmpS3817 = (int32_t)_M0L7vrDiv10S1436;
      _M0L6_2atmpS3816 = 10 * _M0L6_2atmpS3817;
      _M0L7vrMod10S1437 = _M0L6_2atmpS3815 - _M0L6_2atmpS3816;
      _M0Lm7roundUpS1428 = _M0L7vrMod10S1437 >= 5;
      _M0Lm2vrS1389 = _M0L7vrDiv10S1436;
      _M0Lm2vpS1390 = _M0L7vpDiv10S1433;
      _M0Lm2vmS1391 = _M0L7vmDiv10S1434;
      _M0L6_2atmpS3814 = _M0Lm7removedS1413;
      _M0Lm7removedS1413 = _M0L6_2atmpS3814 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS3822 = _M0Lm2vrS1389;
    _M0L6_2atmpS3825 = _M0Lm2vrS1389;
    _M0L6_2atmpS3826 = _M0Lm2vmS1391;
    _M0L6_2atmpS3824
    = _M0L6_2atmpS3825 == _M0L6_2atmpS3826 || _M0Lm7roundUpS1428;
    #line 522 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3823 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS3824);
    _M0Lm6outputS1415 = _M0L6_2atmpS3822 + _M0L6_2atmpS3823;
  }
  _M0L6_2atmpS3830 = _M0Lm3e10S1392;
  _M0L6_2atmpS3831 = _M0Lm7removedS1413;
  _M0L3expS1438 = _M0L6_2atmpS3830 + _M0L6_2atmpS3831;
  _M0L6_2atmpS3829 = _M0Lm6outputS1415;
  _block_5542
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_5542)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _block_5542->$0 = _M0L6_2atmpS3829;
  _block_5542->$1 = _M0L3expS1438;
  return _block_5542;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS1381) {
  #line 110 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1381) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS1380) {
  #line 58 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1380) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS1379) {
  #line 32 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1379) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS1378) {
  #line 280 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L1vS1378 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS1378 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS1378 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS1378 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS1378 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS1378 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS1378 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS1378 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS1378 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS1378 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS1378 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS1378 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS1378 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS1378 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS1378 >= 100ull) {
    return 3;
  }
  if (_M0L1vS1378 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS1361) {
  int32_t _M0L6_2atmpS3730;
  int32_t _M0L6_2atmpS3729;
  int32_t _M0L4baseS1360;
  int32_t _M0L5base2S1362;
  int32_t _M0L6offsetS1363;
  int32_t _M0L6_2atmpS3728;
  uint64_t _M0L4mul0S1364;
  int32_t _M0L6_2atmpS3727;
  int32_t _M0L6_2atmpS3726;
  uint64_t _M0L4mul1S1365;
  uint64_t _M0L1mS1366;
  struct _M0TPB7Umul128 _M0L7_2abindS1367;
  uint64_t _M0L7_2alow1S1368;
  uint64_t _M0L8_2ahigh1S1369;
  struct _M0TPB7Umul128 _M0L7_2abindS1370;
  uint64_t _M0L7_2alow0S1371;
  uint64_t _M0L8_2ahigh0S1372;
  uint64_t _M0L3sumS1373;
  uint64_t _M0Lm5high1S1374;
  int32_t _M0L6_2atmpS3724;
  int32_t _M0L6_2atmpS3725;
  int32_t _M0L5deltaS1375;
  uint64_t _M0L6_2atmpS3723;
  uint64_t _M0L6_2atmpS3715;
  int32_t _M0L6_2atmpS3722;
  uint32_t _M0L6_2atmpS3719;
  int32_t _M0L6_2atmpS3721;
  int32_t _M0L6_2atmpS3720;
  uint32_t _M0L6_2atmpS3718;
  uint32_t _M0L6_2atmpS3717;
  uint64_t _M0L6_2atmpS3716;
  uint64_t _M0L1aS1376;
  uint64_t _M0L6_2atmpS3714;
  uint64_t _M0L1bS1377;
  #line 239 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3730 = _M0L1iS1361 + 26;
  _M0L6_2atmpS3729 = _M0L6_2atmpS3730 - 1;
  _M0L4baseS1360 = _M0L6_2atmpS3729 / 26;
  _M0L5base2S1362 = _M0L4baseS1360 * 26;
  _M0L6offsetS1363 = _M0L5base2S1362 - _M0L1iS1361;
  _M0L6_2atmpS3728 = _M0L4baseS1360 * 2;
  #line 243 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1364
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS3728);
  _M0L6_2atmpS3727 = _M0L4baseS1360 * 2;
  _M0L6_2atmpS3726 = _M0L6_2atmpS3727 + 1;
  #line 244 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1365
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS3726);
  if (_M0L6offsetS1363 == 0) {
    return (struct _M0TPB8Pow5Pair){.$0 = _M0L4mul0S1364,
                                      .$1 = _M0L4mul1S1365};
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1mS1366
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1363);
  #line 249 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1367 = _M0FPB7umul128(_M0L1mS1366, _M0L4mul1S1365);
  _M0L7_2alow1S1368 = _M0L7_2abindS1367.$0;
  _M0L8_2ahigh1S1369 = _M0L7_2abindS1367.$1;
  #line 250 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1370 = _M0FPB7umul128(_M0L1mS1366, _M0L4mul0S1364);
  _M0L7_2alow0S1371 = _M0L7_2abindS1370.$0;
  _M0L8_2ahigh0S1372 = _M0L7_2abindS1370.$1;
  _M0L3sumS1373 = _M0L8_2ahigh0S1372 + _M0L7_2alow1S1368;
  _M0Lm5high1S1374 = _M0L8_2ahigh1S1369;
  if (_M0L3sumS1373 < _M0L8_2ahigh0S1372) {
    uint64_t _M0L6_2atmpS3713 = _M0Lm5high1S1374;
    _M0Lm5high1S1374 = _M0L6_2atmpS3713 + 1ull;
  }
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3724 = _M0FPB8pow5bits(_M0L5base2S1362);
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3725 = _M0FPB8pow5bits(_M0L1iS1361);
  _M0L5deltaS1375 = _M0L6_2atmpS3724 - _M0L6_2atmpS3725;
  #line 257 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3723
  = _M0FPB13shiftright128(_M0L7_2alow0S1371, _M0L3sumS1373, _M0L5deltaS1375);
  _M0L6_2atmpS3715 = _M0L6_2atmpS3723 + 1ull;
  _M0L6_2atmpS3722 = _M0L1iS1361 / 16;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3719
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS3722);
  _M0L6_2atmpS3721 = _M0L1iS1361 % 16;
  _M0L6_2atmpS3720 = _M0L6_2atmpS3721 << 1;
  _M0L6_2atmpS3718 = _M0L6_2atmpS3719 >> (_M0L6_2atmpS3720 & 31);
  _M0L6_2atmpS3717 = _M0L6_2atmpS3718 & 3u;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3716 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS3717);
  _M0L1aS1376 = _M0L6_2atmpS3715 + _M0L6_2atmpS3716;
  _M0L6_2atmpS3714 = _M0Lm5high1S1374;
  #line 260 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1377
  = _M0FPB13shiftright128(_M0L3sumS1373, _M0L6_2atmpS3714, _M0L5deltaS1375);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1376, .$1 = _M0L1bS1377};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS1343) {
  int32_t _M0L4baseS1342;
  int32_t _M0L5base2S1344;
  int32_t _M0L6offsetS1345;
  int32_t _M0L6_2atmpS3712;
  uint64_t _M0L4mul0S1346;
  int32_t _M0L6_2atmpS3711;
  int32_t _M0L6_2atmpS3710;
  uint64_t _M0L4mul1S1347;
  uint64_t _M0L1mS1348;
  struct _M0TPB7Umul128 _M0L7_2abindS1349;
  uint64_t _M0L7_2alow1S1350;
  uint64_t _M0L8_2ahigh1S1351;
  struct _M0TPB7Umul128 _M0L7_2abindS1352;
  uint64_t _M0L7_2alow0S1353;
  uint64_t _M0L8_2ahigh0S1354;
  uint64_t _M0L3sumS1355;
  uint64_t _M0Lm5high1S1356;
  int32_t _M0L6_2atmpS3708;
  int32_t _M0L6_2atmpS3709;
  int32_t _M0L5deltaS1357;
  uint64_t _M0L6_2atmpS3700;
  int32_t _M0L6_2atmpS3707;
  uint32_t _M0L6_2atmpS3704;
  int32_t _M0L6_2atmpS3706;
  int32_t _M0L6_2atmpS3705;
  uint32_t _M0L6_2atmpS3703;
  uint32_t _M0L6_2atmpS3702;
  uint64_t _M0L6_2atmpS3701;
  uint64_t _M0L1aS1358;
  uint64_t _M0L6_2atmpS3699;
  uint64_t _M0L1bS1359;
  #line 213 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4baseS1342 = _M0L1iS1343 / 26;
  _M0L5base2S1344 = _M0L4baseS1342 * 26;
  _M0L6offsetS1345 = _M0L1iS1343 - _M0L5base2S1344;
  _M0L6_2atmpS3712 = _M0L4baseS1342 * 2;
  #line 217 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1346
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS3712);
  _M0L6_2atmpS3711 = _M0L4baseS1342 * 2;
  _M0L6_2atmpS3710 = _M0L6_2atmpS3711 + 1;
  #line 218 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1347
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS3710);
  if (_M0L6offsetS1345 == 0) {
    return (struct _M0TPB8Pow5Pair){.$0 = _M0L4mul0S1346,
                                      .$1 = _M0L4mul1S1347};
  }
  #line 222 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1mS1348
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1345);
  #line 223 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1349 = _M0FPB7umul128(_M0L1mS1348, _M0L4mul1S1347);
  _M0L7_2alow1S1350 = _M0L7_2abindS1349.$0;
  _M0L8_2ahigh1S1351 = _M0L7_2abindS1349.$1;
  #line 224 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1352 = _M0FPB7umul128(_M0L1mS1348, _M0L4mul0S1346);
  _M0L7_2alow0S1353 = _M0L7_2abindS1352.$0;
  _M0L8_2ahigh0S1354 = _M0L7_2abindS1352.$1;
  _M0L3sumS1355 = _M0L8_2ahigh0S1354 + _M0L7_2alow1S1350;
  _M0Lm5high1S1356 = _M0L8_2ahigh1S1351;
  if (_M0L3sumS1355 < _M0L8_2ahigh0S1354) {
    uint64_t _M0L6_2atmpS3698 = _M0Lm5high1S1356;
    _M0Lm5high1S1356 = _M0L6_2atmpS3698 + 1ull;
  }
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3708 = _M0FPB8pow5bits(_M0L1iS1343);
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3709 = _M0FPB8pow5bits(_M0L5base2S1344);
  _M0L5deltaS1357 = _M0L6_2atmpS3708 - _M0L6_2atmpS3709;
  #line 231 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3700
  = _M0FPB13shiftright128(_M0L7_2alow0S1353, _M0L3sumS1355, _M0L5deltaS1357);
  _M0L6_2atmpS3707 = _M0L1iS1343 / 16;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3704
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS3707);
  _M0L6_2atmpS3706 = _M0L1iS1343 % 16;
  _M0L6_2atmpS3705 = _M0L6_2atmpS3706 << 1;
  _M0L6_2atmpS3703 = _M0L6_2atmpS3704 >> (_M0L6_2atmpS3705 & 31);
  _M0L6_2atmpS3702 = _M0L6_2atmpS3703 & 3u;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3701 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS3702);
  _M0L1aS1358 = _M0L6_2atmpS3700 + _M0L6_2atmpS3701;
  _M0L6_2atmpS3699 = _M0Lm5high1S1356;
  #line 233 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1359
  = _M0FPB13shiftright128(_M0L3sumS1355, _M0L6_2atmpS3699, _M0L5deltaS1357);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1358, .$1 = _M0L1bS1359};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS1316,
  struct _M0TPB8Pow5Pair _M0L3mulS1313,
  int32_t _M0L1jS1329,
  int32_t _M0L7mmShiftS1331
) {
  uint64_t _M0L7_2amul0S1312;
  uint64_t _M0L7_2amul1S1314;
  uint64_t _M0L1mS1315;
  struct _M0TPB7Umul128 _M0L7_2abindS1317;
  uint64_t _M0L5_2aloS1318;
  uint64_t _M0L6_2atmpS1319;
  struct _M0TPB7Umul128 _M0L7_2abindS1320;
  uint64_t _M0L6_2alo2S1321;
  uint64_t _M0L6_2ahi2S1322;
  uint64_t _M0L3midS1323;
  uint64_t _M0L6_2atmpS3697;
  uint64_t _M0L2hiS1324;
  uint64_t _M0L3lo2S1325;
  uint64_t _M0L6_2atmpS3695;
  uint64_t _M0L6_2atmpS3696;
  uint64_t _M0L4mid2S1326;
  uint64_t _M0L6_2atmpS3694;
  uint64_t _M0L3hi2S1327;
  int32_t _M0L6_2atmpS3693;
  int32_t _M0L6_2atmpS3692;
  uint64_t _M0L2vpS1328;
  uint64_t _M0Lm2vmS1330;
  int32_t _M0L6_2atmpS3691;
  int32_t _M0L6_2atmpS3690;
  uint64_t _M0L2vrS1341;
  uint64_t _M0L6_2atmpS3689;
  #line 129 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2amul0S1312 = _M0L3mulS1313.$0;
  _M0L7_2amul1S1314 = _M0L3mulS1313.$1;
  _M0L1mS1315 = _M0L1mS1316 << 1;
  #line 137 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1317 = _M0FPB7umul128(_M0L1mS1315, _M0L7_2amul0S1312);
  _M0L5_2aloS1318 = _M0L7_2abindS1317.$0;
  _M0L6_2atmpS1319 = _M0L7_2abindS1317.$1;
  #line 138 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1320 = _M0FPB7umul128(_M0L1mS1315, _M0L7_2amul1S1314);
  _M0L6_2alo2S1321 = _M0L7_2abindS1320.$0;
  _M0L6_2ahi2S1322 = _M0L7_2abindS1320.$1;
  _M0L3midS1323 = _M0L6_2atmpS1319 + _M0L6_2alo2S1321;
  if (_M0L3midS1323 < _M0L6_2atmpS1319) {
    _M0L6_2atmpS3697 = 1ull;
  } else {
    _M0L6_2atmpS3697 = 0ull;
  }
  _M0L2hiS1324 = _M0L6_2ahi2S1322 + _M0L6_2atmpS3697;
  _M0L3lo2S1325 = _M0L5_2aloS1318 + _M0L7_2amul0S1312;
  _M0L6_2atmpS3695 = _M0L3midS1323 + _M0L7_2amul1S1314;
  if (_M0L3lo2S1325 < _M0L5_2aloS1318) {
    _M0L6_2atmpS3696 = 1ull;
  } else {
    _M0L6_2atmpS3696 = 0ull;
  }
  _M0L4mid2S1326 = _M0L6_2atmpS3695 + _M0L6_2atmpS3696;
  if (_M0L4mid2S1326 < _M0L3midS1323) {
    _M0L6_2atmpS3694 = 1ull;
  } else {
    _M0L6_2atmpS3694 = 0ull;
  }
  _M0L3hi2S1327 = _M0L2hiS1324 + _M0L6_2atmpS3694;
  _M0L6_2atmpS3693 = _M0L1jS1329 - 64;
  _M0L6_2atmpS3692 = _M0L6_2atmpS3693 - 1;
  #line 144 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vpS1328
  = _M0FPB13shiftright128(_M0L4mid2S1326, _M0L3hi2S1327, _M0L6_2atmpS3692);
  _M0Lm2vmS1330 = 0ull;
  if (_M0L7mmShiftS1331) {
    uint64_t _M0L3lo3S1332 = _M0L5_2aloS1318 - _M0L7_2amul0S1312;
    uint64_t _M0L6_2atmpS3679 = _M0L3midS1323 - _M0L7_2amul1S1314;
    uint64_t _M0L6_2atmpS3680;
    uint64_t _M0L4mid3S1333;
    uint64_t _M0L6_2atmpS3678;
    uint64_t _M0L3hi3S1334;
    int32_t _M0L6_2atmpS3677;
    int32_t _M0L6_2atmpS3676;
    if (_M0L5_2aloS1318 < _M0L3lo3S1332) {
      _M0L6_2atmpS3680 = 1ull;
    } else {
      _M0L6_2atmpS3680 = 0ull;
    }
    _M0L4mid3S1333 = _M0L6_2atmpS3679 - _M0L6_2atmpS3680;
    if (_M0L3midS1323 < _M0L4mid3S1333) {
      _M0L6_2atmpS3678 = 1ull;
    } else {
      _M0L6_2atmpS3678 = 0ull;
    }
    _M0L3hi3S1334 = _M0L2hiS1324 - _M0L6_2atmpS3678;
    _M0L6_2atmpS3677 = _M0L1jS1329 - 64;
    _M0L6_2atmpS3676 = _M0L6_2atmpS3677 - 1;
    #line 150 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1330
    = _M0FPB13shiftright128(_M0L4mid3S1333, _M0L3hi3S1334, _M0L6_2atmpS3676);
  } else {
    uint64_t _M0L3lo3S1335 = _M0L5_2aloS1318 + _M0L5_2aloS1318;
    uint64_t _M0L6_2atmpS3687 = _M0L3midS1323 + _M0L3midS1323;
    uint64_t _M0L6_2atmpS3688;
    uint64_t _M0L4mid3S1336;
    uint64_t _M0L6_2atmpS3685;
    uint64_t _M0L6_2atmpS3686;
    uint64_t _M0L3hi3S1337;
    uint64_t _M0L3lo4S1338;
    uint64_t _M0L6_2atmpS3683;
    uint64_t _M0L6_2atmpS3684;
    uint64_t _M0L4mid4S1339;
    uint64_t _M0L6_2atmpS3682;
    uint64_t _M0L3hi4S1340;
    int32_t _M0L6_2atmpS3681;
    if (_M0L3lo3S1335 < _M0L5_2aloS1318) {
      _M0L6_2atmpS3688 = 1ull;
    } else {
      _M0L6_2atmpS3688 = 0ull;
    }
    _M0L4mid3S1336 = _M0L6_2atmpS3687 + _M0L6_2atmpS3688;
    _M0L6_2atmpS3685 = _M0L2hiS1324 + _M0L2hiS1324;
    if (_M0L4mid3S1336 < _M0L3midS1323) {
      _M0L6_2atmpS3686 = 1ull;
    } else {
      _M0L6_2atmpS3686 = 0ull;
    }
    _M0L3hi3S1337 = _M0L6_2atmpS3685 + _M0L6_2atmpS3686;
    _M0L3lo4S1338 = _M0L3lo3S1335 - _M0L7_2amul0S1312;
    _M0L6_2atmpS3683 = _M0L4mid3S1336 - _M0L7_2amul1S1314;
    if (_M0L3lo3S1335 < _M0L3lo4S1338) {
      _M0L6_2atmpS3684 = 1ull;
    } else {
      _M0L6_2atmpS3684 = 0ull;
    }
    _M0L4mid4S1339 = _M0L6_2atmpS3683 - _M0L6_2atmpS3684;
    if (_M0L4mid3S1336 < _M0L4mid4S1339) {
      _M0L6_2atmpS3682 = 1ull;
    } else {
      _M0L6_2atmpS3682 = 0ull;
    }
    _M0L3hi4S1340 = _M0L3hi3S1337 - _M0L6_2atmpS3682;
    _M0L6_2atmpS3681 = _M0L1jS1329 - 64;
    #line 158 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1330
    = _M0FPB13shiftright128(_M0L4mid4S1339, _M0L3hi4S1340, _M0L6_2atmpS3681);
  }
  _M0L6_2atmpS3691 = _M0L1jS1329 - 64;
  _M0L6_2atmpS3690 = _M0L6_2atmpS3691 - 1;
  #line 160 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vrS1341
  = _M0FPB13shiftright128(_M0L3midS1323, _M0L2hiS1324, _M0L6_2atmpS3690);
  _M0L6_2atmpS3689 = _M0Lm2vmS1330;
  return (struct _M0TPB19MulShiftAll64Result){.$0 = _M0L2vrS1341,
                                                .$1 = _M0L2vpS1328,
                                                .$2 = _M0L6_2atmpS3689};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS1310,
  int32_t _M0L1pS1311
) {
  uint64_t _M0L6_2atmpS3675;
  uint64_t _M0L6_2atmpS3674;
  uint64_t _M0L6_2atmpS3673;
  #line 124 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3675 = 1ull << (_M0L1pS1311 & 63);
  _M0L6_2atmpS3674 = _M0L6_2atmpS3675 - 1ull;
  _M0L6_2atmpS3673 = _M0L5valueS1310 & _M0L6_2atmpS3674;
  return _M0L6_2atmpS3673 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS1308,
  int32_t _M0L1pS1309
) {
  int32_t _M0L6_2atmpS3672;
  #line 119 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 120 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3672 = _M0FPB10pow5Factor(_M0L5valueS1308);
  return _M0L6_2atmpS3672 >= _M0L1pS1309;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS1303) {
  uint64_t _M0L6_2atmpS3663;
  uint64_t _M0L6_2atmpS3664;
  uint64_t _M0L6_2atmpS3665;
  uint64_t _M0L6_2atmpS3666;
  uint64_t _M0L6_2atmpS3671;
  int32_t _M0L5countS1304;
  uint64_t _M0L1vS1305;
  #line 94 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3663 = _M0L5valueS1303 % 5ull;
  if (_M0L6_2atmpS3663 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3664 = _M0L5valueS1303 % 25ull;
  if (_M0L6_2atmpS3664 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS3665 = _M0L5valueS1303 % 125ull;
  if (_M0L6_2atmpS3665 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS3666 = _M0L5valueS1303 % 625ull;
  if (_M0L6_2atmpS3666 != 0ull) {
    return 3;
  }
  _M0L6_2atmpS3671 = _M0L5valueS1303 / 625ull;
  _M0L5countS1304 = 4;
  _M0L1vS1305 = _M0L6_2atmpS3671;
  while (1) {
    if (_M0L1vS1305 > 0ull) {
      uint64_t _M0L6_2atmpS3667 = _M0L1vS1305 % 5ull;
      int32_t _M0L6_2atmpS3668;
      uint64_t _M0L6_2atmpS3669;
      if (_M0L6_2atmpS3667 != 0ull) {
        return _M0L5countS1304;
      }
      _M0L6_2atmpS3668 = _M0L5countS1304 + 1;
      _M0L6_2atmpS3669 = _M0L1vS1305 / 5ull;
      _M0L5countS1304 = _M0L6_2atmpS3668;
      _M0L1vS1305 = _M0L6_2atmpS3669;
      continue;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1307;
      moonbit_string_t _M0L6_2atmpS3670;
      int32_t _result_5544;
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L18_2astring__builderS1307
      = _M0MPB13StringBuilder21StringBuilder_2einner(25);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1307, (moonbit_string_t)moonbit_string_literal_139.data);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0MPB13StringBuilder13write__objectGmE(_M0L18_2astring__builderS1307, _M0L5valueS1303);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L6_2atmpS3670
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1307);
      moonbit_decref(_M0L18_2astring__builderS1307);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _result_5544 = _M0FPC15abort5abortGiE(_M0L6_2atmpS3670);
      moonbit_decref(_M0L6_2atmpS3670);
      return _result_5544;
    }
    break;
  }
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS1302,
  uint64_t _M0L2hiS1300,
  int32_t _M0L4distS1301
) {
  int32_t _M0L6_2atmpS3662;
  uint64_t _M0L6_2atmpS3660;
  uint64_t _M0L6_2atmpS3661;
  #line 89 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3662 = 64 - _M0L4distS1301;
  _M0L6_2atmpS3660 = _M0L2hiS1300 << (_M0L6_2atmpS3662 & 63);
  _M0L6_2atmpS3661 = _M0L2loS1302 >> (_M0L4distS1301 & 63);
  return _M0L6_2atmpS3660 | _M0L6_2atmpS3661;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS1290,
  uint64_t _M0L1bS1293
) {
  uint64_t _M0L3aLoS1289;
  uint64_t _M0L3aHiS1291;
  uint64_t _M0L3bLoS1292;
  uint64_t _M0L3bHiS1294;
  uint64_t _M0L1xS1295;
  uint64_t _M0L6_2atmpS3658;
  uint64_t _M0L6_2atmpS3659;
  uint64_t _M0L1yS1296;
  uint64_t _M0L6_2atmpS3656;
  uint64_t _M0L6_2atmpS3657;
  uint64_t _M0L1zS1297;
  uint64_t _M0L6_2atmpS3654;
  uint64_t _M0L6_2atmpS3655;
  uint64_t _M0L6_2atmpS3652;
  uint64_t _M0L6_2atmpS3653;
  uint64_t _M0L1wS1298;
  uint64_t _M0L2loS1299;
  #line 74 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3aLoS1289 = _M0L1aS1290 & 4294967295ull;
  _M0L3aHiS1291 = _M0L1aS1290 >> 32;
  _M0L3bLoS1292 = _M0L1bS1293 & 4294967295ull;
  _M0L3bHiS1294 = _M0L1bS1293 >> 32;
  _M0L1xS1295 = _M0L3aLoS1289 * _M0L3bLoS1292;
  _M0L6_2atmpS3658 = _M0L3aHiS1291 * _M0L3bLoS1292;
  _M0L6_2atmpS3659 = _M0L1xS1295 >> 32;
  _M0L1yS1296 = _M0L6_2atmpS3658 + _M0L6_2atmpS3659;
  _M0L6_2atmpS3656 = _M0L3aLoS1289 * _M0L3bHiS1294;
  _M0L6_2atmpS3657 = _M0L1yS1296 & 4294967295ull;
  _M0L1zS1297 = _M0L6_2atmpS3656 + _M0L6_2atmpS3657;
  _M0L6_2atmpS3654 = _M0L3aHiS1291 * _M0L3bHiS1294;
  _M0L6_2atmpS3655 = _M0L1yS1296 >> 32;
  _M0L6_2atmpS3652 = _M0L6_2atmpS3654 + _M0L6_2atmpS3655;
  _M0L6_2atmpS3653 = _M0L1zS1297 >> 32;
  _M0L1wS1298 = _M0L6_2atmpS3652 + _M0L6_2atmpS3653;
  _M0L2loS1299 = _M0L1aS1290 * _M0L1bS1293;
  return (struct _M0TPB7Umul128){.$0 = _M0L2loS1299, .$1 = _M0L1wS1298};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS1287,
  int32_t _M0L4fromS1284,
  int32_t _M0L2toS1283
) {
  int32_t _M0L3lenS1282;
  int32_t _M0L6_2atmpS3651;
  uint16_t* _M0L6bufferS1285;
  int32_t _M0L1iS1286;
  #line 52 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3lenS1282 = _M0L2toS1283 - _M0L4fromS1284;
  #line 54 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3651 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L6bufferS1285
  = (uint16_t*)moonbit_make_string(_M0L3lenS1282, _M0L6_2atmpS3651);
  _M0L1iS1286 = 0;
  while (1) {
    if (_M0L1iS1286 < _M0L3lenS1282) {
      int32_t _M0L6_2atmpS3649 = _M0L4fromS1284 + _M0L1iS1286;
      int32_t _M0L6_2atmpS3648;
      int32_t _M0L6_2atmpS3647;
      int32_t _M0L6_2atmpS3650;
      if (
        _M0L6_2atmpS3649 < 0
        || _M0L6_2atmpS3649 >= Moonbit_array_length(_M0L5bytesS1287)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3648 = (int32_t)_M0L5bytesS1287[_M0L6_2atmpS3649];
      _M0L6_2atmpS3647 = (uint16_t)_M0L6_2atmpS3648;
      if (
        _M0L1iS1286 < 0
        || _M0L1iS1286 >= Moonbit_array_length(_M0L6bufferS1285)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6bufferS1285[_M0L1iS1286] = _M0L6_2atmpS3647;
      _M0L6_2atmpS3650 = _M0L1iS1286 + 1;
      _M0L1iS1286 = _M0L6_2atmpS3650;
      continue;
    }
    break;
  }
  return _M0L6bufferS1285;
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS1281) {
  int32_t _M0L6_2atmpS3646;
  uint32_t _M0L6_2atmpS3645;
  uint32_t _M0L6_2atmpS3644;
  #line 44 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3646 = _M0L1eS1281 * 78913;
  _M0L6_2atmpS3645 = *(uint32_t*)&_M0L6_2atmpS3646;
  _M0L6_2atmpS3644 = _M0L6_2atmpS3645 >> 18;
  return *(int32_t*)&_M0L6_2atmpS3644;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS1280) {
  int32_t _M0L6_2atmpS3643;
  uint32_t _M0L6_2atmpS3642;
  uint32_t _M0L6_2atmpS3641;
  #line 37 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3643 = _M0L1eS1280 * 732923;
  _M0L6_2atmpS3642 = *(uint32_t*)&_M0L6_2atmpS3643;
  _M0L6_2atmpS3641 = _M0L6_2atmpS3642 >> 20;
  return *(int32_t*)&_M0L6_2atmpS3641;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS1278,
  int32_t _M0L8exponentS1279,
  int32_t _M0L8mantissaS1276
) {
  moonbit_string_t _M0L1sS1277;
  moonbit_string_t _result_5547;
  #line 23 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L8mantissaS1276) {
    return (moonbit_string_t)moonbit_string_literal_140.data;
  }
  if (_M0L4signS1278) {
    _M0L1sS1277 = (moonbit_string_t)moonbit_string_literal_132.data;
  } else {
    _M0L1sS1277 = (moonbit_string_t)moonbit_string_literal_75.data;
  }
  if (_M0L8exponentS1279) {
    moonbit_string_t _result_5546;
    #line 29 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_5546
    = moonbit_add_string(_M0L1sS1277, (moonbit_string_t)moonbit_string_literal_141.data);
    moonbit_decref(_M0L1sS1277);
    return _result_5546;
  }
  #line 31 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_5547
  = moonbit_add_string(_M0L1sS1277, (moonbit_string_t)moonbit_string_literal_142.data);
  moonbit_decref(_M0L1sS1277);
  return _result_5547;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS1275) {
  int32_t _M0L6_2atmpS3640;
  uint32_t _M0L6_2atmpS3639;
  uint32_t _M0L6_2atmpS3638;
  int32_t _M0L6_2atmpS3637;
  #line 18 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3640 = _M0L1eS1275 * 1217359;
  _M0L6_2atmpS3639 = *(uint32_t*)&_M0L6_2atmpS3640;
  _M0L6_2atmpS3638 = _M0L6_2atmpS3639 >> 19;
  _M0L6_2atmpS3637 = *(int32_t*)&_M0L6_2atmpS3638;
  return _M0L6_2atmpS3637 + 1;
}

int32_t _M0IPC16string6StringPB4Hash4hash(moonbit_string_t _M0L4selfS1271) {
  int32_t _tmp_5548;
  uint32_t _M0L6_2atmpS3636;
  uint32_t _M0Lm3accS1269;
  int32_t _M0L7_2abindS1270;
  int32_t _M0L1iS1272;
  uint32_t _M0L6_2atmpS3635;
  #line 522 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _tmp_5548 = 0;
  _M0L6_2atmpS3636 = *(uint32_t*)&_tmp_5548;
  _M0Lm3accS1269 = _M0L6_2atmpS3636 + 374761393u;
  _M0L7_2abindS1270 = Moonbit_array_length(_M0L4selfS1271);
  _M0L1iS1272 = 0;
  while (1) {
    if (_M0L1iS1272 < _M0L7_2abindS1270) {
      uint32_t _M0L6_2atmpS3630 = _M0Lm3accS1269;
      int32_t _M0L6_2atmpS3633;
      int32_t _M0L6_2atmpS3632;
      uint32_t _M0L1vS1273;
      uint32_t _M0L6_2atmpS3631;
      int32_t _M0L6_2atmpS3634;
      _M0Lm3accS1269 = _M0L6_2atmpS3630 + 4u;
      _M0L6_2atmpS3633 = _M0L4selfS1271[_M0L1iS1272];
      _M0L6_2atmpS3632 = (int32_t)_M0L6_2atmpS3633;
      _M0L1vS1273 = *(uint32_t*)&_M0L6_2atmpS3632;
      _M0L6_2atmpS3631 = _M0Lm3accS1269;
      #line 527 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
      _M0Lm3accS1269 = _M0FPB13consume4__acc(_M0L6_2atmpS3631, _M0L1vS1273);
      _M0L6_2atmpS3634 = _M0L1iS1272 + 1;
      _M0L1iS1272 = _M0L6_2atmpS3634;
      continue;
    }
    break;
  }
  _M0L6_2atmpS3635 = _M0Lm3accS1269;
  #line 529 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  return _M0FPB13finalize__acc(_M0L6_2atmpS3635);
}

struct _M0TUssE* _M0MPB5Iter24nextGssE(
  struct _M0TPB4IterGUssEE* _M0L4selfS1264
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUssEE(_M0L4selfS1264);
}

struct _M0TUsbE* _M0MPB5Iter24nextGsbE(
  struct _M0TPB4IterGUsbEE* _M0L4selfS1265
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsbEE(_M0L4selfS1265);
}

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L4selfS1266
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsRP19moonbitDB10RedisValueEE(_M0L4selfS1266);
}

struct _M0TUsfE* _M0MPB5Iter24nextGsfE(
  struct _M0TPB4IterGUsfEE* _M0L4selfS1267
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsfEE(_M0L4selfS1267);
}

struct _M0TUsiE* _M0MPB5Iter24nextGsiE(
  struct _M0TPB4IterGUsiEE* _M0L4selfS1268
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsiEE(_M0L4selfS1268);
}

struct _M0TPB4IterGUssEE* _M0MPB3Map5iter2GssE(
  struct _M0TPB3MapGssE* _M0L4selfS1259
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGssE(_M0L4selfS1259);
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map5iter2GsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1260
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsbE(_M0L4selfS1260);
}

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS1261
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsRP19moonbitDB10RedisValueE(_M0L4selfS1261);
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map5iter2GsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1262
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsfE(_M0L4selfS1262);
}

struct _M0TPB4IterGUsiEE* _M0MPB3Map5iter2GsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1263
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsiE(_M0L4selfS1263);
}

struct _M0TPB4IterGUssEE* _M0MPB3Map4iterGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1205
) {
  struct _M0TPB5EntryGssE* _M0L4headS3589;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS1204;
  int32_t _M0L3lenS1206;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1207;
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3582__l711__* _closure_5550;
  struct _M0TWEOUssE* _M0L6_2atmpS3580;
  int64_t _M0L6_2atmpS3581;
  struct _M0TPB4IterGUssEE* _result_5551;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3589 = _M0L4selfS1205->$5;
  if (_M0L4headS3589) {
    moonbit_incref(_M0L4headS3589);
  }
  _M0L11curr__entryS1204
  = (struct _M0TPB8MutLocalGORPB5EntryGssEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGssEE));
  Moonbit_object_header(_M0L11curr__entryS1204)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 35, 0);
  _M0L11curr__entryS1204->$0 = _M0L4headS3589;
  _M0L3lenS1206 = _M0L4selfS1205->$1;
  _M0L9remainingS1207
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1207)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1207->$0 = _M0L3lenS1206;
  _closure_5550
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3582__l711__*)moonbit_malloc(sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3582__l711__));
  Moonbit_object_header(_closure_5550)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 38, 0);
  _closure_5550->code = &_M0MPB3Map4iterGssEC3582l711;
  _closure_5550->$0 = _M0L9remainingS1207;
  _closure_5550->$1 = _M0L11curr__entryS1204;
  _M0L6_2atmpS3580 = (struct _M0TWEOUssE*)_closure_5550;
  _M0L6_2atmpS3581 = (int64_t)_M0L3lenS1206;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_5551 = _M0MPB4Iter3newGUssEE(_M0L6_2atmpS3580, _M0L6_2atmpS3581);
  moonbit_decref(_M0L6_2atmpS3580);
  return _result_5551;
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1216
) {
  struct _M0TPB5EntryGsbE* _M0L4headS3599;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS1215;
  int32_t _M0L3lenS1217;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1218;
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3592__l711__* _closure_5552;
  struct _M0TWEOUsbE* _M0L6_2atmpS3590;
  int64_t _M0L6_2atmpS3591;
  struct _M0TPB4IterGUsbEE* _result_5553;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3599 = _M0L4selfS1216->$5;
  if (_M0L4headS3599) {
    moonbit_incref(_M0L4headS3599);
  }
  _M0L11curr__entryS1215
  = (struct _M0TPB8MutLocalGORPB5EntryGsbEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE));
  Moonbit_object_header(_M0L11curr__entryS1215)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 42, 0);
  _M0L11curr__entryS1215->$0 = _M0L4headS3599;
  _M0L3lenS1217 = _M0L4selfS1216->$1;
  _M0L9remainingS1218
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1218)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1218->$0 = _M0L3lenS1217;
  _closure_5552
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3592__l711__*)moonbit_malloc(sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3592__l711__));
  Moonbit_object_header(_closure_5552)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 45, 0);
  _closure_5552->code = &_M0MPB3Map4iterGsbEC3592l711;
  _closure_5552->$0 = _M0L9remainingS1218;
  _closure_5552->$1 = _M0L11curr__entryS1215;
  _M0L6_2atmpS3590 = (struct _M0TWEOUsbE*)_closure_5552;
  _M0L6_2atmpS3591 = (int64_t)_M0L3lenS1217;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_5553 = _M0MPB4Iter3newGUsbEE(_M0L6_2atmpS3590, _M0L6_2atmpS3591);
  moonbit_decref(_M0L6_2atmpS3590);
  return _result_5553;
}

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB3Map4iterGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS1227
) {
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4headS3609;
  struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE* _M0L11curr__entryS1226;
  int32_t _M0L3lenS1228;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1229;
  struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3602__l711__* _closure_5554;
  struct _M0TWEOUsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3600;
  int64_t _M0L6_2atmpS3601;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _result_5555;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3609 = _M0L4selfS1227->$5;
  if (_M0L4headS3609) {
    moonbit_incref(_M0L4headS3609);
  }
  _M0L11curr__entryS1226
  = (struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE));
  Moonbit_object_header(_M0L11curr__entryS1226)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 49, 0);
  _M0L11curr__entryS1226->$0 = _M0L4headS3609;
  _M0L3lenS1228 = _M0L4selfS1227->$1;
  _M0L9remainingS1229
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1229)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1229->$0 = _M0L3lenS1228;
  _closure_5554
  = (struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3602__l711__*)moonbit_malloc(sizeof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3602__l711__));
  Moonbit_object_header(_closure_5554)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 52, 0);
  _closure_5554->code = &_M0MPB3Map4iterGsRP19moonbitDB10RedisValueEC3602l711;
  _closure_5554->$0 = _M0L9remainingS1229;
  _closure_5554->$1 = _M0L11curr__entryS1226;
  _M0L6_2atmpS3600
  = (struct _M0TWEOUsRP19moonbitDB10RedisValueE*)_closure_5554;
  _M0L6_2atmpS3601 = (int64_t)_M0L3lenS1228;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_5555
  = _M0MPB4Iter3newGUsRP19moonbitDB10RedisValueEE(_M0L6_2atmpS3600, _M0L6_2atmpS3601);
  moonbit_decref(_M0L6_2atmpS3600);
  return _result_5555;
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1238
) {
  struct _M0TPB5EntryGsfE* _M0L4headS3619;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS1237;
  int32_t _M0L3lenS1239;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1240;
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3612__l711__* _closure_5556;
  struct _M0TWEOUsfE* _M0L6_2atmpS3610;
  int64_t _M0L6_2atmpS3611;
  struct _M0TPB4IterGUsfEE* _result_5557;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3619 = _M0L4selfS1238->$5;
  if (_M0L4headS3619) {
    moonbit_incref(_M0L4headS3619);
  }
  _M0L11curr__entryS1237
  = (struct _M0TPB8MutLocalGORPB5EntryGsfEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE));
  Moonbit_object_header(_M0L11curr__entryS1237)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 56, 0);
  _M0L11curr__entryS1237->$0 = _M0L4headS3619;
  _M0L3lenS1239 = _M0L4selfS1238->$1;
  _M0L9remainingS1240
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1240)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1240->$0 = _M0L3lenS1239;
  _closure_5556
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3612__l711__*)moonbit_malloc(sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3612__l711__));
  Moonbit_object_header(_closure_5556)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 59, 0);
  _closure_5556->code = &_M0MPB3Map4iterGsfEC3612l711;
  _closure_5556->$0 = _M0L9remainingS1240;
  _closure_5556->$1 = _M0L11curr__entryS1237;
  _M0L6_2atmpS3610 = (struct _M0TWEOUsfE*)_closure_5556;
  _M0L6_2atmpS3611 = (int64_t)_M0L3lenS1239;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_5557 = _M0MPB4Iter3newGUsfEE(_M0L6_2atmpS3610, _M0L6_2atmpS3611);
  moonbit_decref(_M0L6_2atmpS3610);
  return _result_5557;
}

struct _M0TPB4IterGUsiEE* _M0MPB3Map4iterGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1249
) {
  struct _M0TPB5EntryGsiE* _M0L4headS3629;
  struct _M0TPB8MutLocalGORPB5EntryGsiEE* _M0L11curr__entryS1248;
  int32_t _M0L3lenS1250;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1251;
  struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3622__l711__* _closure_5558;
  struct _M0TWEOUsiE* _M0L6_2atmpS3620;
  int64_t _M0L6_2atmpS3621;
  struct _M0TPB4IterGUsiEE* _result_5559;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3629 = _M0L4selfS1249->$5;
  if (_M0L4headS3629) {
    moonbit_incref(_M0L4headS3629);
  }
  _M0L11curr__entryS1248
  = (struct _M0TPB8MutLocalGORPB5EntryGsiEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsiEE));
  Moonbit_object_header(_M0L11curr__entryS1248)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 63, 0);
  _M0L11curr__entryS1248->$0 = _M0L4headS3629;
  _M0L3lenS1250 = _M0L4selfS1249->$1;
  _M0L9remainingS1251
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1251)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1251->$0 = _M0L3lenS1250;
  _closure_5558
  = (struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3622__l711__*)moonbit_malloc(sizeof(struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3622__l711__));
  Moonbit_object_header(_closure_5558)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 66, 0);
  _closure_5558->code = &_M0MPB3Map4iterGsiEC3622l711;
  _closure_5558->$0 = _M0L9remainingS1251;
  _closure_5558->$1 = _M0L11curr__entryS1248;
  _M0L6_2atmpS3620 = (struct _M0TWEOUsiE*)_closure_5558;
  _M0L6_2atmpS3621 = (int64_t)_M0L3lenS1250;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_5559 = _M0MPB4Iter3newGUsiEE(_M0L6_2atmpS3620, _M0L6_2atmpS3621);
  moonbit_decref(_M0L6_2atmpS3620);
  return _result_5559;
}

struct _M0TUsiE* _M0MPB3Map4iterGsiEC3622l711(
  struct _M0TWEOUsiE* _M0L6_2aenvS3623
) {
  struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3622__l711__* _M0L14_2acasted__envS3624;
  struct _M0TPB8MutLocalGORPB5EntryGsiEE* _M0L11curr__entryS1248;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1251;
  int32_t _M0L3valS3625;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3624
  = (struct _M0R62Map_3a_3aiter_7c_5bString_2c_20Int_5d_7c_2eanon__u3622__l711__*)_M0L6_2aenvS3623;
  _M0L11curr__entryS1248 = _M0L14_2acasted__envS3624->$1;
  _M0L9remainingS1251 = _M0L14_2acasted__envS3624->$0;
  _M0L3valS3625 = _M0L9remainingS1251->$0;
  if (_M0L3valS3625 > 0) {
    struct _M0TPB5EntryGsiE* _M0L7_2abindS1253 = _M0L11curr__entryS1248->$0;
    if (_M0L7_2abindS1253 == 0) {
      goto join_1252;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS1254 = _M0L7_2abindS1253;
      struct _M0TPB5EntryGsiE* _M0L4_2axS1255 = _M0L7_2aSomeS1254;
      moonbit_string_t _M0L6_2akeyS1256 = _M0L4_2axS1255->$4;
      int32_t _M0L8_2avalueS1257 = _M0L4_2axS1255->$5;
      struct _M0TPB5EntryGsiE* _M0L7_2anextS1258 = _M0L4_2axS1255->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4836 = _M0L11curr__entryS1248->$0;
      int32_t _M0L3valS3627;
      int32_t _M0L6_2atmpS3626;
      struct _M0TUsiE* _M0L8_2atupleS3628;
      if (_M0L7_2anextS1258) {
        moonbit_incref(_M0L7_2anextS1258);
      }
      moonbit_incref(_M0L6_2akeyS1256);
      if (_M0L6_2aoldS4836) {
        moonbit_decref(_M0L6_2aoldS4836);
      }
      _M0L11curr__entryS1248->$0 = _M0L7_2anextS1258;
      _M0L3valS3627 = _M0L9remainingS1251->$0;
      _M0L6_2atmpS3626 = _M0L3valS3627 - 1;
      _M0L9remainingS1251->$0 = _M0L6_2atmpS3626;
      _M0L8_2atupleS3628
      = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
      Moonbit_object_header(_M0L8_2atupleS3628)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 70, 0);
      _M0L8_2atupleS3628->$0 = _M0L6_2akeyS1256;
      _M0L8_2atupleS3628->$1 = _M0L8_2avalueS1257;
      return _M0L8_2atupleS3628;
    }
  } else {
    goto join_1252;
  }
  join_1252:;
  return 0;
}

struct _M0TUsfE* _M0MPB3Map4iterGsfEC3612l711(
  struct _M0TWEOUsfE* _M0L6_2aenvS3613
) {
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3612__l711__* _M0L14_2acasted__envS3614;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS1237;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1240;
  int32_t _M0L3valS3615;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3614
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3612__l711__*)_M0L6_2aenvS3613;
  _M0L11curr__entryS1237 = _M0L14_2acasted__envS3614->$1;
  _M0L9remainingS1240 = _M0L14_2acasted__envS3614->$0;
  _M0L3valS3615 = _M0L9remainingS1240->$0;
  if (_M0L3valS3615 > 0) {
    struct _M0TPB5EntryGsfE* _M0L7_2abindS1242 = _M0L11curr__entryS1237->$0;
    if (_M0L7_2abindS1242 == 0) {
      goto join_1241;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS1243 = _M0L7_2abindS1242;
      struct _M0TPB5EntryGsfE* _M0L4_2axS1244 = _M0L7_2aSomeS1243;
      moonbit_string_t _M0L6_2akeyS1245 = _M0L4_2axS1244->$4;
      float _M0L8_2avalueS1246 = _M0L4_2axS1244->$5;
      struct _M0TPB5EntryGsfE* _M0L7_2anextS1247 = _M0L4_2axS1244->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4840 = _M0L11curr__entryS1237->$0;
      int32_t _M0L3valS3617;
      int32_t _M0L6_2atmpS3616;
      struct _M0TUsfE* _M0L8_2atupleS3618;
      if (_M0L7_2anextS1247) {
        moonbit_incref(_M0L7_2anextS1247);
      }
      moonbit_incref(_M0L6_2akeyS1245);
      if (_M0L6_2aoldS4840) {
        moonbit_decref(_M0L6_2aoldS4840);
      }
      _M0L11curr__entryS1237->$0 = _M0L7_2anextS1247;
      _M0L3valS3617 = _M0L9remainingS1240->$0;
      _M0L6_2atmpS3616 = _M0L3valS3617 - 1;
      _M0L9remainingS1240->$0 = _M0L6_2atmpS3616;
      _M0L8_2atupleS3618
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS3618)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 9, 0);
      _M0L8_2atupleS3618->$0 = _M0L6_2akeyS1245;
      _M0L8_2atupleS3618->$1 = _M0L8_2avalueS1246;
      return _M0L8_2atupleS3618;
    }
  } else {
    goto join_1241;
  }
  join_1241:;
  return 0;
}

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB3Map4iterGsRP19moonbitDB10RedisValueEC3602l711(
  struct _M0TWEOUsRP19moonbitDB10RedisValueE* _M0L6_2aenvS3603
) {
  struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3602__l711__* _M0L14_2acasted__envS3604;
  struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE* _M0L11curr__entryS1226;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1229;
  int32_t _M0L3valS3605;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3604
  = (struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3602__l711__*)_M0L6_2aenvS3603;
  _M0L11curr__entryS1226 = _M0L14_2acasted__envS3604->$1;
  _M0L9remainingS1229 = _M0L14_2acasted__envS3604->$0;
  _M0L3valS3605 = _M0L9remainingS1229->$0;
  if (_M0L3valS3605 > 0) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS1231 =
      _M0L11curr__entryS1226->$0;
    if (_M0L7_2abindS1231 == 0) {
      goto join_1230;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1232 =
        _M0L7_2abindS1231;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4_2axS1233 =
        _M0L7_2aSomeS1232;
      moonbit_string_t _M0L6_2akeyS1234 = _M0L4_2axS1233->$4;
      void* _M0L8_2avalueS1235 = _M0L4_2axS1233->$5;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2anextS1236 =
        _M0L4_2axS1233->$1;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4844 =
        _M0L11curr__entryS1226->$0;
      int32_t _M0L3valS3607;
      int32_t _M0L6_2atmpS3606;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L8_2atupleS3608;
      if (_M0L7_2anextS1236) {
        moonbit_incref(_M0L7_2anextS1236);
      }
      moonbit_incref(_M0L8_2avalueS1235);
      moonbit_incref(_M0L6_2akeyS1234);
      if (_M0L6_2aoldS4844) {
        moonbit_decref(_M0L6_2aoldS4844);
      }
      _M0L11curr__entryS1226->$0 = _M0L7_2anextS1236;
      _M0L3valS3607 = _M0L9remainingS1229->$0;
      _M0L6_2atmpS3606 = _M0L3valS3607 - 1;
      _M0L9remainingS1229->$0 = _M0L6_2atmpS3606;
      _M0L8_2atupleS3608
      = (struct _M0TUsRP19moonbitDB10RedisValueE*)moonbit_malloc(sizeof(struct _M0TUsRP19moonbitDB10RedisValueE));
      Moonbit_object_header(_M0L8_2atupleS3608)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 73, 0);
      _M0L8_2atupleS3608->$0 = _M0L6_2akeyS1234;
      _M0L8_2atupleS3608->$1 = _M0L8_2avalueS1235;
      return _M0L8_2atupleS3608;
    }
  } else {
    goto join_1230;
  }
  join_1230:;
  return 0;
}

struct _M0TUsbE* _M0MPB3Map4iterGsbEC3592l711(
  struct _M0TWEOUsbE* _M0L6_2aenvS3593
) {
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3592__l711__* _M0L14_2acasted__envS3594;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS1215;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1218;
  int32_t _M0L3valS3595;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3594
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3592__l711__*)_M0L6_2aenvS3593;
  _M0L11curr__entryS1215 = _M0L14_2acasted__envS3594->$1;
  _M0L9remainingS1218 = _M0L14_2acasted__envS3594->$0;
  _M0L3valS3595 = _M0L9remainingS1218->$0;
  if (_M0L3valS3595 > 0) {
    struct _M0TPB5EntryGsbE* _M0L7_2abindS1220 = _M0L11curr__entryS1215->$0;
    if (_M0L7_2abindS1220 == 0) {
      goto join_1219;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS1221 = _M0L7_2abindS1220;
      struct _M0TPB5EntryGsbE* _M0L4_2axS1222 = _M0L7_2aSomeS1221;
      moonbit_string_t _M0L6_2akeyS1223 = _M0L4_2axS1222->$4;
      int32_t _M0L8_2avalueS1224 = _M0L4_2axS1222->$5;
      struct _M0TPB5EntryGsbE* _M0L7_2anextS1225 = _M0L4_2axS1222->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4849 = _M0L11curr__entryS1215->$0;
      int32_t _M0L3valS3597;
      int32_t _M0L6_2atmpS3596;
      struct _M0TUsbE* _M0L8_2atupleS3598;
      if (_M0L7_2anextS1225) {
        moonbit_incref(_M0L7_2anextS1225);
      }
      moonbit_incref(_M0L6_2akeyS1223);
      if (_M0L6_2aoldS4849) {
        moonbit_decref(_M0L6_2aoldS4849);
      }
      _M0L11curr__entryS1215->$0 = _M0L7_2anextS1225;
      _M0L3valS3597 = _M0L9remainingS1218->$0;
      _M0L6_2atmpS3596 = _M0L3valS3597 - 1;
      _M0L9remainingS1218->$0 = _M0L6_2atmpS3596;
      _M0L8_2atupleS3598
      = (struct _M0TUsbE*)moonbit_malloc(sizeof(struct _M0TUsbE));
      Moonbit_object_header(_M0L8_2atupleS3598)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 77, 0);
      _M0L8_2atupleS3598->$0 = _M0L6_2akeyS1223;
      _M0L8_2atupleS3598->$1 = _M0L8_2avalueS1224;
      return _M0L8_2atupleS3598;
    }
  } else {
    goto join_1219;
  }
  join_1219:;
  return 0;
}

struct _M0TUssE* _M0MPB3Map4iterGssEC3582l711(
  struct _M0TWEOUssE* _M0L6_2aenvS3583
) {
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3582__l711__* _M0L14_2acasted__envS3584;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS1204;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1207;
  int32_t _M0L3valS3585;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3584
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3582__l711__*)_M0L6_2aenvS3583;
  _M0L11curr__entryS1204 = _M0L14_2acasted__envS3584->$1;
  _M0L9remainingS1207 = _M0L14_2acasted__envS3584->$0;
  _M0L3valS3585 = _M0L9remainingS1207->$0;
  if (_M0L3valS3585 > 0) {
    struct _M0TPB5EntryGssE* _M0L7_2abindS1209 = _M0L11curr__entryS1204->$0;
    if (_M0L7_2abindS1209 == 0) {
      goto join_1208;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS1210 = _M0L7_2abindS1209;
      struct _M0TPB5EntryGssE* _M0L4_2axS1211 = _M0L7_2aSomeS1210;
      moonbit_string_t _M0L6_2akeyS1212 = _M0L4_2axS1211->$4;
      moonbit_string_t _M0L8_2avalueS1213 = _M0L4_2axS1211->$5;
      struct _M0TPB5EntryGssE* _M0L7_2anextS1214 = _M0L4_2axS1211->$1;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4853 = _M0L11curr__entryS1204->$0;
      int32_t _M0L3valS3587;
      int32_t _M0L6_2atmpS3586;
      struct _M0TUssE* _M0L8_2atupleS3588;
      if (_M0L7_2anextS1214) {
        moonbit_incref(_M0L7_2anextS1214);
      }
      moonbit_incref(_M0L8_2avalueS1213);
      moonbit_incref(_M0L6_2akeyS1212);
      if (_M0L6_2aoldS4853) {
        moonbit_decref(_M0L6_2aoldS4853);
      }
      _M0L11curr__entryS1204->$0 = _M0L7_2anextS1214;
      _M0L3valS3587 = _M0L9remainingS1207->$0;
      _M0L6_2atmpS3586 = _M0L3valS3587 - 1;
      _M0L9remainingS1207->$0 = _M0L6_2atmpS3586;
      _M0L8_2atupleS3588
      = (struct _M0TUssE*)moonbit_malloc(sizeof(struct _M0TUssE));
      Moonbit_object_header(_M0L8_2atupleS3588)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 80, 0);
      _M0L8_2atupleS3588->$0 = _M0L6_2akeyS1212;
      _M0L8_2atupleS3588->$1 = _M0L8_2avalueS1213;
      return _M0L8_2atupleS3588;
    }
  } else {
    goto join_1208;
  }
  join_1208:;
  return 0;
}

int32_t _M0MPB3Map6lengthGssE(struct _M0TPB3MapGssE* _M0L4selfS1201) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS1201->$1;
}

int32_t _M0MPB3Map6lengthGsbE(struct _M0TPB3MapGsbE* _M0L4selfS1202) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS1202->$1;
}

int32_t _M0MPB3Map6lengthGsfE(struct _M0TPB3MapGsfE* _M0L4selfS1203) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS1203->$1;
}

int32_t _M0MPB3Map6removeGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1193,
  moonbit_string_t _M0L3keyS1194
) {
  int32_t _M0L6_2atmpS3576;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3576 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1194);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsiE(_M0L4selfS1193, _M0L3keyS1194, _M0L6_2atmpS3576);
  return 0;
}

int32_t _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS1195,
  moonbit_string_t _M0L3keyS1196
) {
  int32_t _M0L6_2atmpS3577;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3577 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1196);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsRP19moonbitDB10RedisValueE(_M0L4selfS1195, _M0L3keyS1196, _M0L6_2atmpS3577);
  return 0;
}

int32_t _M0MPB3Map6removeGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1197,
  moonbit_string_t _M0L3keyS1198
) {
  int32_t _M0L6_2atmpS3578;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3578 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1198);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGssE(_M0L4selfS1197, _M0L3keyS1198, _M0L6_2atmpS3578);
  return 0;
}

int32_t _M0MPB3Map6removeGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1199,
  moonbit_string_t _M0L3keyS1200
) {
  int32_t _M0L6_2atmpS3579;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3579 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1200);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsbE(_M0L4selfS1199, _M0L3keyS1200, _M0L6_2atmpS3579);
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1160,
  moonbit_string_t _M0L3keyS1164,
  int32_t _M0L4hashS1163
) {
  int32_t _M0L14capacity__maskS3539;
  int32_t _M0L6_2atmpS3538;
  int32_t _M0L1iS1157;
  int32_t _M0L3idxS1158;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3539 = _M0L4selfS1160->$3;
  _M0L6_2atmpS3538 = _M0L4hashS1163 & _M0L14capacity__maskS3539;
  _M0L1iS1157 = 0;
  _M0L3idxS1158 = _M0L6_2atmpS3538;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3537 = _M0L4selfS1160->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS1159;
    if (
      _M0L3idxS1158 < 0
      || _M0L3idxS1158 >= Moonbit_array_length(_M0L7entriesS3537)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1159
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3537[_M0L3idxS1158];
    if (_M0L7_2abindS1159 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS1161 = _M0L7_2abindS1159;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS1162 = _M0L7_2aSomeS1161;
      int32_t _M0L4hashS3529 = _M0L8_2aentryS1162->$3;
      int32_t _if__result_5566;
      int32_t _M0L3pslS3532;
      int32_t _M0L6_2atmpS3533;
      int32_t _M0L6_2atmpS3535;
      int32_t _M0L14capacity__maskS3536;
      int32_t _M0L6_2atmpS3534;
      if (_M0L4hashS3529 == _M0L4hashS1163) {
        moonbit_string_t _M0L3keyS3528 = _M0L8_2aentryS1162->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5566
        = _M0L3keyS3528 == _M0L3keyS1164
          || Moonbit_array_length(_M0L3keyS3528)
             == Moonbit_array_length(_M0L3keyS1164)
             && 0
                == memcmp(_M0L3keyS3528, _M0L3keyS1164, Moonbit_array_length(_M0L3keyS3528) * 2);
      } else {
        _if__result_5566 = 0;
      }
      if (_if__result_5566) {
        int32_t _M0L4sizeS3531;
        int32_t _M0L6_2atmpS3530;
        moonbit_incref(_M0L8_2aentryS1162);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsiE(_M0L4selfS1160, _M0L8_2aentryS1162);
        moonbit_decref(_M0L8_2aentryS1162);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsiE(_M0L4selfS1160, _M0L3idxS1158);
        _M0L4sizeS3531 = _M0L4selfS1160->$1;
        _M0L6_2atmpS3530 = _M0L4sizeS3531 - 1;
        _M0L4selfS1160->$1 = _M0L6_2atmpS3530;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS1162);
      }
      _M0L3pslS3532 = _M0L8_2aentryS1162->$2;
      moonbit_decref(_M0L8_2aentryS1162);
      if (_M0L1iS1157 > _M0L3pslS3532) {
        break;
      }
      _M0L6_2atmpS3533 = _M0L1iS1157 + 1;
      _M0L6_2atmpS3535 = _M0L3idxS1158 + 1;
      _M0L14capacity__maskS3536 = _M0L4selfS1160->$3;
      _M0L6_2atmpS3534 = _M0L6_2atmpS3535 & _M0L14capacity__maskS3536;
      _M0L1iS1157 = _M0L6_2atmpS3533;
      _M0L3idxS1158 = _M0L6_2atmpS3534;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS1169,
  moonbit_string_t _M0L3keyS1173,
  int32_t _M0L4hashS1172
) {
  int32_t _M0L14capacity__maskS3551;
  int32_t _M0L6_2atmpS3550;
  int32_t _M0L1iS1166;
  int32_t _M0L3idxS1167;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3551 = _M0L4selfS1169->$3;
  _M0L6_2atmpS3550 = _M0L4hashS1172 & _M0L14capacity__maskS3551;
  _M0L1iS1166 = 0;
  _M0L3idxS1167 = _M0L6_2atmpS3550;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3549 =
      _M0L4selfS1169->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS1168;
    if (
      _M0L3idxS1167 < 0
      || _M0L3idxS1167 >= Moonbit_array_length(_M0L7entriesS3549)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1168
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3549[
        _M0L3idxS1167
      ];
    if (_M0L7_2abindS1168 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1170 =
        _M0L7_2abindS1168;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L8_2aentryS1171 =
        _M0L7_2aSomeS1170;
      int32_t _M0L4hashS3541 = _M0L8_2aentryS1171->$3;
      int32_t _if__result_5568;
      int32_t _M0L3pslS3544;
      int32_t _M0L6_2atmpS3545;
      int32_t _M0L6_2atmpS3547;
      int32_t _M0L14capacity__maskS3548;
      int32_t _M0L6_2atmpS3546;
      if (_M0L4hashS3541 == _M0L4hashS1172) {
        moonbit_string_t _M0L3keyS3540 = _M0L8_2aentryS1171->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5568
        = _M0L3keyS3540 == _M0L3keyS1173
          || Moonbit_array_length(_M0L3keyS3540)
             == Moonbit_array_length(_M0L3keyS1173)
             && 0
                == memcmp(_M0L3keyS3540, _M0L3keyS1173, Moonbit_array_length(_M0L3keyS3540) * 2);
      } else {
        _if__result_5568 = 0;
      }
      if (_if__result_5568) {
        int32_t _M0L4sizeS3543;
        int32_t _M0L6_2atmpS3542;
        moonbit_incref(_M0L8_2aentryS1171);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS1169, _M0L8_2aentryS1171);
        moonbit_decref(_M0L8_2aentryS1171);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsRP19moonbitDB10RedisValueE(_M0L4selfS1169, _M0L3idxS1167);
        _M0L4sizeS3543 = _M0L4selfS1169->$1;
        _M0L6_2atmpS3542 = _M0L4sizeS3543 - 1;
        _M0L4selfS1169->$1 = _M0L6_2atmpS3542;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS1171);
      }
      _M0L3pslS3544 = _M0L8_2aentryS1171->$2;
      moonbit_decref(_M0L8_2aentryS1171);
      if (_M0L1iS1166 > _M0L3pslS3544) {
        break;
      }
      _M0L6_2atmpS3545 = _M0L1iS1166 + 1;
      _M0L6_2atmpS3547 = _M0L3idxS1167 + 1;
      _M0L14capacity__maskS3548 = _M0L4selfS1169->$3;
      _M0L6_2atmpS3546 = _M0L6_2atmpS3547 & _M0L14capacity__maskS3548;
      _M0L1iS1166 = _M0L6_2atmpS3545;
      _M0L3idxS1167 = _M0L6_2atmpS3546;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1178,
  moonbit_string_t _M0L3keyS1182,
  int32_t _M0L4hashS1181
) {
  int32_t _M0L14capacity__maskS3563;
  int32_t _M0L6_2atmpS3562;
  int32_t _M0L1iS1175;
  int32_t _M0L3idxS1176;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3563 = _M0L4selfS1178->$3;
  _M0L6_2atmpS3562 = _M0L4hashS1181 & _M0L14capacity__maskS3563;
  _M0L1iS1175 = 0;
  _M0L3idxS1176 = _M0L6_2atmpS3562;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS3561 = _M0L4selfS1178->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS1177;
    if (
      _M0L3idxS1176 < 0
      || _M0L3idxS1176 >= Moonbit_array_length(_M0L7entriesS3561)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1177
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3561[_M0L3idxS1176];
    if (_M0L7_2abindS1177 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS1179 = _M0L7_2abindS1177;
      struct _M0TPB5EntryGssE* _M0L8_2aentryS1180 = _M0L7_2aSomeS1179;
      int32_t _M0L4hashS3553 = _M0L8_2aentryS1180->$3;
      int32_t _if__result_5570;
      int32_t _M0L3pslS3556;
      int32_t _M0L6_2atmpS3557;
      int32_t _M0L6_2atmpS3559;
      int32_t _M0L14capacity__maskS3560;
      int32_t _M0L6_2atmpS3558;
      if (_M0L4hashS3553 == _M0L4hashS1181) {
        moonbit_string_t _M0L3keyS3552 = _M0L8_2aentryS1180->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5570
        = _M0L3keyS3552 == _M0L3keyS1182
          || Moonbit_array_length(_M0L3keyS3552)
             == Moonbit_array_length(_M0L3keyS1182)
             && 0
                == memcmp(_M0L3keyS3552, _M0L3keyS1182, Moonbit_array_length(_M0L3keyS3552) * 2);
      } else {
        _if__result_5570 = 0;
      }
      if (_if__result_5570) {
        int32_t _M0L4sizeS3555;
        int32_t _M0L6_2atmpS3554;
        moonbit_incref(_M0L8_2aentryS1180);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGssE(_M0L4selfS1178, _M0L8_2aentryS1180);
        moonbit_decref(_M0L8_2aentryS1180);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGssE(_M0L4selfS1178, _M0L3idxS1176);
        _M0L4sizeS3555 = _M0L4selfS1178->$1;
        _M0L6_2atmpS3554 = _M0L4sizeS3555 - 1;
        _M0L4selfS1178->$1 = _M0L6_2atmpS3554;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS1180);
      }
      _M0L3pslS3556 = _M0L8_2aentryS1180->$2;
      moonbit_decref(_M0L8_2aentryS1180);
      if (_M0L1iS1175 > _M0L3pslS3556) {
        break;
      }
      _M0L6_2atmpS3557 = _M0L1iS1175 + 1;
      _M0L6_2atmpS3559 = _M0L3idxS1176 + 1;
      _M0L14capacity__maskS3560 = _M0L4selfS1178->$3;
      _M0L6_2atmpS3558 = _M0L6_2atmpS3559 & _M0L14capacity__maskS3560;
      _M0L1iS1175 = _M0L6_2atmpS3557;
      _M0L3idxS1176 = _M0L6_2atmpS3558;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1187,
  moonbit_string_t _M0L3keyS1191,
  int32_t _M0L4hashS1190
) {
  int32_t _M0L14capacity__maskS3575;
  int32_t _M0L6_2atmpS3574;
  int32_t _M0L1iS1184;
  int32_t _M0L3idxS1185;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3575 = _M0L4selfS1187->$3;
  _M0L6_2atmpS3574 = _M0L4hashS1190 & _M0L14capacity__maskS3575;
  _M0L1iS1184 = 0;
  _M0L3idxS1185 = _M0L6_2atmpS3574;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS3573 = _M0L4selfS1187->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS1186;
    if (
      _M0L3idxS1185 < 0
      || _M0L3idxS1185 >= Moonbit_array_length(_M0L7entriesS3573)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1186
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3573[_M0L3idxS1185];
    if (_M0L7_2abindS1186 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS1188 = _M0L7_2abindS1186;
      struct _M0TPB5EntryGsbE* _M0L8_2aentryS1189 = _M0L7_2aSomeS1188;
      int32_t _M0L4hashS3565 = _M0L8_2aentryS1189->$3;
      int32_t _if__result_5572;
      int32_t _M0L3pslS3568;
      int32_t _M0L6_2atmpS3569;
      int32_t _M0L6_2atmpS3571;
      int32_t _M0L14capacity__maskS3572;
      int32_t _M0L6_2atmpS3570;
      if (_M0L4hashS3565 == _M0L4hashS1190) {
        moonbit_string_t _M0L3keyS3564 = _M0L8_2aentryS1189->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5572
        = _M0L3keyS3564 == _M0L3keyS1191
          || Moonbit_array_length(_M0L3keyS3564)
             == Moonbit_array_length(_M0L3keyS1191)
             && 0
                == memcmp(_M0L3keyS3564, _M0L3keyS1191, Moonbit_array_length(_M0L3keyS3564) * 2);
      } else {
        _if__result_5572 = 0;
      }
      if (_if__result_5572) {
        int32_t _M0L4sizeS3567;
        int32_t _M0L6_2atmpS3566;
        moonbit_incref(_M0L8_2aentryS1189);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsbE(_M0L4selfS1187, _M0L8_2aentryS1189);
        moonbit_decref(_M0L8_2aentryS1189);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsbE(_M0L4selfS1187, _M0L3idxS1185);
        _M0L4sizeS3567 = _M0L4selfS1187->$1;
        _M0L6_2atmpS3566 = _M0L4sizeS3567 - 1;
        _M0L4selfS1187->$1 = _M0L6_2atmpS3566;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS1189);
      }
      _M0L3pslS3568 = _M0L8_2aentryS1189->$2;
      moonbit_decref(_M0L8_2aentryS1189);
      if (_M0L1iS1184 > _M0L3pslS3568) {
        break;
      }
      _M0L6_2atmpS3569 = _M0L1iS1184 + 1;
      _M0L6_2atmpS3571 = _M0L3idxS1185 + 1;
      _M0L14capacity__maskS3572 = _M0L4selfS1187->$3;
      _M0L6_2atmpS3570 = _M0L6_2atmpS3571 & _M0L14capacity__maskS3572;
      _M0L1iS1184 = _M0L6_2atmpS3569;
      _M0L3idxS1185 = _M0L6_2atmpS3570;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1119,
  int32_t _M0L3idxS1126
) {
  int32_t _M0L3curS1117;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS1117 = _M0L3idxS1126;
  _2afor_1121:;
  while (1) {
    int32_t _M0L6_2atmpS3505 = _M0L3curS1117 + 1;
    int32_t _M0L14capacity__maskS3506 = _M0L4selfS1119->$3;
    int32_t _M0L4nextS1118 = _M0L6_2atmpS3505 & _M0L14capacity__maskS3506;
    struct _M0TPB5EntryGsiE** _M0L7entriesS3504 = _M0L4selfS1119->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS1122;
    struct _M0TPB5EntryGsiE** _M0L7entriesS3500;
    struct _M0TPB5EntryGsiE* _M0L6_2atmpS3501;
    struct _M0TPB5EntryGsiE* _M0L6_2aoldS4870;
    int32_t _tmp_5575;
    if (
      _M0L4nextS1118 < 0
      || _M0L4nextS1118 >= Moonbit_array_length(_M0L7entriesS3504)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1122
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3504[_M0L4nextS1118];
    if (_M0L7_2abindS1122 == 0) {
      goto join_1120;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS1123 = _M0L7_2abindS1122;
      struct _M0TPB5EntryGsiE* _M0L4_2axS1124 = _M0L7_2aSomeS1123;
      int32_t _M0L4_2axS1125 = _M0L4_2axS1124->$2;
      switch (_M0L4_2axS1125) {
        case 0: {
          goto join_1120;
          break;
        }
        default: {
          int32_t _M0L3pslS3503 = _M0L4_2axS1124->$2;
          int32_t _M0L6_2atmpS3502 = _M0L3pslS3503 - 1;
          _M0L4_2axS1124->$2 = _M0L6_2atmpS3502;
          moonbit_incref(_M0L4_2axS1124);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsiE(_M0L4selfS1119, _M0L4_2axS1124, _M0L3curS1117);
          moonbit_decref(_M0L4_2axS1124);
          _M0L3curS1117 = _M0L4nextS1118;
          goto _2afor_1121;
          break;
        }
      }
    }
    goto joinlet_5574;
    join_1120:;
    _M0L7entriesS3500 = _M0L4selfS1119->$0;
    _M0L6_2atmpS3501 = 0;
    if (
      _M0L3curS1117 < 0
      || _M0L3curS1117 >= Moonbit_array_length(_M0L7entriesS3500)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4870
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3500[_M0L3curS1117];
    if (_M0L6_2aoldS4870) {
      moonbit_decref(_M0L6_2aoldS4870);
    }
    _M0L7entriesS3500[_M0L3curS1117] = _M0L6_2atmpS3501;
    break;
    joinlet_5574:;
    _tmp_5575 = _M0L3curS1117;
    _M0L3curS1117 = _tmp_5575;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS1129,
  int32_t _M0L3idxS1136
) {
  int32_t _M0L3curS1127;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS1127 = _M0L3idxS1136;
  _2afor_1131:;
  while (1) {
    int32_t _M0L6_2atmpS3512 = _M0L3curS1127 + 1;
    int32_t _M0L14capacity__maskS3513 = _M0L4selfS1129->$3;
    int32_t _M0L4nextS1128 = _M0L6_2atmpS3512 & _M0L14capacity__maskS3513;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3511 =
      _M0L4selfS1129->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS1132;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3507;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3508;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4874;
    int32_t _tmp_5578;
    if (
      _M0L4nextS1128 < 0
      || _M0L4nextS1128 >= Moonbit_array_length(_M0L7entriesS3511)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1132
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3511[
        _M0L4nextS1128
      ];
    if (_M0L7_2abindS1132 == 0) {
      goto join_1130;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1133 =
        _M0L7_2abindS1132;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4_2axS1134 =
        _M0L7_2aSomeS1133;
      int32_t _M0L4_2axS1135 = _M0L4_2axS1134->$2;
      switch (_M0L4_2axS1135) {
        case 0: {
          goto join_1130;
          break;
        }
        default: {
          int32_t _M0L3pslS3510 = _M0L4_2axS1134->$2;
          int32_t _M0L6_2atmpS3509 = _M0L3pslS3510 - 1;
          _M0L4_2axS1134->$2 = _M0L6_2atmpS3509;
          moonbit_incref(_M0L4_2axS1134);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS1129, _M0L4_2axS1134, _M0L3curS1127);
          moonbit_decref(_M0L4_2axS1134);
          _M0L3curS1127 = _M0L4nextS1128;
          goto _2afor_1131;
          break;
        }
      }
    }
    goto joinlet_5577;
    join_1130:;
    _M0L7entriesS3507 = _M0L4selfS1129->$0;
    _M0L6_2atmpS3508 = 0;
    if (
      _M0L3curS1127 < 0
      || _M0L3curS1127 >= Moonbit_array_length(_M0L7entriesS3507)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4874
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3507[
        _M0L3curS1127
      ];
    if (_M0L6_2aoldS4874) {
      moonbit_decref(_M0L6_2aoldS4874);
    }
    _M0L7entriesS3507[_M0L3curS1127] = _M0L6_2atmpS3508;
    break;
    joinlet_5577:;
    _tmp_5578 = _M0L3curS1127;
    _M0L3curS1127 = _tmp_5578;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1139,
  int32_t _M0L3idxS1146
) {
  int32_t _M0L3curS1137;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS1137 = _M0L3idxS1146;
  _2afor_1141:;
  while (1) {
    int32_t _M0L6_2atmpS3519 = _M0L3curS1137 + 1;
    int32_t _M0L14capacity__maskS3520 = _M0L4selfS1139->$3;
    int32_t _M0L4nextS1138 = _M0L6_2atmpS3519 & _M0L14capacity__maskS3520;
    struct _M0TPB5EntryGssE** _M0L7entriesS3518 = _M0L4selfS1139->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS1142;
    struct _M0TPB5EntryGssE** _M0L7entriesS3514;
    struct _M0TPB5EntryGssE* _M0L6_2atmpS3515;
    struct _M0TPB5EntryGssE* _M0L6_2aoldS4878;
    int32_t _tmp_5581;
    if (
      _M0L4nextS1138 < 0
      || _M0L4nextS1138 >= Moonbit_array_length(_M0L7entriesS3518)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1142
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3518[_M0L4nextS1138];
    if (_M0L7_2abindS1142 == 0) {
      goto join_1140;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS1143 = _M0L7_2abindS1142;
      struct _M0TPB5EntryGssE* _M0L4_2axS1144 = _M0L7_2aSomeS1143;
      int32_t _M0L4_2axS1145 = _M0L4_2axS1144->$2;
      switch (_M0L4_2axS1145) {
        case 0: {
          goto join_1140;
          break;
        }
        default: {
          int32_t _M0L3pslS3517 = _M0L4_2axS1144->$2;
          int32_t _M0L6_2atmpS3516 = _M0L3pslS3517 - 1;
          _M0L4_2axS1144->$2 = _M0L6_2atmpS3516;
          moonbit_incref(_M0L4_2axS1144);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGssE(_M0L4selfS1139, _M0L4_2axS1144, _M0L3curS1137);
          moonbit_decref(_M0L4_2axS1144);
          _M0L3curS1137 = _M0L4nextS1138;
          goto _2afor_1141;
          break;
        }
      }
    }
    goto joinlet_5580;
    join_1140:;
    _M0L7entriesS3514 = _M0L4selfS1139->$0;
    _M0L6_2atmpS3515 = 0;
    if (
      _M0L3curS1137 < 0
      || _M0L3curS1137 >= Moonbit_array_length(_M0L7entriesS3514)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4878
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3514[_M0L3curS1137];
    if (_M0L6_2aoldS4878) {
      moonbit_decref(_M0L6_2aoldS4878);
    }
    _M0L7entriesS3514[_M0L3curS1137] = _M0L6_2atmpS3515;
    break;
    joinlet_5580:;
    _tmp_5581 = _M0L3curS1137;
    _M0L3curS1137 = _tmp_5581;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1149,
  int32_t _M0L3idxS1156
) {
  int32_t _M0L3curS1147;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS1147 = _M0L3idxS1156;
  _2afor_1151:;
  while (1) {
    int32_t _M0L6_2atmpS3526 = _M0L3curS1147 + 1;
    int32_t _M0L14capacity__maskS3527 = _M0L4selfS1149->$3;
    int32_t _M0L4nextS1148 = _M0L6_2atmpS3526 & _M0L14capacity__maskS3527;
    struct _M0TPB5EntryGsbE** _M0L7entriesS3525 = _M0L4selfS1149->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS1152;
    struct _M0TPB5EntryGsbE** _M0L7entriesS3521;
    struct _M0TPB5EntryGsbE* _M0L6_2atmpS3522;
    struct _M0TPB5EntryGsbE* _M0L6_2aoldS4882;
    int32_t _tmp_5584;
    if (
      _M0L4nextS1148 < 0
      || _M0L4nextS1148 >= Moonbit_array_length(_M0L7entriesS3525)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1152
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3525[_M0L4nextS1148];
    if (_M0L7_2abindS1152 == 0) {
      goto join_1150;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS1153 = _M0L7_2abindS1152;
      struct _M0TPB5EntryGsbE* _M0L4_2axS1154 = _M0L7_2aSomeS1153;
      int32_t _M0L4_2axS1155 = _M0L4_2axS1154->$2;
      switch (_M0L4_2axS1155) {
        case 0: {
          goto join_1150;
          break;
        }
        default: {
          int32_t _M0L3pslS3524 = _M0L4_2axS1154->$2;
          int32_t _M0L6_2atmpS3523 = _M0L3pslS3524 - 1;
          _M0L4_2axS1154->$2 = _M0L6_2atmpS3523;
          moonbit_incref(_M0L4_2axS1154);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsbE(_M0L4selfS1149, _M0L4_2axS1154, _M0L3curS1147);
          moonbit_decref(_M0L4_2axS1154);
          _M0L3curS1147 = _M0L4nextS1148;
          goto _2afor_1151;
          break;
        }
      }
    }
    goto joinlet_5583;
    join_1150:;
    _M0L7entriesS3521 = _M0L4selfS1149->$0;
    _M0L6_2atmpS3522 = 0;
    if (
      _M0L3curS1147 < 0
      || _M0L3curS1147 >= Moonbit_array_length(_M0L7entriesS3521)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4882
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3521[_M0L3curS1147];
    if (_M0L6_2aoldS4882) {
      moonbit_decref(_M0L6_2aoldS4882);
    }
    _M0L7entriesS3521[_M0L3curS1147] = _M0L6_2atmpS3522;
    break;
    joinlet_5583:;
    _tmp_5584 = _M0L3curS1147;
    _M0L3curS1147 = _tmp_5584;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1095,
  struct _M0TPB5EntryGsiE* _M0L5entryS1094
) {
  int32_t _M0L7_2abindS1093;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS1096;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS1093 = _M0L5entryS1094->$0;
  switch (_M0L7_2abindS1093) {
    case -1: {
      struct _M0TPB5EntryGsiE* _M0L4nextS3472 = _M0L5entryS1094->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4887 = _M0L4selfS1095->$5;
      if (_M0L4nextS3472) {
        moonbit_incref(_M0L4nextS3472);
      }
      if (_M0L6_2aoldS4887) {
        moonbit_decref(_M0L6_2aoldS4887);
      }
      _M0L4selfS1095->$5 = _M0L4nextS3472;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS3476 = _M0L4selfS1095->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS3475;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS3473;
      struct _M0TPB5EntryGsiE* _M0L4nextS3474;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4889;
      if (
        _M0L7_2abindS1093 < 0
        || _M0L7_2abindS1093 >= Moonbit_array_length(_M0L7entriesS3476)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3475
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3476[_M0L7_2abindS1093];
      if (_M0L6_2atmpS3475) {
        moonbit_incref(_M0L6_2atmpS3475);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3473
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS3475);
      if (_M0L6_2atmpS3475) {
        moonbit_decref(_M0L6_2atmpS3475);
      }
      _M0L4nextS3474 = _M0L5entryS1094->$1;
      _M0L6_2aoldS4889 = _M0L6_2atmpS3473->$1;
      if (_M0L4nextS3474) {
        moonbit_incref(_M0L4nextS3474);
      }
      if (_M0L6_2aoldS4889) {
        moonbit_decref(_M0L6_2aoldS4889);
      }
      _M0L6_2atmpS3473->$1 = _M0L4nextS3474;
      moonbit_decref(_M0L6_2atmpS3473);
      break;
    }
  }
  _M0L7_2abindS1096 = _M0L5entryS1094->$1;
  if (_M0L7_2abindS1096 == 0) {
    int32_t _M0L4prevS3477 = _M0L5entryS1094->$0;
    _M0L4selfS1095->$6 = _M0L4prevS3477;
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS1097 = _M0L7_2abindS1096;
    struct _M0TPB5EntryGsiE* _M0L7_2anextS1098 = _M0L7_2aSomeS1097;
    int32_t _M0L4prevS3478 = _M0L5entryS1094->$0;
    _M0L7_2anextS1098->$0 = _M0L4prevS3478;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS1101,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS1100
) {
  int32_t _M0L7_2abindS1099;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS1102;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS1099 = _M0L5entryS1100->$0;
  switch (_M0L7_2abindS1099) {
    case -1: {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4nextS3479 =
        _M0L5entryS1100->$1;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4894 =
        _M0L4selfS1101->$5;
      if (_M0L4nextS3479) {
        moonbit_incref(_M0L4nextS3479);
      }
      if (_M0L6_2aoldS4894) {
        moonbit_decref(_M0L6_2aoldS4894);
      }
      _M0L4selfS1101->$5 = _M0L4nextS3479;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3483 =
        _M0L4selfS1101->$0;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3482;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3480;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4nextS3481;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4896;
      if (
        _M0L7_2abindS1099 < 0
        || _M0L7_2abindS1099 >= Moonbit_array_length(_M0L7entriesS3483)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3482
      = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3483[
          _M0L7_2abindS1099
        ];
      if (_M0L6_2atmpS3482) {
        moonbit_incref(_M0L6_2atmpS3482);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3480
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP19moonbitDB10RedisValueEE(_M0L6_2atmpS3482);
      if (_M0L6_2atmpS3482) {
        moonbit_decref(_M0L6_2atmpS3482);
      }
      _M0L4nextS3481 = _M0L5entryS1100->$1;
      _M0L6_2aoldS4896 = _M0L6_2atmpS3480->$1;
      if (_M0L4nextS3481) {
        moonbit_incref(_M0L4nextS3481);
      }
      if (_M0L6_2aoldS4896) {
        moonbit_decref(_M0L6_2aoldS4896);
      }
      _M0L6_2atmpS3480->$1 = _M0L4nextS3481;
      moonbit_decref(_M0L6_2atmpS3480);
      break;
    }
  }
  _M0L7_2abindS1102 = _M0L5entryS1100->$1;
  if (_M0L7_2abindS1102 == 0) {
    int32_t _M0L4prevS3484 = _M0L5entryS1100->$0;
    _M0L4selfS1101->$6 = _M0L4prevS3484;
  } else {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1103 =
      _M0L7_2abindS1102;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2anextS1104 =
      _M0L7_2aSomeS1103;
    int32_t _M0L4prevS3485 = _M0L5entryS1100->$0;
    _M0L7_2anextS1104->$0 = _M0L4prevS3485;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1107,
  struct _M0TPB5EntryGssE* _M0L5entryS1106
) {
  int32_t _M0L7_2abindS1105;
  struct _M0TPB5EntryGssE* _M0L7_2abindS1108;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS1105 = _M0L5entryS1106->$0;
  switch (_M0L7_2abindS1105) {
    case -1: {
      struct _M0TPB5EntryGssE* _M0L4nextS3486 = _M0L5entryS1106->$1;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4901 = _M0L4selfS1107->$5;
      if (_M0L4nextS3486) {
        moonbit_incref(_M0L4nextS3486);
      }
      if (_M0L6_2aoldS4901) {
        moonbit_decref(_M0L6_2aoldS4901);
      }
      _M0L4selfS1107->$5 = _M0L4nextS3486;
      break;
    }
    default: {
      struct _M0TPB5EntryGssE** _M0L7entriesS3490 = _M0L4selfS1107->$0;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS3489;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS3487;
      struct _M0TPB5EntryGssE* _M0L4nextS3488;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4903;
      if (
        _M0L7_2abindS1105 < 0
        || _M0L7_2abindS1105 >= Moonbit_array_length(_M0L7entriesS3490)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3489
      = (struct _M0TPB5EntryGssE*)_M0L7entriesS3490[_M0L7_2abindS1105];
      if (_M0L6_2atmpS3489) {
        moonbit_incref(_M0L6_2atmpS3489);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3487
      = _M0MPC16option6Option6unwrapGRPB5EntryGssEE(_M0L6_2atmpS3489);
      if (_M0L6_2atmpS3489) {
        moonbit_decref(_M0L6_2atmpS3489);
      }
      _M0L4nextS3488 = _M0L5entryS1106->$1;
      _M0L6_2aoldS4903 = _M0L6_2atmpS3487->$1;
      if (_M0L4nextS3488) {
        moonbit_incref(_M0L4nextS3488);
      }
      if (_M0L6_2aoldS4903) {
        moonbit_decref(_M0L6_2aoldS4903);
      }
      _M0L6_2atmpS3487->$1 = _M0L4nextS3488;
      moonbit_decref(_M0L6_2atmpS3487);
      break;
    }
  }
  _M0L7_2abindS1108 = _M0L5entryS1106->$1;
  if (_M0L7_2abindS1108 == 0) {
    int32_t _M0L4prevS3491 = _M0L5entryS1106->$0;
    _M0L4selfS1107->$6 = _M0L4prevS3491;
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS1109 = _M0L7_2abindS1108;
    struct _M0TPB5EntryGssE* _M0L7_2anextS1110 = _M0L7_2aSomeS1109;
    int32_t _M0L4prevS3492 = _M0L5entryS1106->$0;
    _M0L7_2anextS1110->$0 = _M0L4prevS3492;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1113,
  struct _M0TPB5EntryGsbE* _M0L5entryS1112
) {
  int32_t _M0L7_2abindS1111;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS1114;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS1111 = _M0L5entryS1112->$0;
  switch (_M0L7_2abindS1111) {
    case -1: {
      struct _M0TPB5EntryGsbE* _M0L4nextS3493 = _M0L5entryS1112->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4908 = _M0L4selfS1113->$5;
      if (_M0L4nextS3493) {
        moonbit_incref(_M0L4nextS3493);
      }
      if (_M0L6_2aoldS4908) {
        moonbit_decref(_M0L6_2aoldS4908);
      }
      _M0L4selfS1113->$5 = _M0L4nextS3493;
      break;
    }
    default: {
      struct _M0TPB5EntryGsbE** _M0L7entriesS3497 = _M0L4selfS1113->$0;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS3496;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS3494;
      struct _M0TPB5EntryGsbE* _M0L4nextS3495;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4910;
      if (
        _M0L7_2abindS1111 < 0
        || _M0L7_2abindS1111 >= Moonbit_array_length(_M0L7entriesS3497)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3496
      = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3497[_M0L7_2abindS1111];
      if (_M0L6_2atmpS3496) {
        moonbit_incref(_M0L6_2atmpS3496);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3494
      = _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(_M0L6_2atmpS3496);
      if (_M0L6_2atmpS3496) {
        moonbit_decref(_M0L6_2atmpS3496);
      }
      _M0L4nextS3495 = _M0L5entryS1112->$1;
      _M0L6_2aoldS4910 = _M0L6_2atmpS3494->$1;
      if (_M0L4nextS3495) {
        moonbit_incref(_M0L4nextS3495);
      }
      if (_M0L6_2aoldS4910) {
        moonbit_decref(_M0L6_2aoldS4910);
      }
      _M0L6_2atmpS3494->$1 = _M0L4nextS3495;
      moonbit_decref(_M0L6_2atmpS3494);
      break;
    }
  }
  _M0L7_2abindS1114 = _M0L5entryS1112->$1;
  if (_M0L7_2abindS1114 == 0) {
    int32_t _M0L4prevS3498 = _M0L5entryS1112->$0;
    _M0L4selfS1113->$6 = _M0L4prevS3498;
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS1115 = _M0L7_2abindS1114;
    struct _M0TPB5EntryGsbE* _M0L7_2anextS1116 = _M0L7_2aSomeS1115;
    int32_t _M0L4prevS3499 = _M0L5entryS1112->$0;
    _M0L7_2anextS1116->$0 = _M0L4prevS3499;
  }
  return 0;
}

int32_t _M0MPB3Map8containsGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1053,
  moonbit_string_t _M0L3keyS1049
) {
  int32_t _M0L4hashS1048;
  int32_t _M0L14capacity__maskS3431;
  int32_t _M0L6_2atmpS3430;
  int32_t _M0L1iS1050;
  int32_t _M0L3idxS1051;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1048 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1049);
  _M0L14capacity__maskS3431 = _M0L4selfS1053->$3;
  _M0L6_2atmpS3430 = _M0L4hashS1048 & _M0L14capacity__maskS3431;
  _M0L1iS1050 = 0;
  _M0L3idxS1051 = _M0L6_2atmpS3430;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS3429 = _M0L4selfS1053->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS1052;
    if (
      _M0L3idxS1051 < 0
      || _M0L3idxS1051 >= Moonbit_array_length(_M0L7entriesS3429)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1052
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3429[_M0L3idxS1051];
    if (_M0L7_2abindS1052 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS1054 = _M0L7_2abindS1052;
      struct _M0TPB5EntryGsbE* _M0L8_2aentryS1055 = _M0L7_2aSomeS1054;
      int32_t _M0L4hashS3423 = _M0L8_2aentryS1055->$3;
      int32_t _if__result_5586;
      int32_t _M0L3pslS3424;
      int32_t _M0L6_2atmpS3425;
      int32_t _M0L6_2atmpS3427;
      int32_t _M0L14capacity__maskS3428;
      int32_t _M0L6_2atmpS3426;
      if (_M0L4hashS3423 == _M0L4hashS1048) {
        moonbit_string_t _M0L3keyS3422 = _M0L8_2aentryS1055->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5586
        = _M0L3keyS3422 == _M0L3keyS1049
          || Moonbit_array_length(_M0L3keyS3422)
             == Moonbit_array_length(_M0L3keyS1049)
             && 0
                == memcmp(_M0L3keyS3422, _M0L3keyS1049, Moonbit_array_length(_M0L3keyS3422) * 2);
      } else {
        _if__result_5586 = 0;
      }
      if (_if__result_5586) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS1055);
      }
      _M0L3pslS3424 = _M0L8_2aentryS1055->$2;
      moonbit_decref(_M0L8_2aentryS1055);
      if (_M0L1iS1050 > _M0L3pslS3424) {
        return 0;
      }
      _M0L6_2atmpS3425 = _M0L1iS1050 + 1;
      _M0L6_2atmpS3427 = _M0L3idxS1051 + 1;
      _M0L14capacity__maskS3428 = _M0L4selfS1053->$3;
      _M0L6_2atmpS3426 = _M0L6_2atmpS3427 & _M0L14capacity__maskS3428;
      _M0L1iS1050 = _M0L6_2atmpS3425;
      _M0L3idxS1051 = _M0L6_2atmpS3426;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1062,
  moonbit_string_t _M0L3keyS1058
) {
  int32_t _M0L4hashS1057;
  int32_t _M0L14capacity__maskS3441;
  int32_t _M0L6_2atmpS3440;
  int32_t _M0L1iS1059;
  int32_t _M0L3idxS1060;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1057 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1058);
  _M0L14capacity__maskS3441 = _M0L4selfS1062->$3;
  _M0L6_2atmpS3440 = _M0L4hashS1057 & _M0L14capacity__maskS3441;
  _M0L1iS1059 = 0;
  _M0L3idxS1060 = _M0L6_2atmpS3440;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS3439 = _M0L4selfS1062->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS1061;
    if (
      _M0L3idxS1060 < 0
      || _M0L3idxS1060 >= Moonbit_array_length(_M0L7entriesS3439)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1061
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3439[_M0L3idxS1060];
    if (_M0L7_2abindS1061 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS1063 = _M0L7_2abindS1061;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS1064 = _M0L7_2aSomeS1063;
      int32_t _M0L4hashS3433 = _M0L8_2aentryS1064->$3;
      int32_t _if__result_5588;
      int32_t _M0L3pslS3434;
      int32_t _M0L6_2atmpS3435;
      int32_t _M0L6_2atmpS3437;
      int32_t _M0L14capacity__maskS3438;
      int32_t _M0L6_2atmpS3436;
      if (_M0L4hashS3433 == _M0L4hashS1057) {
        moonbit_string_t _M0L3keyS3432 = _M0L8_2aentryS1064->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5588
        = _M0L3keyS3432 == _M0L3keyS1058
          || Moonbit_array_length(_M0L3keyS3432)
             == Moonbit_array_length(_M0L3keyS1058)
             && 0
                == memcmp(_M0L3keyS3432, _M0L3keyS1058, Moonbit_array_length(_M0L3keyS3432) * 2);
      } else {
        _if__result_5588 = 0;
      }
      if (_if__result_5588) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS1064);
      }
      _M0L3pslS3434 = _M0L8_2aentryS1064->$2;
      moonbit_decref(_M0L8_2aentryS1064);
      if (_M0L1iS1059 > _M0L3pslS3434) {
        return 0;
      }
      _M0L6_2atmpS3435 = _M0L1iS1059 + 1;
      _M0L6_2atmpS3437 = _M0L3idxS1060 + 1;
      _M0L14capacity__maskS3438 = _M0L4selfS1062->$3;
      _M0L6_2atmpS3436 = _M0L6_2atmpS3437 & _M0L14capacity__maskS3438;
      _M0L1iS1059 = _M0L6_2atmpS3435;
      _M0L3idxS1060 = _M0L6_2atmpS3436;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS1071,
  moonbit_string_t _M0L3keyS1067
) {
  int32_t _M0L4hashS1066;
  int32_t _M0L14capacity__maskS3451;
  int32_t _M0L6_2atmpS3450;
  int32_t _M0L1iS1068;
  int32_t _M0L3idxS1069;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1066 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1067);
  _M0L14capacity__maskS3451 = _M0L4selfS1071->$3;
  _M0L6_2atmpS3450 = _M0L4hashS1066 & _M0L14capacity__maskS3451;
  _M0L1iS1068 = 0;
  _M0L3idxS1069 = _M0L6_2atmpS3450;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3449 =
      _M0L4selfS1071->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS1070;
    if (
      _M0L3idxS1069 < 0
      || _M0L3idxS1069 >= Moonbit_array_length(_M0L7entriesS3449)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1070
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3449[
        _M0L3idxS1069
      ];
    if (_M0L7_2abindS1070 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1072 =
        _M0L7_2abindS1070;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L8_2aentryS1073 =
        _M0L7_2aSomeS1072;
      int32_t _M0L4hashS3443 = _M0L8_2aentryS1073->$3;
      int32_t _if__result_5590;
      int32_t _M0L3pslS3444;
      int32_t _M0L6_2atmpS3445;
      int32_t _M0L6_2atmpS3447;
      int32_t _M0L14capacity__maskS3448;
      int32_t _M0L6_2atmpS3446;
      if (_M0L4hashS3443 == _M0L4hashS1066) {
        moonbit_string_t _M0L3keyS3442 = _M0L8_2aentryS1073->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5590
        = _M0L3keyS3442 == _M0L3keyS1067
          || Moonbit_array_length(_M0L3keyS3442)
             == Moonbit_array_length(_M0L3keyS1067)
             && 0
                == memcmp(_M0L3keyS3442, _M0L3keyS1067, Moonbit_array_length(_M0L3keyS3442) * 2);
      } else {
        _if__result_5590 = 0;
      }
      if (_if__result_5590) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS1073);
      }
      _M0L3pslS3444 = _M0L8_2aentryS1073->$2;
      moonbit_decref(_M0L8_2aentryS1073);
      if (_M0L1iS1068 > _M0L3pslS3444) {
        return 0;
      }
      _M0L6_2atmpS3445 = _M0L1iS1068 + 1;
      _M0L6_2atmpS3447 = _M0L3idxS1069 + 1;
      _M0L14capacity__maskS3448 = _M0L4selfS1071->$3;
      _M0L6_2atmpS3446 = _M0L6_2atmpS3447 & _M0L14capacity__maskS3448;
      _M0L1iS1068 = _M0L6_2atmpS3445;
      _M0L3idxS1069 = _M0L6_2atmpS3446;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1080,
  moonbit_string_t _M0L3keyS1076
) {
  int32_t _M0L4hashS1075;
  int32_t _M0L14capacity__maskS3461;
  int32_t _M0L6_2atmpS3460;
  int32_t _M0L1iS1077;
  int32_t _M0L3idxS1078;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1075 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1076);
  _M0L14capacity__maskS3461 = _M0L4selfS1080->$3;
  _M0L6_2atmpS3460 = _M0L4hashS1075 & _M0L14capacity__maskS3461;
  _M0L1iS1077 = 0;
  _M0L3idxS1078 = _M0L6_2atmpS3460;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3459 = _M0L4selfS1080->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS1079;
    if (
      _M0L3idxS1078 < 0
      || _M0L3idxS1078 >= Moonbit_array_length(_M0L7entriesS3459)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1079
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3459[_M0L3idxS1078];
    if (_M0L7_2abindS1079 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS1081 = _M0L7_2abindS1079;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS1082 = _M0L7_2aSomeS1081;
      int32_t _M0L4hashS3453 = _M0L8_2aentryS1082->$3;
      int32_t _if__result_5592;
      int32_t _M0L3pslS3454;
      int32_t _M0L6_2atmpS3455;
      int32_t _M0L6_2atmpS3457;
      int32_t _M0L14capacity__maskS3458;
      int32_t _M0L6_2atmpS3456;
      if (_M0L4hashS3453 == _M0L4hashS1075) {
        moonbit_string_t _M0L3keyS3452 = _M0L8_2aentryS1082->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5592
        = _M0L3keyS3452 == _M0L3keyS1076
          || Moonbit_array_length(_M0L3keyS3452)
             == Moonbit_array_length(_M0L3keyS1076)
             && 0
                == memcmp(_M0L3keyS3452, _M0L3keyS1076, Moonbit_array_length(_M0L3keyS3452) * 2);
      } else {
        _if__result_5592 = 0;
      }
      if (_if__result_5592) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS1082);
      }
      _M0L3pslS3454 = _M0L8_2aentryS1082->$2;
      moonbit_decref(_M0L8_2aentryS1082);
      if (_M0L1iS1077 > _M0L3pslS3454) {
        return 0;
      }
      _M0L6_2atmpS3455 = _M0L1iS1077 + 1;
      _M0L6_2atmpS3457 = _M0L3idxS1078 + 1;
      _M0L14capacity__maskS3458 = _M0L4selfS1080->$3;
      _M0L6_2atmpS3456 = _M0L6_2atmpS3457 & _M0L14capacity__maskS3458;
      _M0L1iS1077 = _M0L6_2atmpS3455;
      _M0L3idxS1078 = _M0L6_2atmpS3456;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1089,
  moonbit_string_t _M0L3keyS1085
) {
  int32_t _M0L4hashS1084;
  int32_t _M0L14capacity__maskS3471;
  int32_t _M0L6_2atmpS3470;
  int32_t _M0L1iS1086;
  int32_t _M0L3idxS1087;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1084 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1085);
  _M0L14capacity__maskS3471 = _M0L4selfS1089->$3;
  _M0L6_2atmpS3470 = _M0L4hashS1084 & _M0L14capacity__maskS3471;
  _M0L1iS1086 = 0;
  _M0L3idxS1087 = _M0L6_2atmpS3470;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS3469 = _M0L4selfS1089->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS1088;
    if (
      _M0L3idxS1087 < 0
      || _M0L3idxS1087 >= Moonbit_array_length(_M0L7entriesS3469)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1088
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3469[_M0L3idxS1087];
    if (_M0L7_2abindS1088 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS1090 = _M0L7_2abindS1088;
      struct _M0TPB5EntryGssE* _M0L8_2aentryS1091 = _M0L7_2aSomeS1090;
      int32_t _M0L4hashS3463 = _M0L8_2aentryS1091->$3;
      int32_t _if__result_5594;
      int32_t _M0L3pslS3464;
      int32_t _M0L6_2atmpS3465;
      int32_t _M0L6_2atmpS3467;
      int32_t _M0L14capacity__maskS3468;
      int32_t _M0L6_2atmpS3466;
      if (_M0L4hashS3463 == _M0L4hashS1084) {
        moonbit_string_t _M0L3keyS3462 = _M0L8_2aentryS1091->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5594
        = _M0L3keyS3462 == _M0L3keyS1085
          || Moonbit_array_length(_M0L3keyS3462)
             == Moonbit_array_length(_M0L3keyS1085)
             && 0
                == memcmp(_M0L3keyS3462, _M0L3keyS1085, Moonbit_array_length(_M0L3keyS3462) * 2);
      } else {
        _if__result_5594 = 0;
      }
      if (_if__result_5594) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS1091);
      }
      _M0L3pslS3464 = _M0L8_2aentryS1091->$2;
      moonbit_decref(_M0L8_2aentryS1091);
      if (_M0L1iS1086 > _M0L3pslS3464) {
        return 0;
      }
      _M0L6_2atmpS3465 = _M0L1iS1086 + 1;
      _M0L6_2atmpS3467 = _M0L3idxS1087 + 1;
      _M0L14capacity__maskS3468 = _M0L4selfS1089->$3;
      _M0L6_2atmpS3466 = _M0L6_2atmpS3467 & _M0L14capacity__maskS3468;
      _M0L1iS1086 = _M0L6_2atmpS3465;
      _M0L3idxS1087 = _M0L6_2atmpS3466;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS1017,
  moonbit_string_t _M0L3keyS1013
) {
  int32_t _M0L4hashS1012;
  int32_t _M0L14capacity__maskS3381;
  int32_t _M0L6_2atmpS3380;
  int32_t _M0L1iS1014;
  int32_t _M0L3idxS1015;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1012 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1013);
  _M0L14capacity__maskS3381 = _M0L4selfS1017->$3;
  _M0L6_2atmpS3380 = _M0L4hashS1012 & _M0L14capacity__maskS3381;
  _M0L1iS1014 = 0;
  _M0L3idxS1015 = _M0L6_2atmpS3380;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3379 =
      _M0L4selfS1017->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS1016;
    if (
      _M0L3idxS1015 < 0
      || _M0L3idxS1015 >= Moonbit_array_length(_M0L7entriesS3379)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1016
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3379[
        _M0L3idxS1015
      ];
    if (_M0L7_2abindS1016 == 0) {
      void* _M0L6_2atmpS3368 = 0;
      return _M0L6_2atmpS3368;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1018 =
        _M0L7_2abindS1016;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L8_2aentryS1019 =
        _M0L7_2aSomeS1018;
      int32_t _M0L4hashS3370 = _M0L8_2aentryS1019->$3;
      int32_t _if__result_5596;
      int32_t _M0L3pslS3373;
      int32_t _M0L6_2atmpS3375;
      int32_t _M0L6_2atmpS3377;
      int32_t _M0L14capacity__maskS3378;
      int32_t _M0L6_2atmpS3376;
      if (_M0L4hashS3370 == _M0L4hashS1012) {
        moonbit_string_t _M0L3keyS3369 = _M0L8_2aentryS1019->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5596
        = _M0L3keyS3369 == _M0L3keyS1013
          || Moonbit_array_length(_M0L3keyS3369)
             == Moonbit_array_length(_M0L3keyS1013)
             && 0
                == memcmp(_M0L3keyS3369, _M0L3keyS1013, Moonbit_array_length(_M0L3keyS3369) * 2);
      } else {
        _if__result_5596 = 0;
      }
      if (_if__result_5596) {
        void* _M0L5valueS3372 = _M0L8_2aentryS1019->$5;
        void* _M0L6_2atmpS3371;
        moonbit_incref(_M0L5valueS3372);
        _M0L6_2atmpS3371 = _M0L5valueS3372;
        return _M0L6_2atmpS3371;
      } else {
        moonbit_incref(_M0L8_2aentryS1019);
      }
      _M0L3pslS3373 = _M0L8_2aentryS1019->$2;
      moonbit_decref(_M0L8_2aentryS1019);
      if (_M0L1iS1014 > _M0L3pslS3373) {
        void* _M0L6_2atmpS3374 = 0;
        return _M0L6_2atmpS3374;
      }
      _M0L6_2atmpS3375 = _M0L1iS1014 + 1;
      _M0L6_2atmpS3377 = _M0L3idxS1015 + 1;
      _M0L14capacity__maskS3378 = _M0L4selfS1017->$3;
      _M0L6_2atmpS3376 = _M0L6_2atmpS3377 & _M0L14capacity__maskS3378;
      _M0L1iS1014 = _M0L6_2atmpS3375;
      _M0L3idxS1015 = _M0L6_2atmpS3376;
      continue;
    }
    break;
  }
}

moonbit_string_t _M0MPB3Map3getGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1026,
  moonbit_string_t _M0L3keyS1022
) {
  int32_t _M0L4hashS1021;
  int32_t _M0L14capacity__maskS3395;
  int32_t _M0L6_2atmpS3394;
  int32_t _M0L1iS1023;
  int32_t _M0L3idxS1024;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1021 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1022);
  _M0L14capacity__maskS3395 = _M0L4selfS1026->$3;
  _M0L6_2atmpS3394 = _M0L4hashS1021 & _M0L14capacity__maskS3395;
  _M0L1iS1023 = 0;
  _M0L3idxS1024 = _M0L6_2atmpS3394;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS3393 = _M0L4selfS1026->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS1025;
    if (
      _M0L3idxS1024 < 0
      || _M0L3idxS1024 >= Moonbit_array_length(_M0L7entriesS3393)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1025
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3393[_M0L3idxS1024];
    if (_M0L7_2abindS1025 == 0) {
      moonbit_string_t _M0L6_2atmpS3382 = 0;
      return _M0L6_2atmpS3382;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS1027 = _M0L7_2abindS1025;
      struct _M0TPB5EntryGssE* _M0L8_2aentryS1028 = _M0L7_2aSomeS1027;
      int32_t _M0L4hashS3384 = _M0L8_2aentryS1028->$3;
      int32_t _if__result_5598;
      int32_t _M0L3pslS3387;
      int32_t _M0L6_2atmpS3389;
      int32_t _M0L6_2atmpS3391;
      int32_t _M0L14capacity__maskS3392;
      int32_t _M0L6_2atmpS3390;
      if (_M0L4hashS3384 == _M0L4hashS1021) {
        moonbit_string_t _M0L3keyS3383 = _M0L8_2aentryS1028->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5598
        = _M0L3keyS3383 == _M0L3keyS1022
          || Moonbit_array_length(_M0L3keyS3383)
             == Moonbit_array_length(_M0L3keyS1022)
             && 0
                == memcmp(_M0L3keyS3383, _M0L3keyS1022, Moonbit_array_length(_M0L3keyS3383) * 2);
      } else {
        _if__result_5598 = 0;
      }
      if (_if__result_5598) {
        moonbit_string_t _M0L5valueS3386 = _M0L8_2aentryS1028->$5;
        moonbit_string_t _M0L6_2atmpS3385;
        moonbit_incref(_M0L5valueS3386);
        _M0L6_2atmpS3385 = _M0L5valueS3386;
        return _M0L6_2atmpS3385;
      } else {
        moonbit_incref(_M0L8_2aentryS1028);
      }
      _M0L3pslS3387 = _M0L8_2aentryS1028->$2;
      moonbit_decref(_M0L8_2aentryS1028);
      if (_M0L1iS1023 > _M0L3pslS3387) {
        moonbit_string_t _M0L6_2atmpS3388 = 0;
        return _M0L6_2atmpS3388;
      }
      _M0L6_2atmpS3389 = _M0L1iS1023 + 1;
      _M0L6_2atmpS3391 = _M0L3idxS1024 + 1;
      _M0L14capacity__maskS3392 = _M0L4selfS1026->$3;
      _M0L6_2atmpS3390 = _M0L6_2atmpS3391 & _M0L14capacity__maskS3392;
      _M0L1iS1023 = _M0L6_2atmpS3389;
      _M0L3idxS1024 = _M0L6_2atmpS3390;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1035,
  moonbit_string_t _M0L3keyS1031
) {
  int32_t _M0L4hashS1030;
  int32_t _M0L14capacity__maskS3409;
  int32_t _M0L6_2atmpS3408;
  int32_t _M0L1iS1032;
  int32_t _M0L3idxS1033;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1030 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1031);
  _M0L14capacity__maskS3409 = _M0L4selfS1035->$3;
  _M0L6_2atmpS3408 = _M0L4hashS1030 & _M0L14capacity__maskS3409;
  _M0L1iS1032 = 0;
  _M0L3idxS1033 = _M0L6_2atmpS3408;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS3407 = _M0L4selfS1035->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS1034;
    if (
      _M0L3idxS1033 < 0
      || _M0L3idxS1033 >= Moonbit_array_length(_M0L7entriesS3407)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1034
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3407[_M0L3idxS1033];
    if (_M0L7_2abindS1034 == 0) {
      void* _M0L4NoneS3396 =
        (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      return _M0L4NoneS3396;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS1036 = _M0L7_2abindS1034;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS1037 = _M0L7_2aSomeS1036;
      int32_t _M0L4hashS3398 = _M0L8_2aentryS1037->$3;
      int32_t _if__result_5600;
      int32_t _M0L3pslS3401;
      int32_t _M0L6_2atmpS3403;
      int32_t _M0L6_2atmpS3405;
      int32_t _M0L14capacity__maskS3406;
      int32_t _M0L6_2atmpS3404;
      if (_M0L4hashS3398 == _M0L4hashS1030) {
        moonbit_string_t _M0L3keyS3397 = _M0L8_2aentryS1037->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5600
        = _M0L3keyS3397 == _M0L3keyS1031
          || Moonbit_array_length(_M0L3keyS3397)
             == Moonbit_array_length(_M0L3keyS1031)
             && 0
                == memcmp(_M0L3keyS3397, _M0L3keyS1031, Moonbit_array_length(_M0L3keyS3397) * 2);
      } else {
        _if__result_5600 = 0;
      }
      if (_if__result_5600) {
        float _M0L5valueS3400 = _M0L8_2aentryS1037->$5;
        void* _M0L4SomeS3399 =
          (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGfE4Some));
        Moonbit_object_header(_M0L4SomeS3399)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 1);
        ((struct _M0DTPC16option6OptionGfE4Some*)_M0L4SomeS3399)->$0
        = _M0L5valueS3400;
        return _M0L4SomeS3399;
      } else {
        moonbit_incref(_M0L8_2aentryS1037);
      }
      _M0L3pslS3401 = _M0L8_2aentryS1037->$2;
      moonbit_decref(_M0L8_2aentryS1037);
      if (_M0L1iS1032 > _M0L3pslS3401) {
        void* _M0L4NoneS3402 =
          (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        return _M0L4NoneS3402;
      }
      _M0L6_2atmpS3403 = _M0L1iS1032 + 1;
      _M0L6_2atmpS3405 = _M0L3idxS1033 + 1;
      _M0L14capacity__maskS3406 = _M0L4selfS1035->$3;
      _M0L6_2atmpS3404 = _M0L6_2atmpS3405 & _M0L14capacity__maskS3406;
      _M0L1iS1032 = _M0L6_2atmpS3403;
      _M0L3idxS1033 = _M0L6_2atmpS3404;
      continue;
    }
    break;
  }
}

int64_t _M0MPB3Map3getGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1044,
  moonbit_string_t _M0L3keyS1040
) {
  int32_t _M0L4hashS1039;
  int32_t _M0L14capacity__maskS3421;
  int32_t _M0L6_2atmpS3420;
  int32_t _M0L1iS1041;
  int32_t _M0L3idxS1042;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS1039 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1040);
  _M0L14capacity__maskS3421 = _M0L4selfS1044->$3;
  _M0L6_2atmpS3420 = _M0L4hashS1039 & _M0L14capacity__maskS3421;
  _M0L1iS1041 = 0;
  _M0L3idxS1042 = _M0L6_2atmpS3420;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3419 = _M0L4selfS1044->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS1043;
    if (
      _M0L3idxS1042 < 0
      || _M0L3idxS1042 >= Moonbit_array_length(_M0L7entriesS3419)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1043
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3419[_M0L3idxS1042];
    if (_M0L7_2abindS1043 == 0) {
      return 4294967296ll;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS1045 = _M0L7_2abindS1043;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS1046 = _M0L7_2aSomeS1045;
      int32_t _M0L4hashS3411 = _M0L8_2aentryS1046->$3;
      int32_t _if__result_5602;
      int32_t _M0L3pslS3414;
      int32_t _M0L6_2atmpS3415;
      int32_t _M0L6_2atmpS3417;
      int32_t _M0L14capacity__maskS3418;
      int32_t _M0L6_2atmpS3416;
      if (_M0L4hashS3411 == _M0L4hashS1039) {
        moonbit_string_t _M0L3keyS3410 = _M0L8_2aentryS1046->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5602
        = _M0L3keyS3410 == _M0L3keyS1040
          || Moonbit_array_length(_M0L3keyS3410)
             == Moonbit_array_length(_M0L3keyS1040)
             && 0
                == memcmp(_M0L3keyS3410, _M0L3keyS1040, Moonbit_array_length(_M0L3keyS3410) * 2);
      } else {
        _if__result_5602 = 0;
      }
      if (_if__result_5602) {
        int32_t _M0L5valueS3413 = _M0L8_2aentryS1046->$5;
        int64_t _M0L6_2atmpS3412 = (int64_t)_M0L5valueS3413;
        return _M0L6_2atmpS3412;
      } else {
        moonbit_incref(_M0L8_2aentryS1046);
      }
      _M0L3pslS3414 = _M0L8_2aentryS1046->$2;
      moonbit_decref(_M0L8_2aentryS1046);
      if (_M0L1iS1041 > _M0L3pslS3414) {
        return 4294967296ll;
      }
      _M0L6_2atmpS3415 = _M0L1iS1041 + 1;
      _M0L6_2atmpS3417 = _M0L3idxS1042 + 1;
      _M0L14capacity__maskS3418 = _M0L4selfS1044->$3;
      _M0L6_2atmpS3416 = _M0L6_2atmpS3417 & _M0L14capacity__maskS3418;
      _M0L1iS1041 = _M0L6_2atmpS3415;
      _M0L3idxS1042 = _M0L6_2atmpS3416;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0MPB3Map3MapGsRP19moonbitDB10RedisValueE(
  struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE _M0L3arrS958,
  int64_t _M0L8capacityS960
) {
  int32_t _M0L3endS3322;
  int32_t _M0L5startS3323;
  int32_t _M0L6lengthS957;
  int32_t _M0L8capacityS959;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L1mS963;
  int32_t _M0L3endS3319;
  int32_t _M0L5startS3320;
  int32_t _M0L7_2abindS964;
  int32_t _M0L2__S965;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS3322 = _M0L3arrS958.$2;
  _M0L5startS3323 = _M0L3arrS958.$1;
  _M0L6lengthS957 = _M0L3endS3322 - _M0L5startS3323;
  if (_M0L8capacityS960 == 4294967296ll) {
    if (_M0L6lengthS957 == 0) {
      _M0L8capacityS959 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS959 = _M0FPB21capacity__for__length(_M0L6lengthS957);
    }
  } else {
    int64_t _M0L7_2aSomeS961 = _M0L8capacityS960;
    int32_t _M0L11_2acapacityS962 = (int32_t)_M0L7_2aSomeS961;
    int32_t _M0L6_2atmpS3321;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS3321 = _M0FPB21capacity__for__length(_M0L6lengthS957);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS959
    = _M0MPC13int3Int3max(_M0L11_2acapacityS962, _M0L6_2atmpS3321);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS963 = _M0FPB8new__mapGsRP19moonbitDB10RedisValueE(_M0L8capacityS959);
  _M0L3endS3319 = _M0L3arrS958.$2;
  _M0L5startS3320 = _M0L3arrS958.$1;
  _M0L7_2abindS964 = _M0L3endS3319 - _M0L5startS3320;
  _M0L2__S965 = 0;
  while (1) {
    if (_M0L2__S965 < _M0L7_2abindS964) {
      struct _M0TUsRP19moonbitDB10RedisValueE** _M0L3bufS3316 =
        _M0L3arrS958.$0;
      int32_t _M0L5startS3318 = _M0L3arrS958.$1;
      int32_t _M0L6_2atmpS3317 = _M0L5startS3318 + _M0L2__S965;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L1eS966 =
        (struct _M0TUsRP19moonbitDB10RedisValueE*)_M0L3bufS3316[
          _M0L6_2atmpS3317
        ];
      moonbit_string_t _M0L6_2atmpS3313 = _M0L1eS966->$0;
      void* _M0L6_2atmpS3314 = _M0L1eS966->$1;
      int32_t _M0L6_2atmpS3315;
      moonbit_incref(_M0L6_2atmpS3314);
      moonbit_incref(_M0L6_2atmpS3313);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L1mS963, _M0L6_2atmpS3313, _M0L6_2atmpS3314);
      moonbit_decref(_M0L6_2atmpS3313);
      moonbit_decref(_M0L6_2atmpS3314);
      _M0L6_2atmpS3315 = _M0L2__S965 + 1;
      _M0L2__S965 = _M0L6_2atmpS3315;
      continue;
    }
    break;
  }
  return _M0L1mS963;
}

struct _M0TPB3MapGsiE* _M0MPB3Map3MapGsiE(
  struct _M0TPB9ArrayViewGUsiEE _M0L3arrS969,
  int64_t _M0L8capacityS971
) {
  int32_t _M0L3endS3333;
  int32_t _M0L5startS3334;
  int32_t _M0L6lengthS968;
  int32_t _M0L8capacityS970;
  struct _M0TPB3MapGsiE* _M0L1mS974;
  int32_t _M0L3endS3330;
  int32_t _M0L5startS3331;
  int32_t _M0L7_2abindS975;
  int32_t _M0L2__S976;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS3333 = _M0L3arrS969.$2;
  _M0L5startS3334 = _M0L3arrS969.$1;
  _M0L6lengthS968 = _M0L3endS3333 - _M0L5startS3334;
  if (_M0L8capacityS971 == 4294967296ll) {
    if (_M0L6lengthS968 == 0) {
      _M0L8capacityS970 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS970 = _M0FPB21capacity__for__length(_M0L6lengthS968);
    }
  } else {
    int64_t _M0L7_2aSomeS972 = _M0L8capacityS971;
    int32_t _M0L11_2acapacityS973 = (int32_t)_M0L7_2aSomeS972;
    int32_t _M0L6_2atmpS3332;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS3332 = _M0FPB21capacity__for__length(_M0L6lengthS968);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS970
    = _M0MPC13int3Int3max(_M0L11_2acapacityS973, _M0L6_2atmpS3332);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS974 = _M0FPB8new__mapGsiE(_M0L8capacityS970);
  _M0L3endS3330 = _M0L3arrS969.$2;
  _M0L5startS3331 = _M0L3arrS969.$1;
  _M0L7_2abindS975 = _M0L3endS3330 - _M0L5startS3331;
  _M0L2__S976 = 0;
  while (1) {
    if (_M0L2__S976 < _M0L7_2abindS975) {
      struct _M0TUsiE** _M0L3bufS3327 = _M0L3arrS969.$0;
      int32_t _M0L5startS3329 = _M0L3arrS969.$1;
      int32_t _M0L6_2atmpS3328 = _M0L5startS3329 + _M0L2__S976;
      struct _M0TUsiE* _M0L1eS977 =
        (struct _M0TUsiE*)_M0L3bufS3327[_M0L6_2atmpS3328];
      moonbit_string_t _M0L6_2atmpS3324 = _M0L1eS977->$0;
      int32_t _M0L6_2atmpS3325 = _M0L1eS977->$1;
      int32_t _M0L6_2atmpS3326;
      moonbit_incref(_M0L6_2atmpS3324);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsiE(_M0L1mS974, _M0L6_2atmpS3324, _M0L6_2atmpS3325);
      moonbit_decref(_M0L6_2atmpS3324);
      _M0L6_2atmpS3326 = _M0L2__S976 + 1;
      _M0L2__S976 = _M0L6_2atmpS3326;
      continue;
    }
    break;
  }
  return _M0L1mS974;
}

struct _M0TPB3MapGssE* _M0MPB3Map3MapGssE(
  struct _M0TPB9ArrayViewGUssEE _M0L3arrS980,
  int64_t _M0L8capacityS982
) {
  int32_t _M0L3endS3344;
  int32_t _M0L5startS3345;
  int32_t _M0L6lengthS979;
  int32_t _M0L8capacityS981;
  struct _M0TPB3MapGssE* _M0L1mS985;
  int32_t _M0L3endS3341;
  int32_t _M0L5startS3342;
  int32_t _M0L7_2abindS986;
  int32_t _M0L2__S987;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS3344 = _M0L3arrS980.$2;
  _M0L5startS3345 = _M0L3arrS980.$1;
  _M0L6lengthS979 = _M0L3endS3344 - _M0L5startS3345;
  if (_M0L8capacityS982 == 4294967296ll) {
    if (_M0L6lengthS979 == 0) {
      _M0L8capacityS981 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS981 = _M0FPB21capacity__for__length(_M0L6lengthS979);
    }
  } else {
    int64_t _M0L7_2aSomeS983 = _M0L8capacityS982;
    int32_t _M0L11_2acapacityS984 = (int32_t)_M0L7_2aSomeS983;
    int32_t _M0L6_2atmpS3343;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS3343 = _M0FPB21capacity__for__length(_M0L6lengthS979);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS981
    = _M0MPC13int3Int3max(_M0L11_2acapacityS984, _M0L6_2atmpS3343);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS985 = _M0FPB8new__mapGssE(_M0L8capacityS981);
  _M0L3endS3341 = _M0L3arrS980.$2;
  _M0L5startS3342 = _M0L3arrS980.$1;
  _M0L7_2abindS986 = _M0L3endS3341 - _M0L5startS3342;
  _M0L2__S987 = 0;
  while (1) {
    if (_M0L2__S987 < _M0L7_2abindS986) {
      struct _M0TUssE** _M0L3bufS3338 = _M0L3arrS980.$0;
      int32_t _M0L5startS3340 = _M0L3arrS980.$1;
      int32_t _M0L6_2atmpS3339 = _M0L5startS3340 + _M0L2__S987;
      struct _M0TUssE* _M0L1eS988 =
        (struct _M0TUssE*)_M0L3bufS3338[_M0L6_2atmpS3339];
      moonbit_string_t _M0L6_2atmpS3335 = _M0L1eS988->$0;
      moonbit_string_t _M0L6_2atmpS3336 = _M0L1eS988->$1;
      int32_t _M0L6_2atmpS3337;
      moonbit_incref(_M0L6_2atmpS3336);
      moonbit_incref(_M0L6_2atmpS3335);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGssE(_M0L1mS985, _M0L6_2atmpS3335, _M0L6_2atmpS3336);
      moonbit_decref(_M0L6_2atmpS3335);
      moonbit_decref(_M0L6_2atmpS3336);
      _M0L6_2atmpS3337 = _M0L2__S987 + 1;
      _M0L2__S987 = _M0L6_2atmpS3337;
      continue;
    }
    break;
  }
  return _M0L1mS985;
}

struct _M0TPB3MapGsbE* _M0MPB3Map3MapGsbE(
  struct _M0TPB9ArrayViewGUsbEE _M0L3arrS991,
  int64_t _M0L8capacityS993
) {
  int32_t _M0L3endS3355;
  int32_t _M0L5startS3356;
  int32_t _M0L6lengthS990;
  int32_t _M0L8capacityS992;
  struct _M0TPB3MapGsbE* _M0L1mS996;
  int32_t _M0L3endS3352;
  int32_t _M0L5startS3353;
  int32_t _M0L7_2abindS997;
  int32_t _M0L2__S998;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS3355 = _M0L3arrS991.$2;
  _M0L5startS3356 = _M0L3arrS991.$1;
  _M0L6lengthS990 = _M0L3endS3355 - _M0L5startS3356;
  if (_M0L8capacityS993 == 4294967296ll) {
    if (_M0L6lengthS990 == 0) {
      _M0L8capacityS992 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS992 = _M0FPB21capacity__for__length(_M0L6lengthS990);
    }
  } else {
    int64_t _M0L7_2aSomeS994 = _M0L8capacityS993;
    int32_t _M0L11_2acapacityS995 = (int32_t)_M0L7_2aSomeS994;
    int32_t _M0L6_2atmpS3354;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS3354 = _M0FPB21capacity__for__length(_M0L6lengthS990);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS992
    = _M0MPC13int3Int3max(_M0L11_2acapacityS995, _M0L6_2atmpS3354);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS996 = _M0FPB8new__mapGsbE(_M0L8capacityS992);
  _M0L3endS3352 = _M0L3arrS991.$2;
  _M0L5startS3353 = _M0L3arrS991.$1;
  _M0L7_2abindS997 = _M0L3endS3352 - _M0L5startS3353;
  _M0L2__S998 = 0;
  while (1) {
    if (_M0L2__S998 < _M0L7_2abindS997) {
      struct _M0TUsbE** _M0L3bufS3349 = _M0L3arrS991.$0;
      int32_t _M0L5startS3351 = _M0L3arrS991.$1;
      int32_t _M0L6_2atmpS3350 = _M0L5startS3351 + _M0L2__S998;
      struct _M0TUsbE* _M0L1eS999 =
        (struct _M0TUsbE*)_M0L3bufS3349[_M0L6_2atmpS3350];
      moonbit_string_t _M0L6_2atmpS3346 = _M0L1eS999->$0;
      int32_t _M0L6_2atmpS3347 = _M0L1eS999->$1;
      int32_t _M0L6_2atmpS3348;
      moonbit_incref(_M0L6_2atmpS3346);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsbE(_M0L1mS996, _M0L6_2atmpS3346, _M0L6_2atmpS3347);
      moonbit_decref(_M0L6_2atmpS3346);
      _M0L6_2atmpS3348 = _M0L2__S998 + 1;
      _M0L2__S998 = _M0L6_2atmpS3348;
      continue;
    }
    break;
  }
  return _M0L1mS996;
}

struct _M0TPB3MapGsfE* _M0MPB3Map3MapGsfE(
  struct _M0TPB9ArrayViewGUsfEE _M0L3arrS1002,
  int64_t _M0L8capacityS1004
) {
  int32_t _M0L3endS3366;
  int32_t _M0L5startS3367;
  int32_t _M0L6lengthS1001;
  int32_t _M0L8capacityS1003;
  struct _M0TPB3MapGsfE* _M0L1mS1007;
  int32_t _M0L3endS3363;
  int32_t _M0L5startS3364;
  int32_t _M0L7_2abindS1008;
  int32_t _M0L2__S1009;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS3366 = _M0L3arrS1002.$2;
  _M0L5startS3367 = _M0L3arrS1002.$1;
  _M0L6lengthS1001 = _M0L3endS3366 - _M0L5startS3367;
  if (_M0L8capacityS1004 == 4294967296ll) {
    if (_M0L6lengthS1001 == 0) {
      _M0L8capacityS1003 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS1003 = _M0FPB21capacity__for__length(_M0L6lengthS1001);
    }
  } else {
    int64_t _M0L7_2aSomeS1005 = _M0L8capacityS1004;
    int32_t _M0L11_2acapacityS1006 = (int32_t)_M0L7_2aSomeS1005;
    int32_t _M0L6_2atmpS3365;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS3365 = _M0FPB21capacity__for__length(_M0L6lengthS1001);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS1003
    = _M0MPC13int3Int3max(_M0L11_2acapacityS1006, _M0L6_2atmpS3365);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS1007 = _M0FPB8new__mapGsfE(_M0L8capacityS1003);
  _M0L3endS3363 = _M0L3arrS1002.$2;
  _M0L5startS3364 = _M0L3arrS1002.$1;
  _M0L7_2abindS1008 = _M0L3endS3363 - _M0L5startS3364;
  _M0L2__S1009 = 0;
  while (1) {
    if (_M0L2__S1009 < _M0L7_2abindS1008) {
      struct _M0TUsfE** _M0L3bufS3360 = _M0L3arrS1002.$0;
      int32_t _M0L5startS3362 = _M0L3arrS1002.$1;
      int32_t _M0L6_2atmpS3361 = _M0L5startS3362 + _M0L2__S1009;
      struct _M0TUsfE* _M0L1eS1010 =
        (struct _M0TUsfE*)_M0L3bufS3360[_M0L6_2atmpS3361];
      moonbit_string_t _M0L6_2atmpS3357 = _M0L1eS1010->$0;
      float _M0L6_2atmpS3358 = _M0L1eS1010->$1;
      int32_t _M0L6_2atmpS3359;
      moonbit_incref(_M0L6_2atmpS3357);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsfE(_M0L1mS1007, _M0L6_2atmpS3357, _M0L6_2atmpS3358);
      moonbit_decref(_M0L6_2atmpS3357);
      _M0L6_2atmpS3359 = _M0L2__S1009 + 1;
      _M0L2__S1009 = _M0L6_2atmpS3359;
      continue;
    }
    break;
  }
  return _M0L1mS1007;
}

int32_t _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS942,
  moonbit_string_t _M0L3keyS943,
  void* _M0L5valueS944
) {
  int32_t _M0L6_2atmpS3308;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3308 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS943);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRP19moonbitDB10RedisValueE(_M0L4selfS942, _M0L3keyS943, _M0L5valueS944, _M0L6_2atmpS3308);
  return 0;
}

int32_t _M0MPB3Map3setGssE(
  struct _M0TPB3MapGssE* _M0L4selfS945,
  moonbit_string_t _M0L3keyS946,
  moonbit_string_t _M0L5valueS947
) {
  int32_t _M0L6_2atmpS3309;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3309 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS946);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGssE(_M0L4selfS945, _M0L3keyS946, _M0L5valueS947, _M0L6_2atmpS3309);
  return 0;
}

int32_t _M0MPB3Map3setGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS948,
  moonbit_string_t _M0L3keyS949,
  int32_t _M0L5valueS950
) {
  int32_t _M0L6_2atmpS3310;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3310 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS949);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsbE(_M0L4selfS948, _M0L3keyS949, _M0L5valueS950, _M0L6_2atmpS3310);
  return 0;
}

int32_t _M0MPB3Map3setGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS951,
  moonbit_string_t _M0L3keyS952,
  float _M0L5valueS953
) {
  int32_t _M0L6_2atmpS3311;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3311 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS952);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsfE(_M0L4selfS951, _M0L3keyS952, _M0L5valueS953, _M0L6_2atmpS3311);
  return 0;
}

int32_t _M0MPB3Map3setGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS954,
  moonbit_string_t _M0L3keyS955,
  int32_t _M0L5valueS956
) {
  int32_t _M0L6_2atmpS3312;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3312 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS955);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsiE(_M0L4selfS954, _M0L3keyS955, _M0L5valueS956, _M0L6_2atmpS3312);
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS865,
  moonbit_string_t _M0L3keyS871,
  void* _M0L5valueS872,
  int32_t _M0L4hashS867
) {
  int32_t _M0L14capacity__maskS3235;
  int32_t _M0L6_2atmpS3234;
  int32_t _M0L3pslS862;
  int32_t _M0L3idxS863;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3235 = _M0L4selfS865->$3;
  _M0L6_2atmpS3234 = _M0L4hashS867 & _M0L14capacity__maskS3235;
  _M0L3pslS862 = 0;
  _M0L3idxS863 = _M0L6_2atmpS3234;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3233 =
      _M0L4selfS865->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS864;
    if (
      _M0L3idxS863 < 0
      || _M0L3idxS863 >= Moonbit_array_length(_M0L7entriesS3233)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS864
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3233[
        _M0L3idxS863
      ];
    if (_M0L7_2abindS864 == 0) {
      int32_t _M0L4sizeS3218 = _M0L4selfS865->$1;
      int32_t _M0L8grow__atS3219 = _M0L4selfS865->$4;
      int32_t _M0L7_2abindS868;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS869;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS870;
      if (_M0L4sizeS3218 >= _M0L8grow__atS3219) {
        int32_t _M0L14capacity__maskS3221;
        int32_t _M0L6_2atmpS3220;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRP19moonbitDB10RedisValueE(_M0L4selfS865);
        _M0L14capacity__maskS3221 = _M0L4selfS865->$3;
        _M0L6_2atmpS3220 = _M0L4hashS867 & _M0L14capacity__maskS3221;
        _M0L3pslS862 = 0;
        _M0L3idxS863 = _M0L6_2atmpS3220;
        continue;
      }
      _M0L7_2abindS868 = _M0L4selfS865->$6;
      _M0L7_2abindS869 = 0;
      moonbit_incref(_M0L3keyS871);
      moonbit_incref(_M0L5valueS872);
      _M0L5entryS870
      = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE));
      Moonbit_object_header(_M0L5entryS870)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 84, 0);
      _M0L5entryS870->$0 = _M0L7_2abindS868;
      _M0L5entryS870->$1 = _M0L7_2abindS869;
      _M0L5entryS870->$2 = _M0L3pslS862;
      _M0L5entryS870->$3 = _M0L4hashS867;
      _M0L5entryS870->$4 = _M0L3keyS871;
      _M0L5entryS870->$5 = _M0L5valueS872;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(_M0L4selfS865, _M0L3idxS863, _M0L5entryS870);
      moonbit_decref(_M0L5entryS870);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS873 =
        _M0L7_2abindS864;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L14_2acurr__entryS874 =
        _M0L7_2aSomeS873;
      int32_t _M0L4hashS3223 = _M0L14_2acurr__entryS874->$3;
      int32_t _if__result_5609;
      int32_t _M0L3pslS3224;
      int32_t _M0L6_2atmpS3229;
      int32_t _M0L6_2atmpS3231;
      int32_t _M0L14capacity__maskS3232;
      int32_t _M0L6_2atmpS3230;
      if (_M0L4hashS3223 == _M0L4hashS867) {
        moonbit_string_t _M0L3keyS3222 = _M0L14_2acurr__entryS874->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5609
        = _M0L3keyS3222 == _M0L3keyS871
          || Moonbit_array_length(_M0L3keyS3222)
             == Moonbit_array_length(_M0L3keyS871)
             && 0
                == memcmp(_M0L3keyS3222, _M0L3keyS871, Moonbit_array_length(_M0L3keyS3222) * 2);
      } else {
        _if__result_5609 = 0;
      }
      if (_if__result_5609) {
        void* _M0L6_2aoldS4960 = _M0L14_2acurr__entryS874->$5;
        moonbit_incref(_M0L5valueS872);
        moonbit_decref(_M0L6_2aoldS4960);
        _M0L14_2acurr__entryS874->$5 = _M0L5valueS872;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS874);
      }
      _M0L3pslS3224 = _M0L14_2acurr__entryS874->$2;
      if (_M0L3pslS862 > _M0L3pslS3224) {
        int32_t _M0L4sizeS3225 = _M0L4selfS865->$1;
        int32_t _M0L8grow__atS3226 = _M0L4selfS865->$4;
        int32_t _M0L7_2abindS875;
        struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS876;
        struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS877;
        if (_M0L4sizeS3225 >= _M0L8grow__atS3226) {
          int32_t _M0L14capacity__maskS3228;
          int32_t _M0L6_2atmpS3227;
          moonbit_decref(_M0L14_2acurr__entryS874);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRP19moonbitDB10RedisValueE(_M0L4selfS865);
          _M0L14capacity__maskS3228 = _M0L4selfS865->$3;
          _M0L6_2atmpS3227 = _M0L4hashS867 & _M0L14capacity__maskS3228;
          _M0L3pslS862 = 0;
          _M0L3idxS863 = _M0L6_2atmpS3227;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP19moonbitDB10RedisValueE(_M0L4selfS865, _M0L3idxS863, _M0L14_2acurr__entryS874);
        moonbit_decref(_M0L14_2acurr__entryS874);
        _M0L7_2abindS875 = _M0L4selfS865->$6;
        _M0L7_2abindS876 = 0;
        moonbit_incref(_M0L3keyS871);
        moonbit_incref(_M0L5valueS872);
        _M0L5entryS877
        = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE));
        Moonbit_object_header(_M0L5entryS877)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 84, 0);
        _M0L5entryS877->$0 = _M0L7_2abindS875;
        _M0L5entryS877->$1 = _M0L7_2abindS876;
        _M0L5entryS877->$2 = _M0L3pslS862;
        _M0L5entryS877->$3 = _M0L4hashS867;
        _M0L5entryS877->$4 = _M0L3keyS871;
        _M0L5entryS877->$5 = _M0L5valueS872;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(_M0L4selfS865, _M0L3idxS863, _M0L5entryS877);
        moonbit_decref(_M0L5entryS877);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS874);
      }
      _M0L6_2atmpS3229 = _M0L3pslS862 + 1;
      _M0L6_2atmpS3231 = _M0L3idxS863 + 1;
      _M0L14capacity__maskS3232 = _M0L4selfS865->$3;
      _M0L6_2atmpS3230 = _M0L6_2atmpS3231 & _M0L14capacity__maskS3232;
      _M0L3pslS862 = _M0L6_2atmpS3229;
      _M0L3idxS863 = _M0L6_2atmpS3230;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGssE(
  struct _M0TPB3MapGssE* _M0L4selfS881,
  moonbit_string_t _M0L3keyS887,
  moonbit_string_t _M0L5valueS888,
  int32_t _M0L4hashS883
) {
  int32_t _M0L14capacity__maskS3253;
  int32_t _M0L6_2atmpS3252;
  int32_t _M0L3pslS878;
  int32_t _M0L3idxS879;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3253 = _M0L4selfS881->$3;
  _M0L6_2atmpS3252 = _M0L4hashS883 & _M0L14capacity__maskS3253;
  _M0L3pslS878 = 0;
  _M0L3idxS879 = _M0L6_2atmpS3252;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS3251 = _M0L4selfS881->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS880;
    if (
      _M0L3idxS879 < 0
      || _M0L3idxS879 >= Moonbit_array_length(_M0L7entriesS3251)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS880
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3251[_M0L3idxS879];
    if (_M0L7_2abindS880 == 0) {
      int32_t _M0L4sizeS3236 = _M0L4selfS881->$1;
      int32_t _M0L8grow__atS3237 = _M0L4selfS881->$4;
      int32_t _M0L7_2abindS884;
      struct _M0TPB5EntryGssE* _M0L7_2abindS885;
      struct _M0TPB5EntryGssE* _M0L5entryS886;
      if (_M0L4sizeS3236 >= _M0L8grow__atS3237) {
        int32_t _M0L14capacity__maskS3239;
        int32_t _M0L6_2atmpS3238;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGssE(_M0L4selfS881);
        _M0L14capacity__maskS3239 = _M0L4selfS881->$3;
        _M0L6_2atmpS3238 = _M0L4hashS883 & _M0L14capacity__maskS3239;
        _M0L3pslS878 = 0;
        _M0L3idxS879 = _M0L6_2atmpS3238;
        continue;
      }
      _M0L7_2abindS884 = _M0L4selfS881->$6;
      _M0L7_2abindS885 = 0;
      moonbit_incref(_M0L3keyS887);
      moonbit_incref(_M0L5valueS888);
      _M0L5entryS886
      = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
      Moonbit_object_header(_M0L5entryS886)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 89, 0);
      _M0L5entryS886->$0 = _M0L7_2abindS884;
      _M0L5entryS886->$1 = _M0L7_2abindS885;
      _M0L5entryS886->$2 = _M0L3pslS878;
      _M0L5entryS886->$3 = _M0L4hashS883;
      _M0L5entryS886->$4 = _M0L3keyS887;
      _M0L5entryS886->$5 = _M0L5valueS888;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS881, _M0L3idxS879, _M0L5entryS886);
      moonbit_decref(_M0L5entryS886);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS889 = _M0L7_2abindS880;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS890 = _M0L7_2aSomeS889;
      int32_t _M0L4hashS3241 = _M0L14_2acurr__entryS890->$3;
      int32_t _if__result_5611;
      int32_t _M0L3pslS3242;
      int32_t _M0L6_2atmpS3247;
      int32_t _M0L6_2atmpS3249;
      int32_t _M0L14capacity__maskS3250;
      int32_t _M0L6_2atmpS3248;
      if (_M0L4hashS3241 == _M0L4hashS883) {
        moonbit_string_t _M0L3keyS3240 = _M0L14_2acurr__entryS890->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5611
        = _M0L3keyS3240 == _M0L3keyS887
          || Moonbit_array_length(_M0L3keyS3240)
             == Moonbit_array_length(_M0L3keyS887)
             && 0
                == memcmp(_M0L3keyS3240, _M0L3keyS887, Moonbit_array_length(_M0L3keyS3240) * 2);
      } else {
        _if__result_5611 = 0;
      }
      if (_if__result_5611) {
        moonbit_string_t _M0L6_2aoldS4964 = _M0L14_2acurr__entryS890->$5;
        moonbit_incref(_M0L5valueS888);
        moonbit_decref(_M0L6_2aoldS4964);
        _M0L14_2acurr__entryS890->$5 = _M0L5valueS888;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS890);
      }
      _M0L3pslS3242 = _M0L14_2acurr__entryS890->$2;
      if (_M0L3pslS878 > _M0L3pslS3242) {
        int32_t _M0L4sizeS3243 = _M0L4selfS881->$1;
        int32_t _M0L8grow__atS3244 = _M0L4selfS881->$4;
        int32_t _M0L7_2abindS891;
        struct _M0TPB5EntryGssE* _M0L7_2abindS892;
        struct _M0TPB5EntryGssE* _M0L5entryS893;
        if (_M0L4sizeS3243 >= _M0L8grow__atS3244) {
          int32_t _M0L14capacity__maskS3246;
          int32_t _M0L6_2atmpS3245;
          moonbit_decref(_M0L14_2acurr__entryS890);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGssE(_M0L4selfS881);
          _M0L14capacity__maskS3246 = _M0L4selfS881->$3;
          _M0L6_2atmpS3245 = _M0L4hashS883 & _M0L14capacity__maskS3246;
          _M0L3pslS878 = 0;
          _M0L3idxS879 = _M0L6_2atmpS3245;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS881, _M0L3idxS879, _M0L14_2acurr__entryS890);
        moonbit_decref(_M0L14_2acurr__entryS890);
        _M0L7_2abindS891 = _M0L4selfS881->$6;
        _M0L7_2abindS892 = 0;
        moonbit_incref(_M0L3keyS887);
        moonbit_incref(_M0L5valueS888);
        _M0L5entryS893
        = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
        Moonbit_object_header(_M0L5entryS893)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 89, 0);
        _M0L5entryS893->$0 = _M0L7_2abindS891;
        _M0L5entryS893->$1 = _M0L7_2abindS892;
        _M0L5entryS893->$2 = _M0L3pslS878;
        _M0L5entryS893->$3 = _M0L4hashS883;
        _M0L5entryS893->$4 = _M0L3keyS887;
        _M0L5entryS893->$5 = _M0L5valueS888;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS881, _M0L3idxS879, _M0L5entryS893);
        moonbit_decref(_M0L5entryS893);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS890);
      }
      _M0L6_2atmpS3247 = _M0L3pslS878 + 1;
      _M0L6_2atmpS3249 = _M0L3idxS879 + 1;
      _M0L14capacity__maskS3250 = _M0L4selfS881->$3;
      _M0L6_2atmpS3248 = _M0L6_2atmpS3249 & _M0L14capacity__maskS3250;
      _M0L3pslS878 = _M0L6_2atmpS3247;
      _M0L3idxS879 = _M0L6_2atmpS3248;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS897,
  moonbit_string_t _M0L3keyS903,
  int32_t _M0L5valueS904,
  int32_t _M0L4hashS899
) {
  int32_t _M0L14capacity__maskS3271;
  int32_t _M0L6_2atmpS3270;
  int32_t _M0L3pslS894;
  int32_t _M0L3idxS895;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3271 = _M0L4selfS897->$3;
  _M0L6_2atmpS3270 = _M0L4hashS899 & _M0L14capacity__maskS3271;
  _M0L3pslS894 = 0;
  _M0L3idxS895 = _M0L6_2atmpS3270;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS3269 = _M0L4selfS897->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS896;
    if (
      _M0L3idxS895 < 0
      || _M0L3idxS895 >= Moonbit_array_length(_M0L7entriesS3269)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS896
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3269[_M0L3idxS895];
    if (_M0L7_2abindS896 == 0) {
      int32_t _M0L4sizeS3254 = _M0L4selfS897->$1;
      int32_t _M0L8grow__atS3255 = _M0L4selfS897->$4;
      int32_t _M0L7_2abindS900;
      struct _M0TPB5EntryGsbE* _M0L7_2abindS901;
      struct _M0TPB5EntryGsbE* _M0L5entryS902;
      if (_M0L4sizeS3254 >= _M0L8grow__atS3255) {
        int32_t _M0L14capacity__maskS3257;
        int32_t _M0L6_2atmpS3256;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsbE(_M0L4selfS897);
        _M0L14capacity__maskS3257 = _M0L4selfS897->$3;
        _M0L6_2atmpS3256 = _M0L4hashS899 & _M0L14capacity__maskS3257;
        _M0L3pslS894 = 0;
        _M0L3idxS895 = _M0L6_2atmpS3256;
        continue;
      }
      _M0L7_2abindS900 = _M0L4selfS897->$6;
      _M0L7_2abindS901 = 0;
      moonbit_incref(_M0L3keyS903);
      _M0L5entryS902
      = (struct _M0TPB5EntryGsbE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsbE));
      Moonbit_object_header(_M0L5entryS902)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 94, 0);
      _M0L5entryS902->$0 = _M0L7_2abindS900;
      _M0L5entryS902->$1 = _M0L7_2abindS901;
      _M0L5entryS902->$2 = _M0L3pslS894;
      _M0L5entryS902->$3 = _M0L4hashS899;
      _M0L5entryS902->$4 = _M0L3keyS903;
      _M0L5entryS902->$5 = _M0L5valueS904;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS897, _M0L3idxS895, _M0L5entryS902);
      moonbit_decref(_M0L5entryS902);
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS905 = _M0L7_2abindS896;
      struct _M0TPB5EntryGsbE* _M0L14_2acurr__entryS906 = _M0L7_2aSomeS905;
      int32_t _M0L4hashS3259 = _M0L14_2acurr__entryS906->$3;
      int32_t _if__result_5613;
      int32_t _M0L3pslS3260;
      int32_t _M0L6_2atmpS3265;
      int32_t _M0L6_2atmpS3267;
      int32_t _M0L14capacity__maskS3268;
      int32_t _M0L6_2atmpS3266;
      if (_M0L4hashS3259 == _M0L4hashS899) {
        moonbit_string_t _M0L3keyS3258 = _M0L14_2acurr__entryS906->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5613
        = _M0L3keyS3258 == _M0L3keyS903
          || Moonbit_array_length(_M0L3keyS3258)
             == Moonbit_array_length(_M0L3keyS903)
             && 0
                == memcmp(_M0L3keyS3258, _M0L3keyS903, Moonbit_array_length(_M0L3keyS3258) * 2);
      } else {
        _if__result_5613 = 0;
      }
      if (_if__result_5613) {
        _M0L14_2acurr__entryS906->$5 = _M0L5valueS904;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS906);
      }
      _M0L3pslS3260 = _M0L14_2acurr__entryS906->$2;
      if (_M0L3pslS894 > _M0L3pslS3260) {
        int32_t _M0L4sizeS3261 = _M0L4selfS897->$1;
        int32_t _M0L8grow__atS3262 = _M0L4selfS897->$4;
        int32_t _M0L7_2abindS907;
        struct _M0TPB5EntryGsbE* _M0L7_2abindS908;
        struct _M0TPB5EntryGsbE* _M0L5entryS909;
        if (_M0L4sizeS3261 >= _M0L8grow__atS3262) {
          int32_t _M0L14capacity__maskS3264;
          int32_t _M0L6_2atmpS3263;
          moonbit_decref(_M0L14_2acurr__entryS906);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsbE(_M0L4selfS897);
          _M0L14capacity__maskS3264 = _M0L4selfS897->$3;
          _M0L6_2atmpS3263 = _M0L4hashS899 & _M0L14capacity__maskS3264;
          _M0L3pslS894 = 0;
          _M0L3idxS895 = _M0L6_2atmpS3263;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsbE(_M0L4selfS897, _M0L3idxS895, _M0L14_2acurr__entryS906);
        moonbit_decref(_M0L14_2acurr__entryS906);
        _M0L7_2abindS907 = _M0L4selfS897->$6;
        _M0L7_2abindS908 = 0;
        moonbit_incref(_M0L3keyS903);
        _M0L5entryS909
        = (struct _M0TPB5EntryGsbE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsbE));
        Moonbit_object_header(_M0L5entryS909)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 94, 0);
        _M0L5entryS909->$0 = _M0L7_2abindS907;
        _M0L5entryS909->$1 = _M0L7_2abindS908;
        _M0L5entryS909->$2 = _M0L3pslS894;
        _M0L5entryS909->$3 = _M0L4hashS899;
        _M0L5entryS909->$4 = _M0L3keyS903;
        _M0L5entryS909->$5 = _M0L5valueS904;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS897, _M0L3idxS895, _M0L5entryS909);
        moonbit_decref(_M0L5entryS909);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS906);
      }
      _M0L6_2atmpS3265 = _M0L3pslS894 + 1;
      _M0L6_2atmpS3267 = _M0L3idxS895 + 1;
      _M0L14capacity__maskS3268 = _M0L4selfS897->$3;
      _M0L6_2atmpS3266 = _M0L6_2atmpS3267 & _M0L14capacity__maskS3268;
      _M0L3pslS894 = _M0L6_2atmpS3265;
      _M0L3idxS895 = _M0L6_2atmpS3266;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS913,
  moonbit_string_t _M0L3keyS919,
  float _M0L5valueS920,
  int32_t _M0L4hashS915
) {
  int32_t _M0L14capacity__maskS3289;
  int32_t _M0L6_2atmpS3288;
  int32_t _M0L3pslS910;
  int32_t _M0L3idxS911;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3289 = _M0L4selfS913->$3;
  _M0L6_2atmpS3288 = _M0L4hashS915 & _M0L14capacity__maskS3289;
  _M0L3pslS910 = 0;
  _M0L3idxS911 = _M0L6_2atmpS3288;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS3287 = _M0L4selfS913->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS912;
    if (
      _M0L3idxS911 < 0
      || _M0L3idxS911 >= Moonbit_array_length(_M0L7entriesS3287)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS912
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3287[_M0L3idxS911];
    if (_M0L7_2abindS912 == 0) {
      int32_t _M0L4sizeS3272 = _M0L4selfS913->$1;
      int32_t _M0L8grow__atS3273 = _M0L4selfS913->$4;
      int32_t _M0L7_2abindS916;
      struct _M0TPB5EntryGsfE* _M0L7_2abindS917;
      struct _M0TPB5EntryGsfE* _M0L5entryS918;
      if (_M0L4sizeS3272 >= _M0L8grow__atS3273) {
        int32_t _M0L14capacity__maskS3275;
        int32_t _M0L6_2atmpS3274;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsfE(_M0L4selfS913);
        _M0L14capacity__maskS3275 = _M0L4selfS913->$3;
        _M0L6_2atmpS3274 = _M0L4hashS915 & _M0L14capacity__maskS3275;
        _M0L3pslS910 = 0;
        _M0L3idxS911 = _M0L6_2atmpS3274;
        continue;
      }
      _M0L7_2abindS916 = _M0L4selfS913->$6;
      _M0L7_2abindS917 = 0;
      moonbit_incref(_M0L3keyS919);
      _M0L5entryS918
      = (struct _M0TPB5EntryGsfE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsfE));
      Moonbit_object_header(_M0L5entryS918)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 98, 0);
      _M0L5entryS918->$0 = _M0L7_2abindS916;
      _M0L5entryS918->$1 = _M0L7_2abindS917;
      _M0L5entryS918->$2 = _M0L3pslS910;
      _M0L5entryS918->$3 = _M0L4hashS915;
      _M0L5entryS918->$4 = _M0L3keyS919;
      _M0L5entryS918->$5 = _M0L5valueS920;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS913, _M0L3idxS911, _M0L5entryS918);
      moonbit_decref(_M0L5entryS918);
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS921 = _M0L7_2abindS912;
      struct _M0TPB5EntryGsfE* _M0L14_2acurr__entryS922 = _M0L7_2aSomeS921;
      int32_t _M0L4hashS3277 = _M0L14_2acurr__entryS922->$3;
      int32_t _if__result_5615;
      int32_t _M0L3pslS3278;
      int32_t _M0L6_2atmpS3283;
      int32_t _M0L6_2atmpS3285;
      int32_t _M0L14capacity__maskS3286;
      int32_t _M0L6_2atmpS3284;
      if (_M0L4hashS3277 == _M0L4hashS915) {
        moonbit_string_t _M0L3keyS3276 = _M0L14_2acurr__entryS922->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5615
        = _M0L3keyS3276 == _M0L3keyS919
          || Moonbit_array_length(_M0L3keyS3276)
             == Moonbit_array_length(_M0L3keyS919)
             && 0
                == memcmp(_M0L3keyS3276, _M0L3keyS919, Moonbit_array_length(_M0L3keyS3276) * 2);
      } else {
        _if__result_5615 = 0;
      }
      if (_if__result_5615) {
        _M0L14_2acurr__entryS922->$5 = _M0L5valueS920;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS922);
      }
      _M0L3pslS3278 = _M0L14_2acurr__entryS922->$2;
      if (_M0L3pslS910 > _M0L3pslS3278) {
        int32_t _M0L4sizeS3279 = _M0L4selfS913->$1;
        int32_t _M0L8grow__atS3280 = _M0L4selfS913->$4;
        int32_t _M0L7_2abindS923;
        struct _M0TPB5EntryGsfE* _M0L7_2abindS924;
        struct _M0TPB5EntryGsfE* _M0L5entryS925;
        if (_M0L4sizeS3279 >= _M0L8grow__atS3280) {
          int32_t _M0L14capacity__maskS3282;
          int32_t _M0L6_2atmpS3281;
          moonbit_decref(_M0L14_2acurr__entryS922);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsfE(_M0L4selfS913);
          _M0L14capacity__maskS3282 = _M0L4selfS913->$3;
          _M0L6_2atmpS3281 = _M0L4hashS915 & _M0L14capacity__maskS3282;
          _M0L3pslS910 = 0;
          _M0L3idxS911 = _M0L6_2atmpS3281;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsfE(_M0L4selfS913, _M0L3idxS911, _M0L14_2acurr__entryS922);
        moonbit_decref(_M0L14_2acurr__entryS922);
        _M0L7_2abindS923 = _M0L4selfS913->$6;
        _M0L7_2abindS924 = 0;
        moonbit_incref(_M0L3keyS919);
        _M0L5entryS925
        = (struct _M0TPB5EntryGsfE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsfE));
        Moonbit_object_header(_M0L5entryS925)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 98, 0);
        _M0L5entryS925->$0 = _M0L7_2abindS923;
        _M0L5entryS925->$1 = _M0L7_2abindS924;
        _M0L5entryS925->$2 = _M0L3pslS910;
        _M0L5entryS925->$3 = _M0L4hashS915;
        _M0L5entryS925->$4 = _M0L3keyS919;
        _M0L5entryS925->$5 = _M0L5valueS920;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS913, _M0L3idxS911, _M0L5entryS925);
        moonbit_decref(_M0L5entryS925);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS922);
      }
      _M0L6_2atmpS3283 = _M0L3pslS910 + 1;
      _M0L6_2atmpS3285 = _M0L3idxS911 + 1;
      _M0L14capacity__maskS3286 = _M0L4selfS913->$3;
      _M0L6_2atmpS3284 = _M0L6_2atmpS3285 & _M0L14capacity__maskS3286;
      _M0L3pslS910 = _M0L6_2atmpS3283;
      _M0L3idxS911 = _M0L6_2atmpS3284;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS929,
  moonbit_string_t _M0L3keyS935,
  int32_t _M0L5valueS936,
  int32_t _M0L4hashS931
) {
  int32_t _M0L14capacity__maskS3307;
  int32_t _M0L6_2atmpS3306;
  int32_t _M0L3pslS926;
  int32_t _M0L3idxS927;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3307 = _M0L4selfS929->$3;
  _M0L6_2atmpS3306 = _M0L4hashS931 & _M0L14capacity__maskS3307;
  _M0L3pslS926 = 0;
  _M0L3idxS927 = _M0L6_2atmpS3306;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3305 = _M0L4selfS929->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS928;
    if (
      _M0L3idxS927 < 0
      || _M0L3idxS927 >= Moonbit_array_length(_M0L7entriesS3305)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS928
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3305[_M0L3idxS927];
    if (_M0L7_2abindS928 == 0) {
      int32_t _M0L4sizeS3290 = _M0L4selfS929->$1;
      int32_t _M0L8grow__atS3291 = _M0L4selfS929->$4;
      int32_t _M0L7_2abindS932;
      struct _M0TPB5EntryGsiE* _M0L7_2abindS933;
      struct _M0TPB5EntryGsiE* _M0L5entryS934;
      if (_M0L4sizeS3290 >= _M0L8grow__atS3291) {
        int32_t _M0L14capacity__maskS3293;
        int32_t _M0L6_2atmpS3292;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsiE(_M0L4selfS929);
        _M0L14capacity__maskS3293 = _M0L4selfS929->$3;
        _M0L6_2atmpS3292 = _M0L4hashS931 & _M0L14capacity__maskS3293;
        _M0L3pslS926 = 0;
        _M0L3idxS927 = _M0L6_2atmpS3292;
        continue;
      }
      _M0L7_2abindS932 = _M0L4selfS929->$6;
      _M0L7_2abindS933 = 0;
      moonbit_incref(_M0L3keyS935);
      _M0L5entryS934
      = (struct _M0TPB5EntryGsiE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsiE));
      Moonbit_object_header(_M0L5entryS934)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 102, 0);
      _M0L5entryS934->$0 = _M0L7_2abindS932;
      _M0L5entryS934->$1 = _M0L7_2abindS933;
      _M0L5entryS934->$2 = _M0L3pslS926;
      _M0L5entryS934->$3 = _M0L4hashS931;
      _M0L5entryS934->$4 = _M0L3keyS935;
      _M0L5entryS934->$5 = _M0L5valueS936;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS929, _M0L3idxS927, _M0L5entryS934);
      moonbit_decref(_M0L5entryS934);
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS937 = _M0L7_2abindS928;
      struct _M0TPB5EntryGsiE* _M0L14_2acurr__entryS938 = _M0L7_2aSomeS937;
      int32_t _M0L4hashS3295 = _M0L14_2acurr__entryS938->$3;
      int32_t _if__result_5617;
      int32_t _M0L3pslS3296;
      int32_t _M0L6_2atmpS3301;
      int32_t _M0L6_2atmpS3303;
      int32_t _M0L14capacity__maskS3304;
      int32_t _M0L6_2atmpS3302;
      if (_M0L4hashS3295 == _M0L4hashS931) {
        moonbit_string_t _M0L3keyS3294 = _M0L14_2acurr__entryS938->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_5617
        = _M0L3keyS3294 == _M0L3keyS935
          || Moonbit_array_length(_M0L3keyS3294)
             == Moonbit_array_length(_M0L3keyS935)
             && 0
                == memcmp(_M0L3keyS3294, _M0L3keyS935, Moonbit_array_length(_M0L3keyS3294) * 2);
      } else {
        _if__result_5617 = 0;
      }
      if (_if__result_5617) {
        _M0L14_2acurr__entryS938->$5 = _M0L5valueS936;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS938);
      }
      _M0L3pslS3296 = _M0L14_2acurr__entryS938->$2;
      if (_M0L3pslS926 > _M0L3pslS3296) {
        int32_t _M0L4sizeS3297 = _M0L4selfS929->$1;
        int32_t _M0L8grow__atS3298 = _M0L4selfS929->$4;
        int32_t _M0L7_2abindS939;
        struct _M0TPB5EntryGsiE* _M0L7_2abindS940;
        struct _M0TPB5EntryGsiE* _M0L5entryS941;
        if (_M0L4sizeS3297 >= _M0L8grow__atS3298) {
          int32_t _M0L14capacity__maskS3300;
          int32_t _M0L6_2atmpS3299;
          moonbit_decref(_M0L14_2acurr__entryS938);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsiE(_M0L4selfS929);
          _M0L14capacity__maskS3300 = _M0L4selfS929->$3;
          _M0L6_2atmpS3299 = _M0L4hashS931 & _M0L14capacity__maskS3300;
          _M0L3pslS926 = 0;
          _M0L3idxS927 = _M0L6_2atmpS3299;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsiE(_M0L4selfS929, _M0L3idxS927, _M0L14_2acurr__entryS938);
        moonbit_decref(_M0L14_2acurr__entryS938);
        _M0L7_2abindS939 = _M0L4selfS929->$6;
        _M0L7_2abindS940 = 0;
        moonbit_incref(_M0L3keyS935);
        _M0L5entryS941
        = (struct _M0TPB5EntryGsiE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsiE));
        Moonbit_object_header(_M0L5entryS941)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 102, 0);
        _M0L5entryS941->$0 = _M0L7_2abindS939;
        _M0L5entryS941->$1 = _M0L7_2abindS940;
        _M0L5entryS941->$2 = _M0L3pslS926;
        _M0L5entryS941->$3 = _M0L4hashS931;
        _M0L5entryS941->$4 = _M0L3keyS935;
        _M0L5entryS941->$5 = _M0L5valueS936;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS929, _M0L3idxS927, _M0L5entryS941);
        moonbit_decref(_M0L5entryS941);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS938);
      }
      _M0L6_2atmpS3301 = _M0L3pslS926 + 1;
      _M0L6_2atmpS3303 = _M0L3idxS927 + 1;
      _M0L14capacity__maskS3304 = _M0L4selfS929->$3;
      _M0L6_2atmpS3302 = _M0L6_2atmpS3303 & _M0L14capacity__maskS3304;
      _M0L3pslS926 = _M0L6_2atmpS3301;
      _M0L3idxS927 = _M0L6_2atmpS3302;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS823
) {
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L9old__headS822;
  int32_t _M0L8capacityS3185;
  int32_t _M0L13new__capacityS824;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3179;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L6_2atmpS3178;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L6_2aoldS4980;
  int32_t _M0L6_2atmpS3180;
  int32_t _M0L8capacityS3182;
  int32_t _M0L6_2atmpS3181;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3183;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4979;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L1xS825;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS822 = _M0L4selfS823->$5;
  _M0L8capacityS3185 = _M0L4selfS823->$2;
  _M0L13new__capacityS824 = _M0L8capacityS3185 << 1;
  _M0L6_2atmpS3179 = 0;
  _M0L6_2atmpS3178
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE**)moonbit_make_ref_array(_M0L13new__capacityS824, _M0L6_2atmpS3179);
  _M0L6_2aoldS4980 = _M0L4selfS823->$0;
  if (_M0L9old__headS822) {
    moonbit_incref(_M0L9old__headS822);
  }
  moonbit_decref(_M0L6_2aoldS4980);
  _M0L4selfS823->$0 = _M0L6_2atmpS3178;
  _M0L4selfS823->$2 = _M0L13new__capacityS824;
  _M0L6_2atmpS3180 = _M0L13new__capacityS824 - 1;
  _M0L4selfS823->$3 = _M0L6_2atmpS3180;
  _M0L8capacityS3182 = _M0L4selfS823->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3181 = _M0FPB21calc__grow__threshold(_M0L8capacityS3182);
  _M0L4selfS823->$4 = _M0L6_2atmpS3181;
  _M0L4selfS823->$1 = 0;
  _M0L6_2atmpS3183 = 0;
  _M0L6_2aoldS4979 = _M0L4selfS823->$5;
  if (_M0L6_2aoldS4979) {
    moonbit_decref(_M0L6_2aoldS4979);
  }
  _M0L4selfS823->$5 = _M0L6_2atmpS3183;
  _M0L4selfS823->$6 = -1;
  _M0L1xS825 = _M0L9old__headS822;
  while (1) {
    if (_M0L1xS825 == 0) {
      if (_M0L1xS825) {
        moonbit_decref(_M0L1xS825);
      }
      break;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS827 =
        _M0L1xS825;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4_2aeS828 =
        _M0L7_2aSomeS827;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L15next__in__chainS829 =
        _M0L4_2aeS828->$1;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3184 = 0;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4977 =
        _M0L4_2aeS828->$1;
      if (_M0L15next__in__chainS829) {
        moonbit_incref(_M0L15next__in__chainS829);
      }
      if (_M0L6_2aoldS4977) {
        moonbit_decref(_M0L6_2aoldS4977);
      }
      _M0L4_2aeS828->$1 = _M0L6_2atmpS3184;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS823, _M0L4_2aeS828);
      moonbit_decref(_M0L4_2aeS828);
      _M0L1xS825 = _M0L15next__in__chainS829;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE* _M0L4selfS831) {
  struct _M0TPB5EntryGssE* _M0L9old__headS830;
  int32_t _M0L8capacityS3193;
  int32_t _M0L13new__capacityS832;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS3187;
  struct _M0TPB5EntryGssE** _M0L6_2atmpS3186;
  struct _M0TPB5EntryGssE** _M0L6_2aoldS4985;
  int32_t _M0L6_2atmpS3188;
  int32_t _M0L8capacityS3190;
  int32_t _M0L6_2atmpS3189;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS3191;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS4984;
  struct _M0TPB5EntryGssE* _M0L1xS833;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS830 = _M0L4selfS831->$5;
  _M0L8capacityS3193 = _M0L4selfS831->$2;
  _M0L13new__capacityS832 = _M0L8capacityS3193 << 1;
  _M0L6_2atmpS3187 = 0;
  _M0L6_2atmpS3186
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L13new__capacityS832, _M0L6_2atmpS3187);
  _M0L6_2aoldS4985 = _M0L4selfS831->$0;
  if (_M0L9old__headS830) {
    moonbit_incref(_M0L9old__headS830);
  }
  moonbit_decref(_M0L6_2aoldS4985);
  _M0L4selfS831->$0 = _M0L6_2atmpS3186;
  _M0L4selfS831->$2 = _M0L13new__capacityS832;
  _M0L6_2atmpS3188 = _M0L13new__capacityS832 - 1;
  _M0L4selfS831->$3 = _M0L6_2atmpS3188;
  _M0L8capacityS3190 = _M0L4selfS831->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3189 = _M0FPB21calc__grow__threshold(_M0L8capacityS3190);
  _M0L4selfS831->$4 = _M0L6_2atmpS3189;
  _M0L4selfS831->$1 = 0;
  _M0L6_2atmpS3191 = 0;
  _M0L6_2aoldS4984 = _M0L4selfS831->$5;
  if (_M0L6_2aoldS4984) {
    moonbit_decref(_M0L6_2aoldS4984);
  }
  _M0L4selfS831->$5 = _M0L6_2atmpS3191;
  _M0L4selfS831->$6 = -1;
  _M0L1xS833 = _M0L9old__headS830;
  while (1) {
    if (_M0L1xS833 == 0) {
      if (_M0L1xS833) {
        moonbit_decref(_M0L1xS833);
      }
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS835 = _M0L1xS833;
      struct _M0TPB5EntryGssE* _M0L4_2aeS836 = _M0L7_2aSomeS835;
      struct _M0TPB5EntryGssE* _M0L15next__in__chainS837 = _M0L4_2aeS836->$1;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS3192 = 0;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4982 = _M0L4_2aeS836->$1;
      if (_M0L15next__in__chainS837) {
        moonbit_incref(_M0L15next__in__chainS837);
      }
      if (_M0L6_2aoldS4982) {
        moonbit_decref(_M0L6_2aoldS4982);
      }
      _M0L4_2aeS836->$1 = _M0L6_2atmpS3192;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGssE(_M0L4selfS831, _M0L4_2aeS836);
      moonbit_decref(_M0L4_2aeS836);
      _M0L1xS833 = _M0L15next__in__chainS837;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsbE(struct _M0TPB3MapGsbE* _M0L4selfS839) {
  struct _M0TPB5EntryGsbE* _M0L9old__headS838;
  int32_t _M0L8capacityS3201;
  int32_t _M0L13new__capacityS840;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS3195;
  struct _M0TPB5EntryGsbE** _M0L6_2atmpS3194;
  struct _M0TPB5EntryGsbE** _M0L6_2aoldS4990;
  int32_t _M0L6_2atmpS3196;
  int32_t _M0L8capacityS3198;
  int32_t _M0L6_2atmpS3197;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS3199;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS4989;
  struct _M0TPB5EntryGsbE* _M0L1xS841;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS838 = _M0L4selfS839->$5;
  _M0L8capacityS3201 = _M0L4selfS839->$2;
  _M0L13new__capacityS840 = _M0L8capacityS3201 << 1;
  _M0L6_2atmpS3195 = 0;
  _M0L6_2atmpS3194
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L13new__capacityS840, _M0L6_2atmpS3195);
  _M0L6_2aoldS4990 = _M0L4selfS839->$0;
  if (_M0L9old__headS838) {
    moonbit_incref(_M0L9old__headS838);
  }
  moonbit_decref(_M0L6_2aoldS4990);
  _M0L4selfS839->$0 = _M0L6_2atmpS3194;
  _M0L4selfS839->$2 = _M0L13new__capacityS840;
  _M0L6_2atmpS3196 = _M0L13new__capacityS840 - 1;
  _M0L4selfS839->$3 = _M0L6_2atmpS3196;
  _M0L8capacityS3198 = _M0L4selfS839->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3197 = _M0FPB21calc__grow__threshold(_M0L8capacityS3198);
  _M0L4selfS839->$4 = _M0L6_2atmpS3197;
  _M0L4selfS839->$1 = 0;
  _M0L6_2atmpS3199 = 0;
  _M0L6_2aoldS4989 = _M0L4selfS839->$5;
  if (_M0L6_2aoldS4989) {
    moonbit_decref(_M0L6_2aoldS4989);
  }
  _M0L4selfS839->$5 = _M0L6_2atmpS3199;
  _M0L4selfS839->$6 = -1;
  _M0L1xS841 = _M0L9old__headS838;
  while (1) {
    if (_M0L1xS841 == 0) {
      if (_M0L1xS841) {
        moonbit_decref(_M0L1xS841);
      }
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS843 = _M0L1xS841;
      struct _M0TPB5EntryGsbE* _M0L4_2aeS844 = _M0L7_2aSomeS843;
      struct _M0TPB5EntryGsbE* _M0L15next__in__chainS845 = _M0L4_2aeS844->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS3200 = 0;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4987 = _M0L4_2aeS844->$1;
      if (_M0L15next__in__chainS845) {
        moonbit_incref(_M0L15next__in__chainS845);
      }
      if (_M0L6_2aoldS4987) {
        moonbit_decref(_M0L6_2aoldS4987);
      }
      _M0L4_2aeS844->$1 = _M0L6_2atmpS3200;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsbE(_M0L4selfS839, _M0L4_2aeS844);
      moonbit_decref(_M0L4_2aeS844);
      _M0L1xS841 = _M0L15next__in__chainS845;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsfE(struct _M0TPB3MapGsfE* _M0L4selfS847) {
  struct _M0TPB5EntryGsfE* _M0L9old__headS846;
  int32_t _M0L8capacityS3209;
  int32_t _M0L13new__capacityS848;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS3203;
  struct _M0TPB5EntryGsfE** _M0L6_2atmpS3202;
  struct _M0TPB5EntryGsfE** _M0L6_2aoldS4995;
  int32_t _M0L6_2atmpS3204;
  int32_t _M0L8capacityS3206;
  int32_t _M0L6_2atmpS3205;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS3207;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS4994;
  struct _M0TPB5EntryGsfE* _M0L1xS849;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS846 = _M0L4selfS847->$5;
  _M0L8capacityS3209 = _M0L4selfS847->$2;
  _M0L13new__capacityS848 = _M0L8capacityS3209 << 1;
  _M0L6_2atmpS3203 = 0;
  _M0L6_2atmpS3202
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L13new__capacityS848, _M0L6_2atmpS3203);
  _M0L6_2aoldS4995 = _M0L4selfS847->$0;
  if (_M0L9old__headS846) {
    moonbit_incref(_M0L9old__headS846);
  }
  moonbit_decref(_M0L6_2aoldS4995);
  _M0L4selfS847->$0 = _M0L6_2atmpS3202;
  _M0L4selfS847->$2 = _M0L13new__capacityS848;
  _M0L6_2atmpS3204 = _M0L13new__capacityS848 - 1;
  _M0L4selfS847->$3 = _M0L6_2atmpS3204;
  _M0L8capacityS3206 = _M0L4selfS847->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3205 = _M0FPB21calc__grow__threshold(_M0L8capacityS3206);
  _M0L4selfS847->$4 = _M0L6_2atmpS3205;
  _M0L4selfS847->$1 = 0;
  _M0L6_2atmpS3207 = 0;
  _M0L6_2aoldS4994 = _M0L4selfS847->$5;
  if (_M0L6_2aoldS4994) {
    moonbit_decref(_M0L6_2aoldS4994);
  }
  _M0L4selfS847->$5 = _M0L6_2atmpS3207;
  _M0L4selfS847->$6 = -1;
  _M0L1xS849 = _M0L9old__headS846;
  while (1) {
    if (_M0L1xS849 == 0) {
      if (_M0L1xS849) {
        moonbit_decref(_M0L1xS849);
      }
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS851 = _M0L1xS849;
      struct _M0TPB5EntryGsfE* _M0L4_2aeS852 = _M0L7_2aSomeS851;
      struct _M0TPB5EntryGsfE* _M0L15next__in__chainS853 = _M0L4_2aeS852->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS3208 = 0;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4992 = _M0L4_2aeS852->$1;
      if (_M0L15next__in__chainS853) {
        moonbit_incref(_M0L15next__in__chainS853);
      }
      if (_M0L6_2aoldS4992) {
        moonbit_decref(_M0L6_2aoldS4992);
      }
      _M0L4_2aeS852->$1 = _M0L6_2atmpS3208;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsfE(_M0L4selfS847, _M0L4_2aeS852);
      moonbit_decref(_M0L4_2aeS852);
      _M0L1xS849 = _M0L15next__in__chainS853;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsiE(struct _M0TPB3MapGsiE* _M0L4selfS855) {
  struct _M0TPB5EntryGsiE* _M0L9old__headS854;
  int32_t _M0L8capacityS3217;
  int32_t _M0L13new__capacityS856;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS3211;
  struct _M0TPB5EntryGsiE** _M0L6_2atmpS3210;
  struct _M0TPB5EntryGsiE** _M0L6_2aoldS5000;
  int32_t _M0L6_2atmpS3212;
  int32_t _M0L8capacityS3214;
  int32_t _M0L6_2atmpS3213;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS3215;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS4999;
  struct _M0TPB5EntryGsiE* _M0L1xS857;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS854 = _M0L4selfS855->$5;
  _M0L8capacityS3217 = _M0L4selfS855->$2;
  _M0L13new__capacityS856 = _M0L8capacityS3217 << 1;
  _M0L6_2atmpS3211 = 0;
  _M0L6_2atmpS3210
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L13new__capacityS856, _M0L6_2atmpS3211);
  _M0L6_2aoldS5000 = _M0L4selfS855->$0;
  if (_M0L9old__headS854) {
    moonbit_incref(_M0L9old__headS854);
  }
  moonbit_decref(_M0L6_2aoldS5000);
  _M0L4selfS855->$0 = _M0L6_2atmpS3210;
  _M0L4selfS855->$2 = _M0L13new__capacityS856;
  _M0L6_2atmpS3212 = _M0L13new__capacityS856 - 1;
  _M0L4selfS855->$3 = _M0L6_2atmpS3212;
  _M0L8capacityS3214 = _M0L4selfS855->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3213 = _M0FPB21calc__grow__threshold(_M0L8capacityS3214);
  _M0L4selfS855->$4 = _M0L6_2atmpS3213;
  _M0L4selfS855->$1 = 0;
  _M0L6_2atmpS3215 = 0;
  _M0L6_2aoldS4999 = _M0L4selfS855->$5;
  if (_M0L6_2aoldS4999) {
    moonbit_decref(_M0L6_2aoldS4999);
  }
  _M0L4selfS855->$5 = _M0L6_2atmpS3215;
  _M0L4selfS855->$6 = -1;
  _M0L1xS857 = _M0L9old__headS854;
  while (1) {
    if (_M0L1xS857 == 0) {
      if (_M0L1xS857) {
        moonbit_decref(_M0L1xS857);
      }
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS859 = _M0L1xS857;
      struct _M0TPB5EntryGsiE* _M0L4_2aeS860 = _M0L7_2aSomeS859;
      struct _M0TPB5EntryGsiE* _M0L15next__in__chainS861 = _M0L4_2aeS860->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS3216 = 0;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4997 = _M0L4_2aeS860->$1;
      if (_M0L15next__in__chainS861) {
        moonbit_incref(_M0L15next__in__chainS861);
      }
      if (_M0L6_2aoldS4997) {
        moonbit_decref(_M0L6_2aoldS4997);
      }
      _M0L4_2aeS860->$1 = _M0L6_2atmpS3216;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsiE(_M0L4selfS855, _M0L4_2aeS860);
      moonbit_decref(_M0L4_2aeS860);
      _M0L1xS857 = _M0L15next__in__chainS861;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS782,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5outerS778
) {
  int32_t _M0L4hashS777;
  int32_t _M0L14capacity__maskS3137;
  int32_t _M0L6_2atmpS3136;
  int32_t _M0L3pslS779;
  int32_t _M0L3idxS780;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS777 = _M0L5outerS778->$3;
  _M0L14capacity__maskS3137 = _M0L4selfS782->$3;
  _M0L6_2atmpS3136 = _M0L4hashS777 & _M0L14capacity__maskS3137;
  _M0L3pslS779 = 0;
  _M0L3idxS780 = _M0L6_2atmpS3136;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3135 =
      _M0L4selfS782->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS781;
    if (
      _M0L3idxS780 < 0
      || _M0L3idxS780 >= Moonbit_array_length(_M0L7entriesS3135)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS781
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3135[
        _M0L3idxS780
      ];
    if (_M0L7_2abindS781 == 0) {
      int32_t _M0L4tailS3128;
      _M0L5outerS778->$2 = _M0L3pslS779;
      _M0L4tailS3128 = _M0L4selfS782->$6;
      _M0L5outerS778->$0 = _M0L4tailS3128;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(_M0L4selfS782, _M0L3idxS780, _M0L5outerS778);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS783 =
        _M0L7_2abindS781;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2acurrS784 =
        _M0L7_2aSomeS783;
      int32_t _M0L3pslS3129 = _M0L7_2acurrS784->$2;
      if (_M0L3pslS779 > _M0L3pslS3129) {
        int32_t _M0L4tailS3130;
        moonbit_incref(_M0L7_2acurrS784);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP19moonbitDB10RedisValueE(_M0L4selfS782, _M0L3idxS780, _M0L7_2acurrS784);
        moonbit_decref(_M0L7_2acurrS784);
        _M0L5outerS778->$2 = _M0L3pslS779;
        _M0L4tailS3130 = _M0L4selfS782->$6;
        _M0L5outerS778->$0 = _M0L4tailS3130;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(_M0L4selfS782, _M0L3idxS780, _M0L5outerS778);
        return 0;
      } else {
        int32_t _M0L6_2atmpS3131 = _M0L3pslS779 + 1;
        int32_t _M0L6_2atmpS3133 = _M0L3idxS780 + 1;
        int32_t _M0L14capacity__maskS3134 = _M0L4selfS782->$3;
        int32_t _M0L6_2atmpS3132 =
          _M0L6_2atmpS3133 & _M0L14capacity__maskS3134;
        _M0L3pslS779 = _M0L6_2atmpS3131;
        _M0L3idxS780 = _M0L6_2atmpS3132;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS791,
  struct _M0TPB5EntryGssE* _M0L5outerS787
) {
  int32_t _M0L4hashS786;
  int32_t _M0L14capacity__maskS3147;
  int32_t _M0L6_2atmpS3146;
  int32_t _M0L3pslS788;
  int32_t _M0L3idxS789;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS786 = _M0L5outerS787->$3;
  _M0L14capacity__maskS3147 = _M0L4selfS791->$3;
  _M0L6_2atmpS3146 = _M0L4hashS786 & _M0L14capacity__maskS3147;
  _M0L3pslS788 = 0;
  _M0L3idxS789 = _M0L6_2atmpS3146;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS3145 = _M0L4selfS791->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS790;
    if (
      _M0L3idxS789 < 0
      || _M0L3idxS789 >= Moonbit_array_length(_M0L7entriesS3145)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS790
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3145[_M0L3idxS789];
    if (_M0L7_2abindS790 == 0) {
      int32_t _M0L4tailS3138;
      _M0L5outerS787->$2 = _M0L3pslS788;
      _M0L4tailS3138 = _M0L4selfS791->$6;
      _M0L5outerS787->$0 = _M0L4tailS3138;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS791, _M0L3idxS789, _M0L5outerS787);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS792 = _M0L7_2abindS790;
      struct _M0TPB5EntryGssE* _M0L7_2acurrS793 = _M0L7_2aSomeS792;
      int32_t _M0L3pslS3139 = _M0L7_2acurrS793->$2;
      if (_M0L3pslS788 > _M0L3pslS3139) {
        int32_t _M0L4tailS3140;
        moonbit_incref(_M0L7_2acurrS793);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS791, _M0L3idxS789, _M0L7_2acurrS793);
        moonbit_decref(_M0L7_2acurrS793);
        _M0L5outerS787->$2 = _M0L3pslS788;
        _M0L4tailS3140 = _M0L4selfS791->$6;
        _M0L5outerS787->$0 = _M0L4tailS3140;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS791, _M0L3idxS789, _M0L5outerS787);
        return 0;
      } else {
        int32_t _M0L6_2atmpS3141 = _M0L3pslS788 + 1;
        int32_t _M0L6_2atmpS3143 = _M0L3idxS789 + 1;
        int32_t _M0L14capacity__maskS3144 = _M0L4selfS791->$3;
        int32_t _M0L6_2atmpS3142 =
          _M0L6_2atmpS3143 & _M0L14capacity__maskS3144;
        _M0L3pslS788 = _M0L6_2atmpS3141;
        _M0L3idxS789 = _M0L6_2atmpS3142;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS800,
  struct _M0TPB5EntryGsbE* _M0L5outerS796
) {
  int32_t _M0L4hashS795;
  int32_t _M0L14capacity__maskS3157;
  int32_t _M0L6_2atmpS3156;
  int32_t _M0L3pslS797;
  int32_t _M0L3idxS798;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS795 = _M0L5outerS796->$3;
  _M0L14capacity__maskS3157 = _M0L4selfS800->$3;
  _M0L6_2atmpS3156 = _M0L4hashS795 & _M0L14capacity__maskS3157;
  _M0L3pslS797 = 0;
  _M0L3idxS798 = _M0L6_2atmpS3156;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS3155 = _M0L4selfS800->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS799;
    if (
      _M0L3idxS798 < 0
      || _M0L3idxS798 >= Moonbit_array_length(_M0L7entriesS3155)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS799
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3155[_M0L3idxS798];
    if (_M0L7_2abindS799 == 0) {
      int32_t _M0L4tailS3148;
      _M0L5outerS796->$2 = _M0L3pslS797;
      _M0L4tailS3148 = _M0L4selfS800->$6;
      _M0L5outerS796->$0 = _M0L4tailS3148;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS800, _M0L3idxS798, _M0L5outerS796);
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS801 = _M0L7_2abindS799;
      struct _M0TPB5EntryGsbE* _M0L7_2acurrS802 = _M0L7_2aSomeS801;
      int32_t _M0L3pslS3149 = _M0L7_2acurrS802->$2;
      if (_M0L3pslS797 > _M0L3pslS3149) {
        int32_t _M0L4tailS3150;
        moonbit_incref(_M0L7_2acurrS802);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsbE(_M0L4selfS800, _M0L3idxS798, _M0L7_2acurrS802);
        moonbit_decref(_M0L7_2acurrS802);
        _M0L5outerS796->$2 = _M0L3pslS797;
        _M0L4tailS3150 = _M0L4selfS800->$6;
        _M0L5outerS796->$0 = _M0L4tailS3150;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS800, _M0L3idxS798, _M0L5outerS796);
        return 0;
      } else {
        int32_t _M0L6_2atmpS3151 = _M0L3pslS797 + 1;
        int32_t _M0L6_2atmpS3153 = _M0L3idxS798 + 1;
        int32_t _M0L14capacity__maskS3154 = _M0L4selfS800->$3;
        int32_t _M0L6_2atmpS3152 =
          _M0L6_2atmpS3153 & _M0L14capacity__maskS3154;
        _M0L3pslS797 = _M0L6_2atmpS3151;
        _M0L3idxS798 = _M0L6_2atmpS3152;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS809,
  struct _M0TPB5EntryGsfE* _M0L5outerS805
) {
  int32_t _M0L4hashS804;
  int32_t _M0L14capacity__maskS3167;
  int32_t _M0L6_2atmpS3166;
  int32_t _M0L3pslS806;
  int32_t _M0L3idxS807;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS804 = _M0L5outerS805->$3;
  _M0L14capacity__maskS3167 = _M0L4selfS809->$3;
  _M0L6_2atmpS3166 = _M0L4hashS804 & _M0L14capacity__maskS3167;
  _M0L3pslS806 = 0;
  _M0L3idxS807 = _M0L6_2atmpS3166;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS3165 = _M0L4selfS809->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS808;
    if (
      _M0L3idxS807 < 0
      || _M0L3idxS807 >= Moonbit_array_length(_M0L7entriesS3165)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS808
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3165[_M0L3idxS807];
    if (_M0L7_2abindS808 == 0) {
      int32_t _M0L4tailS3158;
      _M0L5outerS805->$2 = _M0L3pslS806;
      _M0L4tailS3158 = _M0L4selfS809->$6;
      _M0L5outerS805->$0 = _M0L4tailS3158;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS809, _M0L3idxS807, _M0L5outerS805);
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS810 = _M0L7_2abindS808;
      struct _M0TPB5EntryGsfE* _M0L7_2acurrS811 = _M0L7_2aSomeS810;
      int32_t _M0L3pslS3159 = _M0L7_2acurrS811->$2;
      if (_M0L3pslS806 > _M0L3pslS3159) {
        int32_t _M0L4tailS3160;
        moonbit_incref(_M0L7_2acurrS811);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsfE(_M0L4selfS809, _M0L3idxS807, _M0L7_2acurrS811);
        moonbit_decref(_M0L7_2acurrS811);
        _M0L5outerS805->$2 = _M0L3pslS806;
        _M0L4tailS3160 = _M0L4selfS809->$6;
        _M0L5outerS805->$0 = _M0L4tailS3160;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS809, _M0L3idxS807, _M0L5outerS805);
        return 0;
      } else {
        int32_t _M0L6_2atmpS3161 = _M0L3pslS806 + 1;
        int32_t _M0L6_2atmpS3163 = _M0L3idxS807 + 1;
        int32_t _M0L14capacity__maskS3164 = _M0L4selfS809->$3;
        int32_t _M0L6_2atmpS3162 =
          _M0L6_2atmpS3163 & _M0L14capacity__maskS3164;
        _M0L3pslS806 = _M0L6_2atmpS3161;
        _M0L3idxS807 = _M0L6_2atmpS3162;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS818,
  struct _M0TPB5EntryGsiE* _M0L5outerS814
) {
  int32_t _M0L4hashS813;
  int32_t _M0L14capacity__maskS3177;
  int32_t _M0L6_2atmpS3176;
  int32_t _M0L3pslS815;
  int32_t _M0L3idxS816;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS813 = _M0L5outerS814->$3;
  _M0L14capacity__maskS3177 = _M0L4selfS818->$3;
  _M0L6_2atmpS3176 = _M0L4hashS813 & _M0L14capacity__maskS3177;
  _M0L3pslS815 = 0;
  _M0L3idxS816 = _M0L6_2atmpS3176;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3175 = _M0L4selfS818->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS817;
    if (
      _M0L3idxS816 < 0
      || _M0L3idxS816 >= Moonbit_array_length(_M0L7entriesS3175)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS817
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3175[_M0L3idxS816];
    if (_M0L7_2abindS817 == 0) {
      int32_t _M0L4tailS3168;
      _M0L5outerS814->$2 = _M0L3pslS815;
      _M0L4tailS3168 = _M0L4selfS818->$6;
      _M0L5outerS814->$0 = _M0L4tailS3168;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS818, _M0L3idxS816, _M0L5outerS814);
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS819 = _M0L7_2abindS817;
      struct _M0TPB5EntryGsiE* _M0L7_2acurrS820 = _M0L7_2aSomeS819;
      int32_t _M0L3pslS3169 = _M0L7_2acurrS820->$2;
      if (_M0L3pslS815 > _M0L3pslS3169) {
        int32_t _M0L4tailS3170;
        moonbit_incref(_M0L7_2acurrS820);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsiE(_M0L4selfS818, _M0L3idxS816, _M0L7_2acurrS820);
        moonbit_decref(_M0L7_2acurrS820);
        _M0L5outerS814->$2 = _M0L3pslS815;
        _M0L4tailS3170 = _M0L4selfS818->$6;
        _M0L5outerS814->$0 = _M0L4tailS3170;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS818, _M0L3idxS816, _M0L5outerS814);
        return 0;
      } else {
        int32_t _M0L6_2atmpS3171 = _M0L3pslS815 + 1;
        int32_t _M0L6_2atmpS3173 = _M0L3idxS816 + 1;
        int32_t _M0L14capacity__maskS3174 = _M0L4selfS818->$3;
        int32_t _M0L6_2atmpS3172 =
          _M0L6_2atmpS3173 & _M0L14capacity__maskS3174;
        _M0L3pslS815 = _M0L6_2atmpS3171;
        _M0L3idxS816 = _M0L6_2atmpS3172;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS731,
  int32_t _M0L3idxS736,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS735
) {
  int32_t _M0L3pslS3063;
  int32_t _M0L6_2atmpS3059;
  int32_t _M0L6_2atmpS3061;
  int32_t _M0L14capacity__maskS3062;
  int32_t _M0L6_2atmpS3060;
  int32_t _M0L3pslS727;
  int32_t _M0L3idxS728;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS729;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS3063 = _M0L5entryS735->$2;
  _M0L6_2atmpS3059 = _M0L3pslS3063 + 1;
  _M0L6_2atmpS3061 = _M0L3idxS736 + 1;
  _M0L14capacity__maskS3062 = _M0L4selfS731->$3;
  _M0L6_2atmpS3060 = _M0L6_2atmpS3061 & _M0L14capacity__maskS3062;
  moonbit_incref(_M0L5entryS735);
  _M0L3pslS727 = _M0L6_2atmpS3059;
  _M0L3idxS728 = _M0L6_2atmpS3060;
  _M0L5entryS729 = _M0L5entryS735;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3058 =
      _M0L4selfS731->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS730;
    if (
      _M0L3idxS728 < 0
      || _M0L3idxS728 >= Moonbit_array_length(_M0L7entriesS3058)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS730
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3058[
        _M0L3idxS728
      ];
    if (_M0L7_2abindS730 == 0) {
      _M0L5entryS729->$2 = _M0L3pslS727;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS731, _M0L5entryS729, _M0L3idxS728);
      moonbit_decref(_M0L5entryS729);
      break;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS733 =
        _M0L7_2abindS730;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L14_2acurr__entryS734 =
        _M0L7_2aSomeS733;
      int32_t _M0L3pslS3048 = _M0L14_2acurr__entryS734->$2;
      if (_M0L3pslS727 > _M0L3pslS3048) {
        int32_t _M0L3pslS3053;
        int32_t _M0L6_2atmpS3049;
        int32_t _M0L6_2atmpS3051;
        int32_t _M0L14capacity__maskS3052;
        int32_t _M0L6_2atmpS3050;
        _M0L5entryS729->$2 = _M0L3pslS727;
        moonbit_incref(_M0L14_2acurr__entryS734);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS731, _M0L5entryS729, _M0L3idxS728);
        moonbit_decref(_M0L5entryS729);
        _M0L3pslS3053 = _M0L14_2acurr__entryS734->$2;
        _M0L6_2atmpS3049 = _M0L3pslS3053 + 1;
        _M0L6_2atmpS3051 = _M0L3idxS728 + 1;
        _M0L14capacity__maskS3052 = _M0L4selfS731->$3;
        _M0L6_2atmpS3050 = _M0L6_2atmpS3051 & _M0L14capacity__maskS3052;
        _M0L3pslS727 = _M0L6_2atmpS3049;
        _M0L3idxS728 = _M0L6_2atmpS3050;
        _M0L5entryS729 = _M0L14_2acurr__entryS734;
        continue;
      } else {
        int32_t _M0L6_2atmpS3054 = _M0L3pslS727 + 1;
        int32_t _M0L6_2atmpS3056 = _M0L3idxS728 + 1;
        int32_t _M0L14capacity__maskS3057 = _M0L4selfS731->$3;
        int32_t _M0L6_2atmpS3055 =
          _M0L6_2atmpS3056 & _M0L14capacity__maskS3057;
        struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _tmp_5629 =
          _M0L5entryS729;
        _M0L3pslS727 = _M0L6_2atmpS3054;
        _M0L3idxS728 = _M0L6_2atmpS3055;
        _M0L5entryS729 = _tmp_5629;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGssE(
  struct _M0TPB3MapGssE* _M0L4selfS741,
  int32_t _M0L3idxS746,
  struct _M0TPB5EntryGssE* _M0L5entryS745
) {
  int32_t _M0L3pslS3079;
  int32_t _M0L6_2atmpS3075;
  int32_t _M0L6_2atmpS3077;
  int32_t _M0L14capacity__maskS3078;
  int32_t _M0L6_2atmpS3076;
  int32_t _M0L3pslS737;
  int32_t _M0L3idxS738;
  struct _M0TPB5EntryGssE* _M0L5entryS739;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS3079 = _M0L5entryS745->$2;
  _M0L6_2atmpS3075 = _M0L3pslS3079 + 1;
  _M0L6_2atmpS3077 = _M0L3idxS746 + 1;
  _M0L14capacity__maskS3078 = _M0L4selfS741->$3;
  _M0L6_2atmpS3076 = _M0L6_2atmpS3077 & _M0L14capacity__maskS3078;
  moonbit_incref(_M0L5entryS745);
  _M0L3pslS737 = _M0L6_2atmpS3075;
  _M0L3idxS738 = _M0L6_2atmpS3076;
  _M0L5entryS739 = _M0L5entryS745;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS3074 = _M0L4selfS741->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS740;
    if (
      _M0L3idxS738 < 0
      || _M0L3idxS738 >= Moonbit_array_length(_M0L7entriesS3074)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS740
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3074[_M0L3idxS738];
    if (_M0L7_2abindS740 == 0) {
      _M0L5entryS739->$2 = _M0L3pslS737;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGssE(_M0L4selfS741, _M0L5entryS739, _M0L3idxS738);
      moonbit_decref(_M0L5entryS739);
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS743 = _M0L7_2abindS740;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS744 = _M0L7_2aSomeS743;
      int32_t _M0L3pslS3064 = _M0L14_2acurr__entryS744->$2;
      if (_M0L3pslS737 > _M0L3pslS3064) {
        int32_t _M0L3pslS3069;
        int32_t _M0L6_2atmpS3065;
        int32_t _M0L6_2atmpS3067;
        int32_t _M0L14capacity__maskS3068;
        int32_t _M0L6_2atmpS3066;
        _M0L5entryS739->$2 = _M0L3pslS737;
        moonbit_incref(_M0L14_2acurr__entryS744);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGssE(_M0L4selfS741, _M0L5entryS739, _M0L3idxS738);
        moonbit_decref(_M0L5entryS739);
        _M0L3pslS3069 = _M0L14_2acurr__entryS744->$2;
        _M0L6_2atmpS3065 = _M0L3pslS3069 + 1;
        _M0L6_2atmpS3067 = _M0L3idxS738 + 1;
        _M0L14capacity__maskS3068 = _M0L4selfS741->$3;
        _M0L6_2atmpS3066 = _M0L6_2atmpS3067 & _M0L14capacity__maskS3068;
        _M0L3pslS737 = _M0L6_2atmpS3065;
        _M0L3idxS738 = _M0L6_2atmpS3066;
        _M0L5entryS739 = _M0L14_2acurr__entryS744;
        continue;
      } else {
        int32_t _M0L6_2atmpS3070 = _M0L3pslS737 + 1;
        int32_t _M0L6_2atmpS3072 = _M0L3idxS738 + 1;
        int32_t _M0L14capacity__maskS3073 = _M0L4selfS741->$3;
        int32_t _M0L6_2atmpS3071 =
          _M0L6_2atmpS3072 & _M0L14capacity__maskS3073;
        struct _M0TPB5EntryGssE* _tmp_5631 = _M0L5entryS739;
        _M0L3pslS737 = _M0L6_2atmpS3070;
        _M0L3idxS738 = _M0L6_2atmpS3071;
        _M0L5entryS739 = _tmp_5631;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS751,
  int32_t _M0L3idxS756,
  struct _M0TPB5EntryGsbE* _M0L5entryS755
) {
  int32_t _M0L3pslS3095;
  int32_t _M0L6_2atmpS3091;
  int32_t _M0L6_2atmpS3093;
  int32_t _M0L14capacity__maskS3094;
  int32_t _M0L6_2atmpS3092;
  int32_t _M0L3pslS747;
  int32_t _M0L3idxS748;
  struct _M0TPB5EntryGsbE* _M0L5entryS749;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS3095 = _M0L5entryS755->$2;
  _M0L6_2atmpS3091 = _M0L3pslS3095 + 1;
  _M0L6_2atmpS3093 = _M0L3idxS756 + 1;
  _M0L14capacity__maskS3094 = _M0L4selfS751->$3;
  _M0L6_2atmpS3092 = _M0L6_2atmpS3093 & _M0L14capacity__maskS3094;
  moonbit_incref(_M0L5entryS755);
  _M0L3pslS747 = _M0L6_2atmpS3091;
  _M0L3idxS748 = _M0L6_2atmpS3092;
  _M0L5entryS749 = _M0L5entryS755;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS3090 = _M0L4selfS751->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS750;
    if (
      _M0L3idxS748 < 0
      || _M0L3idxS748 >= Moonbit_array_length(_M0L7entriesS3090)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS750
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3090[_M0L3idxS748];
    if (_M0L7_2abindS750 == 0) {
      _M0L5entryS749->$2 = _M0L3pslS747;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsbE(_M0L4selfS751, _M0L5entryS749, _M0L3idxS748);
      moonbit_decref(_M0L5entryS749);
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS753 = _M0L7_2abindS750;
      struct _M0TPB5EntryGsbE* _M0L14_2acurr__entryS754 = _M0L7_2aSomeS753;
      int32_t _M0L3pslS3080 = _M0L14_2acurr__entryS754->$2;
      if (_M0L3pslS747 > _M0L3pslS3080) {
        int32_t _M0L3pslS3085;
        int32_t _M0L6_2atmpS3081;
        int32_t _M0L6_2atmpS3083;
        int32_t _M0L14capacity__maskS3084;
        int32_t _M0L6_2atmpS3082;
        _M0L5entryS749->$2 = _M0L3pslS747;
        moonbit_incref(_M0L14_2acurr__entryS754);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsbE(_M0L4selfS751, _M0L5entryS749, _M0L3idxS748);
        moonbit_decref(_M0L5entryS749);
        _M0L3pslS3085 = _M0L14_2acurr__entryS754->$2;
        _M0L6_2atmpS3081 = _M0L3pslS3085 + 1;
        _M0L6_2atmpS3083 = _M0L3idxS748 + 1;
        _M0L14capacity__maskS3084 = _M0L4selfS751->$3;
        _M0L6_2atmpS3082 = _M0L6_2atmpS3083 & _M0L14capacity__maskS3084;
        _M0L3pslS747 = _M0L6_2atmpS3081;
        _M0L3idxS748 = _M0L6_2atmpS3082;
        _M0L5entryS749 = _M0L14_2acurr__entryS754;
        continue;
      } else {
        int32_t _M0L6_2atmpS3086 = _M0L3pslS747 + 1;
        int32_t _M0L6_2atmpS3088 = _M0L3idxS748 + 1;
        int32_t _M0L14capacity__maskS3089 = _M0L4selfS751->$3;
        int32_t _M0L6_2atmpS3087 =
          _M0L6_2atmpS3088 & _M0L14capacity__maskS3089;
        struct _M0TPB5EntryGsbE* _tmp_5633 = _M0L5entryS749;
        _M0L3pslS747 = _M0L6_2atmpS3086;
        _M0L3idxS748 = _M0L6_2atmpS3087;
        _M0L5entryS749 = _tmp_5633;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS761,
  int32_t _M0L3idxS766,
  struct _M0TPB5EntryGsfE* _M0L5entryS765
) {
  int32_t _M0L3pslS3111;
  int32_t _M0L6_2atmpS3107;
  int32_t _M0L6_2atmpS3109;
  int32_t _M0L14capacity__maskS3110;
  int32_t _M0L6_2atmpS3108;
  int32_t _M0L3pslS757;
  int32_t _M0L3idxS758;
  struct _M0TPB5EntryGsfE* _M0L5entryS759;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS3111 = _M0L5entryS765->$2;
  _M0L6_2atmpS3107 = _M0L3pslS3111 + 1;
  _M0L6_2atmpS3109 = _M0L3idxS766 + 1;
  _M0L14capacity__maskS3110 = _M0L4selfS761->$3;
  _M0L6_2atmpS3108 = _M0L6_2atmpS3109 & _M0L14capacity__maskS3110;
  moonbit_incref(_M0L5entryS765);
  _M0L3pslS757 = _M0L6_2atmpS3107;
  _M0L3idxS758 = _M0L6_2atmpS3108;
  _M0L5entryS759 = _M0L5entryS765;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS3106 = _M0L4selfS761->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS760;
    if (
      _M0L3idxS758 < 0
      || _M0L3idxS758 >= Moonbit_array_length(_M0L7entriesS3106)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS760
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3106[_M0L3idxS758];
    if (_M0L7_2abindS760 == 0) {
      _M0L5entryS759->$2 = _M0L3pslS757;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsfE(_M0L4selfS761, _M0L5entryS759, _M0L3idxS758);
      moonbit_decref(_M0L5entryS759);
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS763 = _M0L7_2abindS760;
      struct _M0TPB5EntryGsfE* _M0L14_2acurr__entryS764 = _M0L7_2aSomeS763;
      int32_t _M0L3pslS3096 = _M0L14_2acurr__entryS764->$2;
      if (_M0L3pslS757 > _M0L3pslS3096) {
        int32_t _M0L3pslS3101;
        int32_t _M0L6_2atmpS3097;
        int32_t _M0L6_2atmpS3099;
        int32_t _M0L14capacity__maskS3100;
        int32_t _M0L6_2atmpS3098;
        _M0L5entryS759->$2 = _M0L3pslS757;
        moonbit_incref(_M0L14_2acurr__entryS764);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsfE(_M0L4selfS761, _M0L5entryS759, _M0L3idxS758);
        moonbit_decref(_M0L5entryS759);
        _M0L3pslS3101 = _M0L14_2acurr__entryS764->$2;
        _M0L6_2atmpS3097 = _M0L3pslS3101 + 1;
        _M0L6_2atmpS3099 = _M0L3idxS758 + 1;
        _M0L14capacity__maskS3100 = _M0L4selfS761->$3;
        _M0L6_2atmpS3098 = _M0L6_2atmpS3099 & _M0L14capacity__maskS3100;
        _M0L3pslS757 = _M0L6_2atmpS3097;
        _M0L3idxS758 = _M0L6_2atmpS3098;
        _M0L5entryS759 = _M0L14_2acurr__entryS764;
        continue;
      } else {
        int32_t _M0L6_2atmpS3102 = _M0L3pslS757 + 1;
        int32_t _M0L6_2atmpS3104 = _M0L3idxS758 + 1;
        int32_t _M0L14capacity__maskS3105 = _M0L4selfS761->$3;
        int32_t _M0L6_2atmpS3103 =
          _M0L6_2atmpS3104 & _M0L14capacity__maskS3105;
        struct _M0TPB5EntryGsfE* _tmp_5635 = _M0L5entryS759;
        _M0L3pslS757 = _M0L6_2atmpS3102;
        _M0L3idxS758 = _M0L6_2atmpS3103;
        _M0L5entryS759 = _tmp_5635;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS771,
  int32_t _M0L3idxS776,
  struct _M0TPB5EntryGsiE* _M0L5entryS775
) {
  int32_t _M0L3pslS3127;
  int32_t _M0L6_2atmpS3123;
  int32_t _M0L6_2atmpS3125;
  int32_t _M0L14capacity__maskS3126;
  int32_t _M0L6_2atmpS3124;
  int32_t _M0L3pslS767;
  int32_t _M0L3idxS768;
  struct _M0TPB5EntryGsiE* _M0L5entryS769;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS3127 = _M0L5entryS775->$2;
  _M0L6_2atmpS3123 = _M0L3pslS3127 + 1;
  _M0L6_2atmpS3125 = _M0L3idxS776 + 1;
  _M0L14capacity__maskS3126 = _M0L4selfS771->$3;
  _M0L6_2atmpS3124 = _M0L6_2atmpS3125 & _M0L14capacity__maskS3126;
  moonbit_incref(_M0L5entryS775);
  _M0L3pslS767 = _M0L6_2atmpS3123;
  _M0L3idxS768 = _M0L6_2atmpS3124;
  _M0L5entryS769 = _M0L5entryS775;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3122 = _M0L4selfS771->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS770;
    if (
      _M0L3idxS768 < 0
      || _M0L3idxS768 >= Moonbit_array_length(_M0L7entriesS3122)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS770
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3122[_M0L3idxS768];
    if (_M0L7_2abindS770 == 0) {
      _M0L5entryS769->$2 = _M0L3pslS767;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsiE(_M0L4selfS771, _M0L5entryS769, _M0L3idxS768);
      moonbit_decref(_M0L5entryS769);
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS773 = _M0L7_2abindS770;
      struct _M0TPB5EntryGsiE* _M0L14_2acurr__entryS774 = _M0L7_2aSomeS773;
      int32_t _M0L3pslS3112 = _M0L14_2acurr__entryS774->$2;
      if (_M0L3pslS767 > _M0L3pslS3112) {
        int32_t _M0L3pslS3117;
        int32_t _M0L6_2atmpS3113;
        int32_t _M0L6_2atmpS3115;
        int32_t _M0L14capacity__maskS3116;
        int32_t _M0L6_2atmpS3114;
        _M0L5entryS769->$2 = _M0L3pslS767;
        moonbit_incref(_M0L14_2acurr__entryS774);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsiE(_M0L4selfS771, _M0L5entryS769, _M0L3idxS768);
        moonbit_decref(_M0L5entryS769);
        _M0L3pslS3117 = _M0L14_2acurr__entryS774->$2;
        _M0L6_2atmpS3113 = _M0L3pslS3117 + 1;
        _M0L6_2atmpS3115 = _M0L3idxS768 + 1;
        _M0L14capacity__maskS3116 = _M0L4selfS771->$3;
        _M0L6_2atmpS3114 = _M0L6_2atmpS3115 & _M0L14capacity__maskS3116;
        _M0L3pslS767 = _M0L6_2atmpS3113;
        _M0L3idxS768 = _M0L6_2atmpS3114;
        _M0L5entryS769 = _M0L14_2acurr__entryS774;
        continue;
      } else {
        int32_t _M0L6_2atmpS3118 = _M0L3pslS767 + 1;
        int32_t _M0L6_2atmpS3120 = _M0L3idxS768 + 1;
        int32_t _M0L14capacity__maskS3121 = _M0L4selfS771->$3;
        int32_t _M0L6_2atmpS3119 =
          _M0L6_2atmpS3120 & _M0L14capacity__maskS3121;
        struct _M0TPB5EntryGsiE* _tmp_5637 = _M0L5entryS769;
        _M0L3pslS767 = _M0L6_2atmpS3118;
        _M0L3idxS768 = _M0L6_2atmpS3119;
        _M0L5entryS769 = _tmp_5637;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS697,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS699,
  int32_t _M0L8new__idxS698
) {
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3038;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3039;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS5023;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS700;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS3038 = _M0L4selfS697->$0;
  _M0L6_2atmpS3039 = _M0L5entryS699;
  if (
    _M0L8new__idxS698 < 0
    || _M0L8new__idxS698 >= Moonbit_array_length(_M0L7entriesS3038)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS5023
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3038[
      _M0L8new__idxS698
    ];
  if (_M0L6_2atmpS3039) {
    moonbit_incref(_M0L6_2atmpS3039);
  }
  if (_M0L6_2aoldS5023) {
    moonbit_decref(_M0L6_2aoldS5023);
  }
  _M0L7entriesS3038[_M0L8new__idxS698] = _M0L6_2atmpS3039;
  _M0L7_2abindS700 = _M0L5entryS699->$1;
  if (_M0L7_2abindS700 == 0) {
    _M0L4selfS697->$6 = _M0L8new__idxS698;
  } else {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS701 =
      _M0L7_2abindS700;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2anextS702 =
      _M0L7_2aSomeS701;
    _M0L7_2anextS702->$0 = _M0L8new__idxS698;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS703,
  struct _M0TPB5EntryGsiE* _M0L5entryS705,
  int32_t _M0L8new__idxS704
) {
  struct _M0TPB5EntryGsiE** _M0L7entriesS3040;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS3041;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS5026;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS706;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS3040 = _M0L4selfS703->$0;
  _M0L6_2atmpS3041 = _M0L5entryS705;
  if (
    _M0L8new__idxS704 < 0
    || _M0L8new__idxS704 >= Moonbit_array_length(_M0L7entriesS3040)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS5026
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3040[_M0L8new__idxS704];
  if (_M0L6_2atmpS3041) {
    moonbit_incref(_M0L6_2atmpS3041);
  }
  if (_M0L6_2aoldS5026) {
    moonbit_decref(_M0L6_2aoldS5026);
  }
  _M0L7entriesS3040[_M0L8new__idxS704] = _M0L6_2atmpS3041;
  _M0L7_2abindS706 = _M0L5entryS705->$1;
  if (_M0L7_2abindS706 == 0) {
    _M0L4selfS703->$6 = _M0L8new__idxS704;
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS707 = _M0L7_2abindS706;
    struct _M0TPB5EntryGsiE* _M0L7_2anextS708 = _M0L7_2aSomeS707;
    _M0L7_2anextS708->$0 = _M0L8new__idxS704;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS709,
  struct _M0TPB5EntryGssE* _M0L5entryS711,
  int32_t _M0L8new__idxS710
) {
  struct _M0TPB5EntryGssE** _M0L7entriesS3042;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS3043;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS5029;
  struct _M0TPB5EntryGssE* _M0L7_2abindS712;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS3042 = _M0L4selfS709->$0;
  _M0L6_2atmpS3043 = _M0L5entryS711;
  if (
    _M0L8new__idxS710 < 0
    || _M0L8new__idxS710 >= Moonbit_array_length(_M0L7entriesS3042)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS5029
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS3042[_M0L8new__idxS710];
  if (_M0L6_2atmpS3043) {
    moonbit_incref(_M0L6_2atmpS3043);
  }
  if (_M0L6_2aoldS5029) {
    moonbit_decref(_M0L6_2aoldS5029);
  }
  _M0L7entriesS3042[_M0L8new__idxS710] = _M0L6_2atmpS3043;
  _M0L7_2abindS712 = _M0L5entryS711->$1;
  if (_M0L7_2abindS712 == 0) {
    _M0L4selfS709->$6 = _M0L8new__idxS710;
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS713 = _M0L7_2abindS712;
    struct _M0TPB5EntryGssE* _M0L7_2anextS714 = _M0L7_2aSomeS713;
    _M0L7_2anextS714->$0 = _M0L8new__idxS710;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS715,
  struct _M0TPB5EntryGsbE* _M0L5entryS717,
  int32_t _M0L8new__idxS716
) {
  struct _M0TPB5EntryGsbE** _M0L7entriesS3044;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS3045;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS5032;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS718;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS3044 = _M0L4selfS715->$0;
  _M0L6_2atmpS3045 = _M0L5entryS717;
  if (
    _M0L8new__idxS716 < 0
    || _M0L8new__idxS716 >= Moonbit_array_length(_M0L7entriesS3044)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS5032
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3044[_M0L8new__idxS716];
  if (_M0L6_2atmpS3045) {
    moonbit_incref(_M0L6_2atmpS3045);
  }
  if (_M0L6_2aoldS5032) {
    moonbit_decref(_M0L6_2aoldS5032);
  }
  _M0L7entriesS3044[_M0L8new__idxS716] = _M0L6_2atmpS3045;
  _M0L7_2abindS718 = _M0L5entryS717->$1;
  if (_M0L7_2abindS718 == 0) {
    _M0L4selfS715->$6 = _M0L8new__idxS716;
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS719 = _M0L7_2abindS718;
    struct _M0TPB5EntryGsbE* _M0L7_2anextS720 = _M0L7_2aSomeS719;
    _M0L7_2anextS720->$0 = _M0L8new__idxS716;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS721,
  struct _M0TPB5EntryGsfE* _M0L5entryS723,
  int32_t _M0L8new__idxS722
) {
  struct _M0TPB5EntryGsfE** _M0L7entriesS3046;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS3047;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS5035;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS724;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS3046 = _M0L4selfS721->$0;
  _M0L6_2atmpS3047 = _M0L5entryS723;
  if (
    _M0L8new__idxS722 < 0
    || _M0L8new__idxS722 >= Moonbit_array_length(_M0L7entriesS3046)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS5035
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3046[_M0L8new__idxS722];
  if (_M0L6_2atmpS3047) {
    moonbit_incref(_M0L6_2atmpS3047);
  }
  if (_M0L6_2aoldS5035) {
    moonbit_decref(_M0L6_2aoldS5035);
  }
  _M0L7entriesS3046[_M0L8new__idxS722] = _M0L6_2atmpS3047;
  _M0L7_2abindS724 = _M0L5entryS723->$1;
  if (_M0L7_2abindS724 == 0) {
    _M0L4selfS721->$6 = _M0L8new__idxS722;
  } else {
    struct _M0TPB5EntryGsfE* _M0L7_2aSomeS725 = _M0L7_2abindS724;
    struct _M0TPB5EntryGsfE* _M0L7_2anextS726 = _M0L7_2aSomeS725;
    _M0L7_2anextS726->$0 = _M0L8new__idxS722;
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS678,
  int32_t _M0L3idxS680,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS679
) {
  int32_t _M0L7_2abindS677;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2998;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2999;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS5037;
  int32_t _M0L4sizeS3001;
  int32_t _M0L6_2atmpS3000;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS677 = _M0L4selfS678->$6;
  switch (_M0L7_2abindS677) {
    case -1: {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2993 =
        _M0L5entryS679;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS5039 =
        _M0L4selfS678->$5;
      if (_M0L6_2atmpS2993) {
        moonbit_incref(_M0L6_2atmpS2993);
      }
      if (_M0L6_2aoldS5039) {
        moonbit_decref(_M0L6_2aoldS5039);
      }
      _M0L4selfS678->$5 = _M0L6_2atmpS2993;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2997 =
        _M0L4selfS678->$0;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2996;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2994;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2995;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS5040;
      if (
        _M0L7_2abindS677 < 0
        || _M0L7_2abindS677 >= Moonbit_array_length(_M0L7entriesS2997)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2996
      = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2997[
          _M0L7_2abindS677
        ];
      if (_M0L6_2atmpS2996) {
        moonbit_incref(_M0L6_2atmpS2996);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2994
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP19moonbitDB10RedisValueEE(_M0L6_2atmpS2996);
      if (_M0L6_2atmpS2996) {
        moonbit_decref(_M0L6_2atmpS2996);
      }
      _M0L6_2atmpS2995 = _M0L5entryS679;
      _M0L6_2aoldS5040 = _M0L6_2atmpS2994->$1;
      if (_M0L6_2atmpS2995) {
        moonbit_incref(_M0L6_2atmpS2995);
      }
      if (_M0L6_2aoldS5040) {
        moonbit_decref(_M0L6_2aoldS5040);
      }
      _M0L6_2atmpS2994->$1 = _M0L6_2atmpS2995;
      moonbit_decref(_M0L6_2atmpS2994);
      break;
    }
  }
  _M0L4selfS678->$6 = _M0L3idxS680;
  _M0L7entriesS2998 = _M0L4selfS678->$0;
  _M0L6_2atmpS2999 = _M0L5entryS679;
  if (
    _M0L3idxS680 < 0
    || _M0L3idxS680 >= Moonbit_array_length(_M0L7entriesS2998)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS5037
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2998[
      _M0L3idxS680
    ];
  if (_M0L6_2atmpS2999) {
    moonbit_incref(_M0L6_2atmpS2999);
  }
  if (_M0L6_2aoldS5037) {
    moonbit_decref(_M0L6_2aoldS5037);
  }
  _M0L7entriesS2998[_M0L3idxS680] = _M0L6_2atmpS2999;
  _M0L4sizeS3001 = _M0L4selfS678->$1;
  _M0L6_2atmpS3000 = _M0L4sizeS3001 + 1;
  _M0L4selfS678->$1 = _M0L6_2atmpS3000;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGssE(
  struct _M0TPB3MapGssE* _M0L4selfS682,
  int32_t _M0L3idxS684,
  struct _M0TPB5EntryGssE* _M0L5entryS683
) {
  int32_t _M0L7_2abindS681;
  struct _M0TPB5EntryGssE** _M0L7entriesS3007;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS3008;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS5043;
  int32_t _M0L4sizeS3010;
  int32_t _M0L6_2atmpS3009;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS681 = _M0L4selfS682->$6;
  switch (_M0L7_2abindS681) {
    case -1: {
      struct _M0TPB5EntryGssE* _M0L6_2atmpS3002 = _M0L5entryS683;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS5045 = _M0L4selfS682->$5;
      if (_M0L6_2atmpS3002) {
        moonbit_incref(_M0L6_2atmpS3002);
      }
      if (_M0L6_2aoldS5045) {
        moonbit_decref(_M0L6_2aoldS5045);
      }
      _M0L4selfS682->$5 = _M0L6_2atmpS3002;
      break;
    }
    default: {
      struct _M0TPB5EntryGssE** _M0L7entriesS3006 = _M0L4selfS682->$0;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS3005;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS3003;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS3004;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS5046;
      if (
        _M0L7_2abindS681 < 0
        || _M0L7_2abindS681 >= Moonbit_array_length(_M0L7entriesS3006)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3005
      = (struct _M0TPB5EntryGssE*)_M0L7entriesS3006[_M0L7_2abindS681];
      if (_M0L6_2atmpS3005) {
        moonbit_incref(_M0L6_2atmpS3005);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3003
      = _M0MPC16option6Option6unwrapGRPB5EntryGssEE(_M0L6_2atmpS3005);
      if (_M0L6_2atmpS3005) {
        moonbit_decref(_M0L6_2atmpS3005);
      }
      _M0L6_2atmpS3004 = _M0L5entryS683;
      _M0L6_2aoldS5046 = _M0L6_2atmpS3003->$1;
      if (_M0L6_2atmpS3004) {
        moonbit_incref(_M0L6_2atmpS3004);
      }
      if (_M0L6_2aoldS5046) {
        moonbit_decref(_M0L6_2aoldS5046);
      }
      _M0L6_2atmpS3003->$1 = _M0L6_2atmpS3004;
      moonbit_decref(_M0L6_2atmpS3003);
      break;
    }
  }
  _M0L4selfS682->$6 = _M0L3idxS684;
  _M0L7entriesS3007 = _M0L4selfS682->$0;
  _M0L6_2atmpS3008 = _M0L5entryS683;
  if (
    _M0L3idxS684 < 0
    || _M0L3idxS684 >= Moonbit_array_length(_M0L7entriesS3007)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS5043
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS3007[_M0L3idxS684];
  if (_M0L6_2atmpS3008) {
    moonbit_incref(_M0L6_2atmpS3008);
  }
  if (_M0L6_2aoldS5043) {
    moonbit_decref(_M0L6_2aoldS5043);
  }
  _M0L7entriesS3007[_M0L3idxS684] = _M0L6_2atmpS3008;
  _M0L4sizeS3010 = _M0L4selfS682->$1;
  _M0L6_2atmpS3009 = _M0L4sizeS3010 + 1;
  _M0L4selfS682->$1 = _M0L6_2atmpS3009;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS686,
  int32_t _M0L3idxS688,
  struct _M0TPB5EntryGsbE* _M0L5entryS687
) {
  int32_t _M0L7_2abindS685;
  struct _M0TPB5EntryGsbE** _M0L7entriesS3016;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS3017;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS5049;
  int32_t _M0L4sizeS3019;
  int32_t _M0L6_2atmpS3018;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS685 = _M0L4selfS686->$6;
  switch (_M0L7_2abindS685) {
    case -1: {
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS3011 = _M0L5entryS687;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS5051 = _M0L4selfS686->$5;
      if (_M0L6_2atmpS3011) {
        moonbit_incref(_M0L6_2atmpS3011);
      }
      if (_M0L6_2aoldS5051) {
        moonbit_decref(_M0L6_2aoldS5051);
      }
      _M0L4selfS686->$5 = _M0L6_2atmpS3011;
      break;
    }
    default: {
      struct _M0TPB5EntryGsbE** _M0L7entriesS3015 = _M0L4selfS686->$0;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS3014;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS3012;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS3013;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS5052;
      if (
        _M0L7_2abindS685 < 0
        || _M0L7_2abindS685 >= Moonbit_array_length(_M0L7entriesS3015)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3014
      = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3015[_M0L7_2abindS685];
      if (_M0L6_2atmpS3014) {
        moonbit_incref(_M0L6_2atmpS3014);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3012
      = _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(_M0L6_2atmpS3014);
      if (_M0L6_2atmpS3014) {
        moonbit_decref(_M0L6_2atmpS3014);
      }
      _M0L6_2atmpS3013 = _M0L5entryS687;
      _M0L6_2aoldS5052 = _M0L6_2atmpS3012->$1;
      if (_M0L6_2atmpS3013) {
        moonbit_incref(_M0L6_2atmpS3013);
      }
      if (_M0L6_2aoldS5052) {
        moonbit_decref(_M0L6_2aoldS5052);
      }
      _M0L6_2atmpS3012->$1 = _M0L6_2atmpS3013;
      moonbit_decref(_M0L6_2atmpS3012);
      break;
    }
  }
  _M0L4selfS686->$6 = _M0L3idxS688;
  _M0L7entriesS3016 = _M0L4selfS686->$0;
  _M0L6_2atmpS3017 = _M0L5entryS687;
  if (
    _M0L3idxS688 < 0
    || _M0L3idxS688 >= Moonbit_array_length(_M0L7entriesS3016)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS5049
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3016[_M0L3idxS688];
  if (_M0L6_2atmpS3017) {
    moonbit_incref(_M0L6_2atmpS3017);
  }
  if (_M0L6_2aoldS5049) {
    moonbit_decref(_M0L6_2aoldS5049);
  }
  _M0L7entriesS3016[_M0L3idxS688] = _M0L6_2atmpS3017;
  _M0L4sizeS3019 = _M0L4selfS686->$1;
  _M0L6_2atmpS3018 = _M0L4sizeS3019 + 1;
  _M0L4selfS686->$1 = _M0L6_2atmpS3018;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS690,
  int32_t _M0L3idxS692,
  struct _M0TPB5EntryGsfE* _M0L5entryS691
) {
  int32_t _M0L7_2abindS689;
  struct _M0TPB5EntryGsfE** _M0L7entriesS3025;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS3026;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS5055;
  int32_t _M0L4sizeS3028;
  int32_t _M0L6_2atmpS3027;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS689 = _M0L4selfS690->$6;
  switch (_M0L7_2abindS689) {
    case -1: {
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS3020 = _M0L5entryS691;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS5057 = _M0L4selfS690->$5;
      if (_M0L6_2atmpS3020) {
        moonbit_incref(_M0L6_2atmpS3020);
      }
      if (_M0L6_2aoldS5057) {
        moonbit_decref(_M0L6_2aoldS5057);
      }
      _M0L4selfS690->$5 = _M0L6_2atmpS3020;
      break;
    }
    default: {
      struct _M0TPB5EntryGsfE** _M0L7entriesS3024 = _M0L4selfS690->$0;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS3023;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS3021;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS3022;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS5058;
      if (
        _M0L7_2abindS689 < 0
        || _M0L7_2abindS689 >= Moonbit_array_length(_M0L7entriesS3024)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3023
      = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3024[_M0L7_2abindS689];
      if (_M0L6_2atmpS3023) {
        moonbit_incref(_M0L6_2atmpS3023);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3021
      = _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(_M0L6_2atmpS3023);
      if (_M0L6_2atmpS3023) {
        moonbit_decref(_M0L6_2atmpS3023);
      }
      _M0L6_2atmpS3022 = _M0L5entryS691;
      _M0L6_2aoldS5058 = _M0L6_2atmpS3021->$1;
      if (_M0L6_2atmpS3022) {
        moonbit_incref(_M0L6_2atmpS3022);
      }
      if (_M0L6_2aoldS5058) {
        moonbit_decref(_M0L6_2aoldS5058);
      }
      _M0L6_2atmpS3021->$1 = _M0L6_2atmpS3022;
      moonbit_decref(_M0L6_2atmpS3021);
      break;
    }
  }
  _M0L4selfS690->$6 = _M0L3idxS692;
  _M0L7entriesS3025 = _M0L4selfS690->$0;
  _M0L6_2atmpS3026 = _M0L5entryS691;
  if (
    _M0L3idxS692 < 0
    || _M0L3idxS692 >= Moonbit_array_length(_M0L7entriesS3025)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS5055
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3025[_M0L3idxS692];
  if (_M0L6_2atmpS3026) {
    moonbit_incref(_M0L6_2atmpS3026);
  }
  if (_M0L6_2aoldS5055) {
    moonbit_decref(_M0L6_2aoldS5055);
  }
  _M0L7entriesS3025[_M0L3idxS692] = _M0L6_2atmpS3026;
  _M0L4sizeS3028 = _M0L4selfS690->$1;
  _M0L6_2atmpS3027 = _M0L4sizeS3028 + 1;
  _M0L4selfS690->$1 = _M0L6_2atmpS3027;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS694,
  int32_t _M0L3idxS696,
  struct _M0TPB5EntryGsiE* _M0L5entryS695
) {
  int32_t _M0L7_2abindS693;
  struct _M0TPB5EntryGsiE** _M0L7entriesS3034;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS3035;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS5061;
  int32_t _M0L4sizeS3037;
  int32_t _M0L6_2atmpS3036;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS693 = _M0L4selfS694->$6;
  switch (_M0L7_2abindS693) {
    case -1: {
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS3029 = _M0L5entryS695;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS5063 = _M0L4selfS694->$5;
      if (_M0L6_2atmpS3029) {
        moonbit_incref(_M0L6_2atmpS3029);
      }
      if (_M0L6_2aoldS5063) {
        moonbit_decref(_M0L6_2aoldS5063);
      }
      _M0L4selfS694->$5 = _M0L6_2atmpS3029;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS3033 = _M0L4selfS694->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS3032;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS3030;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS3031;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS5064;
      if (
        _M0L7_2abindS693 < 0
        || _M0L7_2abindS693 >= Moonbit_array_length(_M0L7entriesS3033)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3032
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3033[_M0L7_2abindS693];
      if (_M0L6_2atmpS3032) {
        moonbit_incref(_M0L6_2atmpS3032);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3030
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS3032);
      if (_M0L6_2atmpS3032) {
        moonbit_decref(_M0L6_2atmpS3032);
      }
      _M0L6_2atmpS3031 = _M0L5entryS695;
      _M0L6_2aoldS5064 = _M0L6_2atmpS3030->$1;
      if (_M0L6_2atmpS3031) {
        moonbit_incref(_M0L6_2atmpS3031);
      }
      if (_M0L6_2aoldS5064) {
        moonbit_decref(_M0L6_2aoldS5064);
      }
      _M0L6_2atmpS3030->$1 = _M0L6_2atmpS3031;
      moonbit_decref(_M0L6_2atmpS3030);
      break;
    }
  }
  _M0L4selfS694->$6 = _M0L3idxS696;
  _M0L7entriesS3034 = _M0L4selfS694->$0;
  _M0L6_2atmpS3035 = _M0L5entryS695;
  if (
    _M0L3idxS696 < 0
    || _M0L3idxS696 >= Moonbit_array_length(_M0L7entriesS3034)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS5061
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3034[_M0L3idxS696];
  if (_M0L6_2atmpS3035) {
    moonbit_incref(_M0L6_2atmpS3035);
  }
  if (_M0L6_2aoldS5061) {
    moonbit_decref(_M0L6_2aoldS5061);
  }
  _M0L7entriesS3034[_M0L3idxS696] = _M0L6_2atmpS3035;
  _M0L4sizeS3037 = _M0L4selfS694->$1;
  _M0L6_2atmpS3036 = _M0L4sizeS3037 + 1;
  _M0L4selfS694->$1 = _M0L6_2atmpS3036;
  return 0;
}

int32_t _M0MPC13int3Int3max(int32_t _M0L4selfS675, int32_t _M0L5otherS676) {
  #line 75 "/home/developer/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS675 > _M0L5otherS676) {
    return _M0L4selfS675;
  } else {
    return _M0L5otherS676;
  }
}

int32_t _M0FPB21capacity__for__length(int32_t _M0L6lengthS674) {
  int32_t _M0Lm8capacityS673;
  int32_t _M0L6_2atmpS2991;
  int32_t _M0L6_2atmpS2990;
  #line 71 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 72 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS673 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS674);
  _M0L6_2atmpS2991 = _M0Lm8capacityS673;
  #line 73 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2990 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2991);
  if (_M0L6lengthS674 > _M0L6_2atmpS2990) {
    int32_t _M0L6_2atmpS2992 = _M0Lm8capacityS673;
    _M0Lm8capacityS673 = _M0L6_2atmpS2992 * 2;
  }
  return _M0Lm8capacityS673;
}

struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0FPB8new__mapGsRP19moonbitDB10RedisValueE(
  int32_t _M0L8capacityS644
) {
  int32_t _M0L8capacityS643;
  int32_t _M0L7_2abindS645;
  int32_t _M0L7_2abindS646;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2985;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7_2abindS647;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS648;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _block_5638;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS643
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS644);
  _M0L7_2abindS645 = _M0L8capacityS643 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS646 = _M0FPB21calc__grow__threshold(_M0L8capacityS643);
  _M0L6_2atmpS2985 = 0;
  _M0L7_2abindS647
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE**)moonbit_make_ref_array(_M0L8capacityS643, _M0L6_2atmpS2985);
  _M0L7_2abindS648 = 0;
  _block_5638
  = (struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRP19moonbitDB10RedisValueE));
  Moonbit_object_header(_block_5638)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 106, 0);
  _block_5638->$0 = _M0L7_2abindS647;
  _block_5638->$1 = 0;
  _block_5638->$2 = _M0L8capacityS643;
  _block_5638->$3 = _M0L7_2abindS645;
  _block_5638->$4 = _M0L7_2abindS646;
  _block_5638->$5 = _M0L7_2abindS648;
  _block_5638->$6 = -1;
  return _block_5638;
}

struct _M0TPB3MapGsiE* _M0FPB8new__mapGsiE(int32_t _M0L8capacityS650) {
  int32_t _M0L8capacityS649;
  int32_t _M0L7_2abindS651;
  int32_t _M0L7_2abindS652;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2986;
  struct _M0TPB5EntryGsiE** _M0L7_2abindS653;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS654;
  struct _M0TPB3MapGsiE* _block_5639;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS649
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS650);
  _M0L7_2abindS651 = _M0L8capacityS649 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS652 = _M0FPB21calc__grow__threshold(_M0L8capacityS649);
  _M0L6_2atmpS2986 = 0;
  _M0L7_2abindS653
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L8capacityS649, _M0L6_2atmpS2986);
  _M0L7_2abindS654 = 0;
  _block_5639
  = (struct _M0TPB3MapGsiE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsiE));
  Moonbit_object_header(_block_5639)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 110, 0);
  _block_5639->$0 = _M0L7_2abindS653;
  _block_5639->$1 = 0;
  _block_5639->$2 = _M0L8capacityS649;
  _block_5639->$3 = _M0L7_2abindS651;
  _block_5639->$4 = _M0L7_2abindS652;
  _block_5639->$5 = _M0L7_2abindS654;
  _block_5639->$6 = -1;
  return _block_5639;
}

struct _M0TPB3MapGssE* _M0FPB8new__mapGssE(int32_t _M0L8capacityS656) {
  int32_t _M0L8capacityS655;
  int32_t _M0L7_2abindS657;
  int32_t _M0L7_2abindS658;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2987;
  struct _M0TPB5EntryGssE** _M0L7_2abindS659;
  struct _M0TPB5EntryGssE* _M0L7_2abindS660;
  struct _M0TPB3MapGssE* _block_5640;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS655
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS656);
  _M0L7_2abindS657 = _M0L8capacityS655 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS658 = _M0FPB21calc__grow__threshold(_M0L8capacityS655);
  _M0L6_2atmpS2987 = 0;
  _M0L7_2abindS659
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L8capacityS655, _M0L6_2atmpS2987);
  _M0L7_2abindS660 = 0;
  _block_5640
  = (struct _M0TPB3MapGssE*)moonbit_malloc(sizeof(struct _M0TPB3MapGssE));
  Moonbit_object_header(_block_5640)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 114, 0);
  _block_5640->$0 = _M0L7_2abindS659;
  _block_5640->$1 = 0;
  _block_5640->$2 = _M0L8capacityS655;
  _block_5640->$3 = _M0L7_2abindS657;
  _block_5640->$4 = _M0L7_2abindS658;
  _block_5640->$5 = _M0L7_2abindS660;
  _block_5640->$6 = -1;
  return _block_5640;
}

struct _M0TPB3MapGsbE* _M0FPB8new__mapGsbE(int32_t _M0L8capacityS662) {
  int32_t _M0L8capacityS661;
  int32_t _M0L7_2abindS663;
  int32_t _M0L7_2abindS664;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2988;
  struct _M0TPB5EntryGsbE** _M0L7_2abindS665;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS666;
  struct _M0TPB3MapGsbE* _block_5641;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS661
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS662);
  _M0L7_2abindS663 = _M0L8capacityS661 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS664 = _M0FPB21calc__grow__threshold(_M0L8capacityS661);
  _M0L6_2atmpS2988 = 0;
  _M0L7_2abindS665
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L8capacityS661, _M0L6_2atmpS2988);
  _M0L7_2abindS666 = 0;
  _block_5641
  = (struct _M0TPB3MapGsbE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsbE));
  Moonbit_object_header(_block_5641)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 118, 0);
  _block_5641->$0 = _M0L7_2abindS665;
  _block_5641->$1 = 0;
  _block_5641->$2 = _M0L8capacityS661;
  _block_5641->$3 = _M0L7_2abindS663;
  _block_5641->$4 = _M0L7_2abindS664;
  _block_5641->$5 = _M0L7_2abindS666;
  _block_5641->$6 = -1;
  return _block_5641;
}

struct _M0TPB3MapGsfE* _M0FPB8new__mapGsfE(int32_t _M0L8capacityS668) {
  int32_t _M0L8capacityS667;
  int32_t _M0L7_2abindS669;
  int32_t _M0L7_2abindS670;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2989;
  struct _M0TPB5EntryGsfE** _M0L7_2abindS671;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS672;
  struct _M0TPB3MapGsfE* _block_5642;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS667
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS668);
  _M0L7_2abindS669 = _M0L8capacityS667 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS670 = _M0FPB21calc__grow__threshold(_M0L8capacityS667);
  _M0L6_2atmpS2989 = 0;
  _M0L7_2abindS671
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L8capacityS667, _M0L6_2atmpS2989);
  _M0L7_2abindS672 = 0;
  _block_5642
  = (struct _M0TPB3MapGsfE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsfE));
  Moonbit_object_header(_block_5642)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 122, 0);
  _block_5642->$0 = _M0L7_2abindS671;
  _block_5642->$1 = 0;
  _block_5642->$2 = _M0L8capacityS667;
  _block_5642->$3 = _M0L7_2abindS669;
  _block_5642->$4 = _M0L7_2abindS670;
  _block_5642->$5 = _M0L7_2abindS672;
  _block_5642->$6 = -1;
  return _block_5642;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS642) {
  #line 33 "/home/developer/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS642 >= 0) {
    int32_t _M0L6_2atmpS2984;
    int32_t _M0L6_2atmpS2983;
    int32_t _M0L6_2atmpS2982;
    int32_t _M0L6_2atmpS2981;
    if (_M0L4selfS642 <= 1) {
      return 1;
    }
    if (_M0L4selfS642 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2984 = _M0L4selfS642 - 1;
    #line 44 "/home/developer/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS2983 = moonbit_clz32(_M0L6_2atmpS2984);
    _M0L6_2atmpS2982 = _M0L6_2atmpS2983 - 1;
    _M0L6_2atmpS2981 = 2147483647 >> (_M0L6_2atmpS2982 & 31);
    return _M0L6_2atmpS2981 + 1;
  } else {
    #line 34 "/home/developer/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS641) {
  int32_t _M0L6_2atmpS2980;
  #line 610 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2980 = _M0L8capacityS641 * 13;
  return _M0L6_2atmpS2980 / 16;
}

int32_t _M0MPC16option6Option6unwrapGiE(int64_t _M0L4selfS629) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS629 == 4294967296ll) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    int64_t _M0L7_2aSomeS630 = _M0L4selfS629;
    return (int32_t)_M0L7_2aSomeS630;
  }
}

struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP19moonbitDB10RedisValueEE(
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4selfS631
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS631 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS632 =
      _M0L4selfS631;
    if (_M0L7_2aSomeS632) {
      moonbit_incref(_M0L7_2aSomeS632);
    }
    return _M0L7_2aSomeS632;
  }
}

struct _M0TPB5EntryGsiE* _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(
  struct _M0TPB5EntryGsiE* _M0L4selfS633
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS633 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS634 = _M0L4selfS633;
    if (_M0L7_2aSomeS634) {
      moonbit_incref(_M0L7_2aSomeS634);
    }
    return _M0L7_2aSomeS634;
  }
}

struct _M0TPB5EntryGssE* _M0MPC16option6Option6unwrapGRPB5EntryGssEE(
  struct _M0TPB5EntryGssE* _M0L4selfS635
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS635 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS636 = _M0L4selfS635;
    if (_M0L7_2aSomeS636) {
      moonbit_incref(_M0L7_2aSomeS636);
    }
    return _M0L7_2aSomeS636;
  }
}

struct _M0TPB5EntryGsbE* _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(
  struct _M0TPB5EntryGsbE* _M0L4selfS637
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS637 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS638 = _M0L4selfS637;
    if (_M0L7_2aSomeS638) {
      moonbit_incref(_M0L7_2aSomeS638);
    }
    return _M0L7_2aSomeS638;
  }
}

struct _M0TPB5EntryGsfE* _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(
  struct _M0TPB5EntryGsfE* _M0L4selfS639
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS639 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsfE* _M0L7_2aSomeS640 = _M0L4selfS639;
    if (_M0L7_2aSomeS640) {
      moonbit_incref(_M0L7_2aSomeS640);
    }
    return _M0L7_2aSomeS640;
  }
}

moonbit_string_t _M0MPC15array9ArrayView4joinGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS603,
  struct _M0TPC16string10StringView _M0L9separatorS616
) {
  int32_t _M0L3endS2955;
  int32_t _M0L5startS2956;
  int32_t _M0L6_2atmpS2954;
  #line 1497 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS2955 = _M0L4selfS603.$2;
  _M0L5startS2956 = _M0L4selfS603.$1;
  _M0L6_2atmpS2954 = _M0L3endS2955 - _M0L5startS2956;
  if (_M0L6_2atmpS2954 == 0) {
    return (moonbit_string_t)moonbit_string_literal_75.data;
  } else {
    moonbit_string_t* _M0L3bufS2978 = _M0L4selfS603.$0;
    int32_t _M0L5startS2979 = _M0L4selfS603.$1;
    moonbit_string_t _M0L5_2ahdS604 =
      (moonbit_string_t)_M0L3bufS2978[_M0L5startS2979];
    moonbit_string_t* _M0L9_2ax__bufS605 = _M0L4selfS603.$0;
    int32_t _M0L5startS2977 = _M0L4selfS603.$1;
    int32_t _M0L11_2ax__startS606 = 1 + _M0L5startS2977;
    int32_t _M0L9_2ax__endS607 = _M0L4selfS603.$2;
    struct _M0TPC16string10StringView _M0L2hdS608;
    int32_t _M0L7_2abindS609;
    int32_t _M0L3endS2975;
    int32_t _M0L5startS2976;
    int32_t _M0L6_2atmpS2974;
    int32_t _M0L10size__hintS610;
    int32_t _M0L2__S611;
    int32_t _M0L10size__hintS612;
    int32_t _M0L10size__hintS617;
    struct _M0TPB13StringBuilder* _M0L3bufS618;
    int32_t _M0L3endS2958;
    int32_t _M0L5startS2959;
    int32_t _M0L6_2atmpS2957;
    moonbit_string_t _result_5646;
    moonbit_incref(_M0L9_2ax__bufS605);
    moonbit_incref(_M0L5_2ahdS604);
    #line 1504 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0L2hdS608
    = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L5_2ahdS604);
    moonbit_decref(_M0L5_2ahdS604);
    _M0L7_2abindS609 = _M0L9_2ax__endS607 - _M0L11_2ax__startS606;
    _M0L3endS2975 = _M0L2hdS608.$2;
    _M0L5startS2976 = _M0L2hdS608.$1;
    _M0L6_2atmpS2974 = _M0L3endS2975 - _M0L5startS2976;
    _M0L2__S611 = 0;
    _M0L10size__hintS612 = _M0L6_2atmpS2974;
    while (1) {
      if (_M0L2__S611 < _M0L7_2abindS609) {
        int32_t _M0L6_2atmpS2973 = _M0L11_2ax__startS606 + _M0L2__S611;
        moonbit_string_t _M0L1sS613 =
          (moonbit_string_t)_M0L9_2ax__bufS605[_M0L6_2atmpS2973];
        int32_t _M0L6_2atmpS2964 = _M0L2__S611 + 1;
        struct _M0TPC16string10StringView _M0L7_2abindS615;
        int32_t _M0L3endS2971;
        int32_t _M0L5startS2972;
        int32_t _M0L6_2atmpS2970;
        int32_t _M0L6_2atmpS2966;
        int32_t _M0L3endS2968;
        int32_t _M0L5startS2969;
        int32_t _M0L6_2atmpS2967;
        int32_t _M0L6_2atmpS2965;
        moonbit_incref(_M0L1sS613);
        #line 1506 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
        _M0L7_2abindS615
        = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS613);
        moonbit_decref(_M0L1sS613);
        _M0L3endS2971 = _M0L7_2abindS615.$2;
        _M0L5startS2972 = _M0L7_2abindS615.$1;
        moonbit_decref(_M0L7_2abindS615.$0);
        _M0L6_2atmpS2970 = _M0L3endS2971 - _M0L5startS2972;
        _M0L6_2atmpS2966 = _M0L10size__hintS612 + _M0L6_2atmpS2970;
        _M0L3endS2968 = _M0L9separatorS616.$2;
        _M0L5startS2969 = _M0L9separatorS616.$1;
        _M0L6_2atmpS2967 = _M0L3endS2968 - _M0L5startS2969;
        _M0L6_2atmpS2965 = _M0L6_2atmpS2966 + _M0L6_2atmpS2967;
        _M0L2__S611 = _M0L6_2atmpS2964;
        _M0L10size__hintS612 = _M0L6_2atmpS2965;
        continue;
      } else {
        _M0L10size__hintS610 = _M0L10size__hintS612;
      }
      break;
    }
    _M0L10size__hintS617 = _M0L10size__hintS610 << 1;
    #line 1511 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0L3bufS618
    = _M0MPB13StringBuilder21StringBuilder_2einner(_M0L10size__hintS617);
    #line 1513 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS618, _M0L2hdS608);
    moonbit_decref(_M0L2hdS608.$0);
    _M0L3endS2958 = _M0L9separatorS616.$2;
    _M0L5startS2959 = _M0L9separatorS616.$1;
    _M0L6_2atmpS2957 = _M0L3endS2958 - _M0L5startS2959;
    if (_M0L6_2atmpS2957 == 0) {
      int32_t _M0L7_2abindS619 = _M0L9_2ax__endS607 - _M0L11_2ax__startS606;
      int32_t _M0L2__S620 = 0;
      while (1) {
        if (_M0L2__S620 < _M0L7_2abindS619) {
          int32_t _M0L6_2atmpS2961 = _M0L11_2ax__startS606 + _M0L2__S620;
          moonbit_string_t _M0L1sS621 =
            (moonbit_string_t)_M0L9_2ax__bufS605[_M0L6_2atmpS2961];
          struct _M0TPC16string10StringView _M0L1sS622;
          int32_t _M0L6_2atmpS2960;
          moonbit_incref(_M0L1sS621);
          #line 1517 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS622
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS621);
          moonbit_decref(_M0L1sS621);
          #line 1518 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS618, _M0L1sS622);
          moonbit_decref(_M0L1sS622.$0);
          _M0L6_2atmpS2960 = _M0L2__S620 + 1;
          _M0L2__S620 = _M0L6_2atmpS2960;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS605);
        }
        break;
      }
    } else {
      int32_t _M0L7_2abindS624 = _M0L9_2ax__endS607 - _M0L11_2ax__startS606;
      int32_t _M0L2__S625 = 0;
      while (1) {
        if (_M0L2__S625 < _M0L7_2abindS624) {
          int32_t _M0L6_2atmpS2963 = _M0L11_2ax__startS606 + _M0L2__S625;
          moonbit_string_t _M0L1sS626 =
            (moonbit_string_t)_M0L9_2ax__bufS605[_M0L6_2atmpS2963];
          struct _M0TPC16string10StringView _M0L1sS627;
          int32_t _M0L6_2atmpS2962;
          moonbit_incref(_M0L1sS626);
          #line 1522 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS627
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS626);
          moonbit_decref(_M0L1sS626);
          #line 1523 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS618, _M0L9separatorS616);
          #line 1525 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS618, _M0L1sS627);
          moonbit_decref(_M0L1sS627.$0);
          _M0L6_2atmpS2962 = _M0L2__S625 + 1;
          _M0L2__S625 = _M0L6_2atmpS2962;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS605);
        }
        break;
      }
    }
    #line 1528 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _result_5646 = _M0MPB13StringBuilder10to__string(_M0L3bufS618);
    moonbit_decref(_M0L3bufS618);
    return _result_5646;
  }
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS599,
  int32_t _M0L5indexS600
) {
  uint64_t* _M0L6_2atmpS2952;
  uint64_t _result_5647;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS599);
  _M0L6_2atmpS2952 = _M0L4selfS599;
  if (
    _M0L5indexS600 < 0
    || _M0L5indexS600 >= Moonbit_array_length(_M0L6_2atmpS2952)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_5647 = (uint64_t)_M0L6_2atmpS2952[_M0L5indexS600];
  moonbit_decref(_M0L6_2atmpS2952);
  return _result_5647;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS601,
  int32_t _M0L5indexS602
) {
  uint32_t* _M0L6_2atmpS2953;
  uint32_t _result_5648;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS601);
  _M0L6_2atmpS2953 = _M0L4selfS601;
  if (
    _M0L5indexS602 < 0
    || _M0L5indexS602 >= Moonbit_array_length(_M0L6_2atmpS2953)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_5648 = (uint32_t)_M0L6_2atmpS2953[_M0L5indexS602];
  moonbit_decref(_M0L6_2atmpS2953);
  return _result_5648;
}

moonbit_string_t _M0IPC16uint646UInt64PB4Show10to__string(
  uint64_t _M0L4selfS598
) {
  #line 50 "/home/developer/.moon/lib/core/builtin/show.mbt"
  #line 51 "/home/developer/.moon/lib/core/builtin/show.mbt"
  return _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS598, 10);
}

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t _M0L4selfS597) {
  #line 35 "/home/developer/.moon/lib/core/builtin/show.mbt"
  #line 36 "/home/developer/.moon/lib/core/builtin/show.mbt"
  return _M0MPC13int3Int18to__string_2einner(_M0L4selfS597, 10);
}

moonbit_string_t _M0IPC14bool4BoolPB4Show10to__string(int32_t _M0L4selfS596) {
  #line 26 "/home/developer/.moon/lib/core/builtin/show.mbt"
  if (_M0L4selfS596) {
    return (moonbit_string_t)moonbit_string_literal_143.data;
  } else {
    return (moonbit_string_t)moonbit_string_literal_144.data;
  }
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS595) {
  #line 2494 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  return (uint64_t)_M0L4selfS595;
}

struct _M0TPC16string10StringView _M0IPC16string6StringPB12ToStringView16to__string__view(
  moonbit_string_t _M0L4selfS594
) {
  int32_t _M0L6_2atmpS2951;
  #line 24 "/home/developer/.moon/lib/core/builtin/string_like.mbt"
  _M0L6_2atmpS2951 = Moonbit_array_length(_M0L4selfS594);
  moonbit_incref(_M0L4selfS594);
  return (struct _M0TPC16string10StringView){.$0 = _M0L4selfS594,
                                               .$1 = 0,
                                               .$2 = _M0L6_2atmpS2951};
}

moonbit_string_t _M0MPC16string6String9to__upper(
  moonbit_string_t _M0L4selfS577
) {
  struct _M0TWcEb* _M0L6_2atmpS2948;
  int64_t _M0L7_2abindS576;
  #line 1789 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS2948
  = (struct _M0TWcEb*)&_M0MPC16string6String9to__upperC2949l1791$closure.data;
  #line 1791 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L7_2abindS576
  = _M0MPC16string6String8find__by(_M0L4selfS577, _M0L6_2atmpS2948);
  moonbit_decref(_M0L6_2atmpS2948);
  if (_M0L7_2abindS576 == 4294967296ll) {
    moonbit_incref(_M0L4selfS577);
    return _M0L4selfS577;
  } else {
    int64_t _M0L7_2aSomeS579 = _M0L7_2abindS576;
    int32_t _M0L6_2aidxS580 = (int32_t)_M0L7_2aSomeS579;
    int32_t _M0L6_2atmpS2947 = Moonbit_array_length(_M0L4selfS577);
    struct _M0TPB13StringBuilder* _M0L3bufS581;
    int64_t _M0L6_2atmpS2946;
    struct _M0TPC16string10StringView _M0L4headS582;
    moonbit_string_t _M0L6_2atmpS2917;
    int32_t _M0L6_2atmpS2918;
    int32_t _M0L3endS2920;
    int32_t _M0L5startS2921;
    int32_t _M0L6_2atmpS2919;
    struct _M0TPC16string10StringView _M0L7_2abindS583;
    moonbit_string_t _M0L7_2abindS584;
    int32_t _M0L7_2abindS585;
    int32_t _M0L7_2abindS586;
    int32_t _M0L16_2astring__indexS587;
    moonbit_string_t _result_5654;
    #line 1794 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _M0L3bufS581
    = _M0MPB13StringBuilder21StringBuilder_2einner(_M0L6_2atmpS2947);
    _M0L6_2atmpS2946 = (int64_t)_M0L6_2aidxS580;
    #line 1795 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _M0L4headS582
    = _M0MPC16string6String12view_2einner(_M0L4selfS577, 0, _M0L6_2atmpS2946);
    #line 1796 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _M0L6_2atmpS2917 = _M0MPC16string10StringView4data(_M0L4headS582);
    #line 1796 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _M0L6_2atmpS2918
    = _M0MPC16string10StringView13start__offset(_M0L4headS582);
    _M0L3endS2920 = _M0L4headS582.$2;
    _M0L5startS2921 = _M0L4headS582.$1;
    moonbit_decref(_M0L4headS582.$0);
    _M0L6_2atmpS2919 = _M0L3endS2920 - _M0L5startS2921;
    #line 1796 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L3bufS581, _M0L6_2atmpS2917, _M0L6_2atmpS2918, _M0L6_2atmpS2919);
    moonbit_decref(_M0L6_2atmpS2917);
    #line 1797 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _M0L7_2abindS583
    = _M0MPC16string6String12view_2einner(_M0L4selfS577, _M0L6_2aidxS580, 4294967296ll);
    _M0L7_2abindS584 = _M0L7_2abindS583.$0;
    _M0L7_2abindS585 = _M0L7_2abindS583.$1;
    _M0L7_2abindS586 = _M0L7_2abindS583.$2;
    _M0L16_2astring__indexS587 = _M0L7_2abindS585;
    while (1) {
      if (_M0L16_2astring__indexS587 < _M0L7_2abindS586) {
        int32_t _M0L31_2adecoded__next__string__indexS589;
        int32_t _M0L16_2adecoded__charS590;
        int32_t _M0L7_2abindS592 =
          _M0L7_2abindS584[_M0L16_2astring__indexS587];
        int32_t _M0L6_2atmpS2927 = (int32_t)_M0L7_2abindS592;
        int32_t _if__result_5651;
        int32_t _if__result_5652;
        if (_M0L6_2atmpS2927 >= 55296) {
          int32_t _M0L6_2atmpS2926 = (int32_t)_M0L7_2abindS592;
          _if__result_5651 = _M0L6_2atmpS2926 <= 56319;
        } else {
          _if__result_5651 = 0;
        }
        if (_if__result_5651) {
          int32_t _M0L6_2atmpS2925 = _M0L16_2astring__indexS587 + 1;
          _if__result_5652 = _M0L6_2atmpS2925 < _M0L7_2abindS586;
        } else {
          _if__result_5652 = 0;
        }
        if (_if__result_5652) {
          int32_t _M0L6_2atmpS2942 = _M0L16_2astring__indexS587 + 1;
          int32_t _M0L7_2abindS593 = _M0L7_2abindS584[_M0L6_2atmpS2942];
          int32_t _M0L6_2atmpS2929 = (int32_t)_M0L7_2abindS593;
          int32_t _if__result_5653;
          if (_M0L6_2atmpS2929 >= 56320) {
            int32_t _M0L6_2atmpS2928 = (int32_t)_M0L7_2abindS593;
            _if__result_5653 = _M0L6_2atmpS2928 <= 57343;
          } else {
            _if__result_5653 = 0;
          }
          if (_if__result_5653) {
            int32_t _M0L6_2atmpS2930 = _M0L16_2astring__indexS587 + 2;
            int32_t _M0L6_2atmpS2938 = (int32_t)_M0L7_2abindS592;
            int32_t _M0L6_2atmpS2937 = _M0L6_2atmpS2938 - 55296;
            int32_t _M0L6_2atmpS2935 = _M0L6_2atmpS2937 * 1024;
            int32_t _M0L6_2atmpS2936 = (int32_t)_M0L7_2abindS593;
            int32_t _M0L6_2atmpS2934 = _M0L6_2atmpS2935 + _M0L6_2atmpS2936;
            int32_t _M0L6_2atmpS2933 = _M0L6_2atmpS2934 - 56320;
            int32_t _M0L6_2atmpS2932 = _M0L6_2atmpS2933 + 65536;
            int32_t _M0L6_2atmpS2931;
            #line 1797 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
            _M0L6_2atmpS2931
            = _M0MPC13int3Int16unsafe__to__char(_M0L6_2atmpS2932);
            _M0L31_2adecoded__next__string__indexS589 = _M0L6_2atmpS2930;
            _M0L16_2adecoded__charS590 = _M0L6_2atmpS2931;
            goto join_588;
          } else {
            int32_t _M0L6_2atmpS2939 = _M0L16_2astring__indexS587 + 1;
            int32_t _M0L6_2atmpS2941 = (int32_t)_M0L7_2abindS592;
            int32_t _M0L6_2atmpS2940;
            #line 1797 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
            _M0L6_2atmpS2940
            = _M0MPC13int3Int16unsafe__to__char(_M0L6_2atmpS2941);
            _M0L31_2adecoded__next__string__indexS589 = _M0L6_2atmpS2939;
            _M0L16_2adecoded__charS590 = _M0L6_2atmpS2940;
            goto join_588;
          }
        } else {
          int32_t _M0L6_2atmpS2943 = _M0L16_2astring__indexS587 + 1;
          int32_t _M0L6_2atmpS2945 = (int32_t)_M0L7_2abindS592;
          int32_t _M0L6_2atmpS2944;
          #line 1797 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
          _M0L6_2atmpS2944
          = _M0MPC13int3Int16unsafe__to__char(_M0L6_2atmpS2945);
          _M0L31_2adecoded__next__string__indexS589 = _M0L6_2atmpS2943;
          _M0L16_2adecoded__charS590 = _M0L6_2atmpS2944;
          goto join_588;
        }
        goto joinlet_5650;
        join_588:;
        #line 1798 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
        if (
          _M0MPC14char4Char20is__ascii__lowercase(_M0L16_2adecoded__charS590)
        ) {
          int32_t _M0L6_2atmpS2924 = _M0L16_2adecoded__charS590;
          int32_t _M0L6_2atmpS2923 = _M0L6_2atmpS2924 - 32;
          int32_t _M0L6_2atmpS2922 = _M0L6_2atmpS2923;
          #line 1799 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS581, _M0L6_2atmpS2922);
        } else {
          #line 1801 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS581, _M0L16_2adecoded__charS590);
        }
        _M0L16_2astring__indexS587
        = _M0L31_2adecoded__next__string__indexS589;
        continue;
        joinlet_5650:;
      } else {
        moonbit_decref(_M0L7_2abindS584);
      }
      break;
    }
    #line 1804 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _result_5654 = _M0MPB13StringBuilder10to__string(_M0L3bufS581);
    moonbit_decref(_M0L3bufS581);
    return _result_5654;
  }
}

int32_t _M0MPC16string6String9to__upperC2949l1791(
  struct _M0TWcEb* _M0L6_2aenvS2950,
  int32_t _M0L1cS578
) {
  #line 1791 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  #line 1791 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  return _M0MPC14char4Char20is__ascii__lowercase(_M0L1cS578);
}

int32_t _M0MPC14char4Char20is__ascii__lowercase(int32_t _M0L4selfS575) {
  #line 103 "/home/developer/.moon/lib/core/builtin/char.mbt"
  return _M0L4selfS575 >= 97 && _M0L4selfS575 <= 122 || 0;
}

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPC16string6String5split(
  moonbit_string_t _M0L4selfS573,
  struct _M0TPC16string10StringView _M0L3sepS574
) {
  int32_t _M0L6_2atmpS2916;
  struct _M0TPC16string10StringView _M0L6_2atmpS2915;
  struct _M0TPB4IterGRPC16string10StringViewE* _result_5655;
  #line 1168 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS2916 = Moonbit_array_length(_M0L4selfS573);
  moonbit_incref(_M0L4selfS573);
  _M0L6_2atmpS2915
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L4selfS573, .$1 = 0, .$2 = _M0L6_2atmpS2916
  };
  #line 1169 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _result_5655
  = _M0MPC16string10StringView5split(_M0L6_2atmpS2915, _M0L3sepS574);
  moonbit_decref(_M0L6_2atmpS2915.$0);
  return _result_5655;
}

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPC16string10StringView5split(
  struct _M0TPC16string10StringView _M0L4selfS564,
  struct _M0TPC16string10StringView _M0L3sepS563
) {
  int32_t _M0L3endS2913;
  int32_t _M0L5startS2914;
  int32_t _M0L8sep__lenS562;
  void* _M0L4SomeS2912;
  struct _M0TPB8MutLocalGORPC16string10StringViewE* _M0L9remainingS566;
  struct _M0R44StringView_3a_3asplit_2eanon__u2903__l1148__* _closure_5657;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS2902;
  struct _M0TPB4IterGRPC16string10StringViewE* _result_5658;
  #line 1139 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L3endS2913 = _M0L3sepS563.$2;
  _M0L5startS2914 = _M0L3sepS563.$1;
  _M0L8sep__lenS562 = _M0L3endS2913 - _M0L5startS2914;
  if (_M0L8sep__lenS562 == 0) {
    struct _M0TPB4IterGcE* _M0L6_2atmpS2897;
    struct _M0TWcERPC16string10StringView* _M0L6_2atmpS2898;
    struct _M0TPB4IterGRPC16string10StringViewE* _result_5656;
    #line 1145 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _M0L6_2atmpS2897 = _M0MPC16string10StringView4iter(_M0L4selfS564);
    _M0L6_2atmpS2898
    = (struct _M0TWcERPC16string10StringView*)&_M0MPC16string10StringView5splitC2899l1145$closure.data;
    #line 1145 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    _result_5656
    = _M0MPB4Iter3mapGcRPC16string10StringViewE(_M0L6_2atmpS2897, _M0L6_2atmpS2898);
    moonbit_decref(_M0L6_2atmpS2897);
    moonbit_decref(_M0L6_2atmpS2898);
    return _result_5656;
  }
  moonbit_incref(_M0L4selfS564.$0);
  _M0L4SomeS2912
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
  Moonbit_object_header(_M0L4SomeS2912)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 129, 1);
  ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2912)->$0
  = _M0L4selfS564;
  _M0L9remainingS566
  = (struct _M0TPB8MutLocalGORPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPC16string10StringViewE));
  Moonbit_object_header(_M0L9remainingS566)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 132, 0);
  _M0L9remainingS566->$0 = _M0L4SomeS2912;
  moonbit_incref(_M0L3sepS563.$0);
  _closure_5657
  = (struct _M0R44StringView_3a_3asplit_2eanon__u2903__l1148__*)moonbit_malloc(sizeof(struct _M0R44StringView_3a_3asplit_2eanon__u2903__l1148__));
  Moonbit_object_header(_closure_5657)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 135, 0);
  _closure_5657->code = &_M0MPC16string10StringView5splitC2903l1148;
  _closure_5657->$0 = _M0L9remainingS566;
  _closure_5657->$1 = _M0L3sepS563;
  _closure_5657->$2 = _M0L8sep__lenS562;
  _M0L6_2atmpS2902
  = (struct _M0TWERPC16option6OptionGRPC16string10StringViewE*)_closure_5657;
  #line 1148 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _result_5658
  = _M0MPB4Iter3newGRPC16string10StringViewE(_M0L6_2atmpS2902, 4294967296ll);
  moonbit_decref(_M0L6_2atmpS2902);
  return _result_5658;
}

void* _M0MPC16string10StringView5splitC2903l1148(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2aenvS2904
) {
  struct _M0R44StringView_3a_3asplit_2eanon__u2903__l1148__* _M0L14_2acasted__envS2905;
  int32_t _M0L8sep__lenS562;
  struct _M0TPC16string10StringView _M0L3sepS563;
  struct _M0TPB8MutLocalGORPC16string10StringViewE* _M0L9remainingS566;
  void* _M0L7_2abindS567;
  #line 1148 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L14_2acasted__envS2905
  = (struct _M0R44StringView_3a_3asplit_2eanon__u2903__l1148__*)_M0L6_2aenvS2904;
  _M0L8sep__lenS562 = _M0L14_2acasted__envS2905->$2;
  _M0L3sepS563 = _M0L14_2acasted__envS2905->$1;
  _M0L9remainingS566 = _M0L14_2acasted__envS2905->$0;
  _M0L7_2abindS567 = _M0L9remainingS566->$0;
  switch (Moonbit_object_tag(_M0L7_2abindS567)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS568 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L7_2abindS567;
      struct _M0TPC16string10StringView _M0L7_2aviewS569 =
        _M0L7_2aSomeS568->$0;
      int64_t _M0L7_2abindS570;
      moonbit_incref(_M0L7_2aviewS569.$0);
      #line 1150 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
      _M0L7_2abindS570
      = _M0MPC16string10StringView4find(_M0L7_2aviewS569, _M0L3sepS563);
      if (_M0L7_2abindS570 == 4294967296ll) {
        void* _M0L4NoneS2906 =
          (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        void* _M0L6_2aoldS5074 = _M0L9remainingS566->$0;
        void* _block_5659;
        moonbit_decref(_M0L6_2aoldS5074);
        _M0L9remainingS566->$0 = _M0L4NoneS2906;
        _block_5659
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
        Moonbit_object_header(_block_5659)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 129, 1);
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_5659)->$0
        = _M0L7_2aviewS569;
        return _block_5659;
      } else {
        int64_t _M0L7_2aSomeS571 = _M0L7_2abindS570;
        int32_t _M0L6_2aendS572 = (int32_t)_M0L7_2aSomeS571;
        int32_t _M0L6_2atmpS2909 = _M0L6_2aendS572 + _M0L8sep__lenS562;
        struct _M0TPC16string10StringView _M0L6_2atmpS2908;
        void* _M0L4SomeS2907;
        void* _M0L6_2aoldS5075;
        int64_t _M0L6_2atmpS2911;
        struct _M0TPC16string10StringView _M0L6_2atmpS2910;
        void* _block_5660;
        #line 1154 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
        _M0L6_2atmpS2908
        = _M0MPC16string10StringView12view_2einner(_M0L7_2aviewS569, _M0L6_2atmpS2909, 4294967296ll);
        _M0L4SomeS2907
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
        Moonbit_object_header(_M0L4SomeS2907)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 129, 1);
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2907)->$0
        = _M0L6_2atmpS2908;
        _M0L6_2aoldS5075 = _M0L9remainingS566->$0;
        moonbit_decref(_M0L6_2aoldS5075);
        _M0L9remainingS566->$0 = _M0L4SomeS2907;
        _M0L6_2atmpS2911 = (int64_t)_M0L6_2aendS572;
        #line 1155 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
        _M0L6_2atmpS2910
        = _M0MPC16string10StringView12view_2einner(_M0L7_2aviewS569, 0, _M0L6_2atmpS2911);
        moonbit_decref(_M0L7_2aviewS569.$0);
        _block_5660
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
        Moonbit_object_header(_block_5660)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 129, 1);
        ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_5660)->$0
        = _M0L6_2atmpS2910;
        return _block_5660;
      }
      break;
    }
    default: {
      return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      break;
    }
  }
}

struct _M0TPC16string10StringView _M0MPC16string10StringView5splitC2899l1145(
  struct _M0TWcERPC16string10StringView* _M0L6_2aenvS2900,
  int32_t _M0L1cS565
) {
  moonbit_string_t _M0L6_2atmpS2901;
  struct _M0TPC16string10StringView _result_5661;
  #line 1145 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  #line 1145 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS2901 = _M0IPC14char4CharPB4Show10to__string(_M0L1cS565);
  #line 1145 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _result_5661
  = _M0MPC16string6String12view_2einner(_M0L6_2atmpS2901, 0, 4294967296ll);
  moonbit_decref(_M0L6_2atmpS2901);
  return _result_5661;
}

moonbit_string_t _M0IPC14char4CharPB4Show10to__string(int32_t _M0L4selfS561) {
  #line 446 "/home/developer/.moon/lib/core/builtin/char.mbt"
  #line 447 "/home/developer/.moon/lib/core/builtin/char.mbt"
  return _M0FPB16char__to__string(_M0L4selfS561);
}

moonbit_string_t _M0FPB16char__to__string(int32_t _M0L4charS560) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS559;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS2896;
  moonbit_string_t _result_5662;
  #line 452 "/home/developer/.moon/lib/core/builtin/char.mbt"
  #line 454 "/home/developer/.moon/lib/core/builtin/char.mbt"
  _M0L7_2aselfS559 = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 454 "/home/developer/.moon/lib/core/builtin/char.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS559, _M0L4charS560);
  _M0L6_2atmpS2896 = _M0L7_2aselfS559;
  #line 454 "/home/developer/.moon/lib/core/builtin/char.mbt"
  _result_5662 = _M0MPB13StringBuilder10to__string(_M0L6_2atmpS2896);
  moonbit_decref(_M0L6_2atmpS2896);
  return _result_5662;
}

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPB4Iter3mapGcRPC16string10StringViewE(
  struct _M0TPB4IterGcE* _M0L4selfS555,
  struct _M0TWcERPC16string10StringView* _M0L1fS558
) {
  struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2892__l391__* _closure_5663;
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2atmpS2890;
  int64_t _M0L10size__hintS2891;
  struct _M0TPB4IterGRPC16string10StringViewE* _block_5664;
  #line 389 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  moonbit_incref(_M0L1fS558);
  moonbit_incref(_M0L4selfS555);
  _closure_5663
  = (struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2892__l391__*)moonbit_malloc(sizeof(struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2892__l391__));
  Moonbit_object_header(_closure_5663)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 139, 0);
  _closure_5663->code = &_M0MPB4Iter3mapGcRPC16string10StringViewEC2892l391;
  _closure_5663->$0 = _M0L1fS558;
  _closure_5663->$1 = _M0L4selfS555;
  _M0L6_2atmpS2890
  = (struct _M0TWERPC16option6OptionGRPC16string10StringViewE*)_closure_5663;
  _M0L10size__hintS2891 = _M0L4selfS555->$1;
  _block_5664
  = (struct _M0TPB4IterGRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB4IterGRPC16string10StringViewE));
  Moonbit_object_header(_block_5664)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 143, 0);
  _block_5664->$0 = _M0L6_2atmpS2890;
  _block_5664->$1 = _M0L10size__hintS2891;
  return _block_5664;
}

void* _M0MPB4Iter3mapGcRPC16string10StringViewEC2892l391(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L6_2aenvS2893
) {
  struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2892__l391__* _M0L14_2acasted__envS2894;
  struct _M0TPB4IterGcE* _M0L4selfS555;
  struct _M0TWcERPC16string10StringView* _M0L1fS558;
  int32_t _M0L7_2abindS554;
  #line 391 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L14_2acasted__envS2894
  = (struct _M0R97Iter_3a_3amap_7c_5bChar_2c_20moonbitlang_2fcore_2fstring_2fStringView_5d_7c_2eanon__u2892__l391__*)_M0L6_2aenvS2893;
  _M0L4selfS555 = _M0L14_2acasted__envS2894->$1;
  _M0L1fS558 = _M0L14_2acasted__envS2894->$0;
  #line 392 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2abindS554 = _M0MPB4Iter4nextGcE(_M0L4selfS555);
  if (_M0L7_2abindS554 == -1) {
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    int32_t _M0L7_2aSomeS556 = _M0L7_2abindS554;
    int32_t _M0L4_2axS557 = _M0L7_2aSomeS556;
    struct _M0TPC16string10StringView _M0L6_2atmpS2895;
    void* _block_5665;
    #line 393 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
    _M0L6_2atmpS2895 = _M0L1fS558->code(_M0L1fS558, _M0L4_2axS557);
    _block_5665
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
    Moonbit_object_header(_block_5665)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 129, 1);
    ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_block_5665)->$0
    = _M0L6_2atmpS2895;
    return _block_5665;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS545,
  moonbit_string_t _M0L5valueS547
) {
  int32_t _M0L3lenS2875;
  moonbit_string_t* _M0L6_2atmpS2877;
  int32_t _M0L6_2atmpS2876;
  int32_t _M0L6lengthS546;
  moonbit_string_t* _M0L3bufS2878;
  moonbit_string_t _M0L6_2aoldS5078;
  int32_t _M0L6_2atmpS2879;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2875 = _M0L4selfS545->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2877 = _M0MPC15array5Array6bufferGsE(_M0L4selfS545);
  _M0L6_2atmpS2876 = Moonbit_array_length(_M0L6_2atmpS2877);
  moonbit_decref(_M0L6_2atmpS2877);
  if (_M0L3lenS2875 == _M0L6_2atmpS2876) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS545);
  }
  _M0L6lengthS546 = _M0L4selfS545->$1;
  _M0L3bufS2878 = _M0L4selfS545->$0;
  _M0L6_2aoldS5078 = (moonbit_string_t)_M0L3bufS2878[_M0L6lengthS546];
  moonbit_incref(_M0L5valueS547);
  moonbit_decref(_M0L6_2aoldS5078);
  _M0L3bufS2878[_M0L6lengthS546] = _M0L5valueS547;
  _M0L6_2atmpS2879 = _M0L6lengthS546 + 1;
  _M0L4selfS545->$1 = _M0L6_2atmpS2879;
  return 0;
}

int32_t _M0MPC15array5Array4pushGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS548,
  moonbit_string_t _M0L5valueS550
) {
  int32_t _M0L3lenS2880;
  moonbit_string_t* _M0L6_2atmpS2882;
  int32_t _M0L6_2atmpS2881;
  int32_t _M0L6lengthS549;
  moonbit_string_t* _M0L3bufS2883;
  moonbit_string_t _M0L6_2aoldS5080;
  int32_t _M0L6_2atmpS2884;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2880 = _M0L4selfS548->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2882 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS548);
  _M0L6_2atmpS2881 = Moonbit_array_length(_M0L6_2atmpS2882);
  moonbit_decref(_M0L6_2atmpS2882);
  if (_M0L3lenS2880 == _M0L6_2atmpS2881) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGOsE(_M0L4selfS548);
  }
  _M0L6lengthS549 = _M0L4selfS548->$1;
  _M0L3bufS2883 = _M0L4selfS548->$0;
  _M0L6_2aoldS5080 = (moonbit_string_t)_M0L3bufS2883[_M0L6lengthS549];
  if (_M0L5valueS550) {
    moonbit_incref(_M0L5valueS550);
  }
  if (_M0L6_2aoldS5080) {
    moonbit_decref(_M0L6_2aoldS5080);
  }
  _M0L3bufS2883[_M0L6lengthS549] = _M0L5valueS550;
  _M0L6_2atmpS2884 = _M0L6lengthS549 + 1;
  _M0L4selfS548->$1 = _M0L6_2atmpS2884;
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS551,
  struct _M0TUsfE* _M0L5valueS553
) {
  int32_t _M0L3lenS2885;
  struct _M0TUsfE** _M0L6_2atmpS2887;
  int32_t _M0L6_2atmpS2886;
  int32_t _M0L6lengthS552;
  struct _M0TUsfE** _M0L3bufS2888;
  struct _M0TUsfE* _M0L6_2aoldS5082;
  int32_t _M0L6_2atmpS2889;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2885 = _M0L4selfS551->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2887 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS551);
  _M0L6_2atmpS2886 = Moonbit_array_length(_M0L6_2atmpS2887);
  moonbit_decref(_M0L6_2atmpS2887);
  if (_M0L3lenS2885 == _M0L6_2atmpS2886) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsfEE(_M0L4selfS551);
  }
  _M0L6lengthS552 = _M0L4selfS551->$1;
  _M0L3bufS2888 = _M0L4selfS551->$0;
  _M0L6_2aoldS5082 = (struct _M0TUsfE*)_M0L3bufS2888[_M0L6lengthS552];
  moonbit_incref(_M0L5valueS553);
  if (_M0L6_2aoldS5082) {
    moonbit_decref(_M0L6_2aoldS5082);
  }
  _M0L3bufS2888[_M0L6lengthS552] = _M0L5valueS553;
  _M0L6_2atmpS2889 = _M0L6lengthS552 + 1;
  _M0L4selfS551->$1 = _M0L6_2atmpS2889;
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS537) {
  int32_t _M0L8old__capS536;
  int32_t _M0L8new__capS538;
  #line 245 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS536 = _M0L4selfS537->$1;
  if (_M0L8old__capS536 == 0) {
    _M0L8new__capS538 = 8;
  } else {
    _M0L8new__capS538 = _M0L8old__capS536 * 2;
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS537, _M0L8new__capS538);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS540
) {
  int32_t _M0L8old__capS539;
  int32_t _M0L8new__capS541;
  #line 245 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS539 = _M0L4selfS540->$1;
  if (_M0L8old__capS539 == 0) {
    _M0L8new__capS541 = 8;
  } else {
    _M0L8new__capS541 = _M0L8old__capS539 * 2;
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGOsE(_M0L4selfS540, _M0L8new__capS541);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS543
) {
  int32_t _M0L8old__capS542;
  int32_t _M0L8new__capS544;
  #line 245 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS542 = _M0L4selfS543->$1;
  if (_M0L8old__capS542 == 0) {
    _M0L8new__capS544 = 8;
  } else {
    _M0L8new__capS544 = _M0L8old__capS542 * 2;
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsfEE(_M0L4selfS543, _M0L8new__capS544);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS519,
  int32_t _M0L13new__capacityS522
) {
  moonbit_string_t* _M0L8old__bufS518;
  int32_t _M0L8old__capS520;
  int32_t _M0L9copy__lenS521;
  moonbit_string_t* _M0L8new__bufS523;
  moonbit_string_t* _M0L6_2aoldS5084;
  #line 189 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__bufS518 = _M0L4selfS519->$0;
  _M0L8old__capS520 = Moonbit_array_length(_M0L8old__bufS518);
  if (_M0L8old__capS520 < _M0L13new__capacityS522) {
    _M0L9copy__lenS521 = _M0L8old__capS520;
  } else {
    _M0L9copy__lenS521 = _M0L13new__capacityS522;
  }
  moonbit_incref(_M0L8old__bufS518);
  #line 193 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS523
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(_M0L8old__bufS518, _M0L13new__capacityS522, _M0L9copy__lenS521, 0, 0);
  moonbit_decref(_M0L8old__bufS518);
  _M0L6_2aoldS5084 = _M0L4selfS519->$0;
  moonbit_decref(_M0L6_2aoldS5084);
  _M0L4selfS519->$0 = _M0L8new__bufS523;
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS525,
  int32_t _M0L13new__capacityS528
) {
  moonbit_string_t* _M0L8old__bufS524;
  int32_t _M0L8old__capS526;
  int32_t _M0L9copy__lenS527;
  moonbit_string_t* _M0L8new__bufS529;
  moonbit_string_t* _M0L6_2aoldS5086;
  #line 189 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__bufS524 = _M0L4selfS525->$0;
  _M0L8old__capS526 = Moonbit_array_length(_M0L8old__bufS524);
  if (_M0L8old__capS526 < _M0L13new__capacityS528) {
    _M0L9copy__lenS527 = _M0L8old__capS526;
  } else {
    _M0L9copy__lenS527 = _M0L13new__capacityS528;
  }
  moonbit_incref(_M0L8old__bufS524);
  #line 193 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS529
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGOsE(_M0L8old__bufS524, _M0L13new__capacityS528, _M0L9copy__lenS527, 0, 0);
  moonbit_decref(_M0L8old__bufS524);
  _M0L6_2aoldS5086 = _M0L4selfS525->$0;
  moonbit_decref(_M0L6_2aoldS5086);
  _M0L4selfS525->$0 = _M0L8new__bufS529;
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS531,
  int32_t _M0L13new__capacityS534
) {
  struct _M0TUsfE** _M0L8old__bufS530;
  int32_t _M0L8old__capS532;
  int32_t _M0L9copy__lenS533;
  struct _M0TUsfE** _M0L8new__bufS535;
  struct _M0TUsfE** _M0L6_2aoldS5088;
  #line 189 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__bufS530 = _M0L4selfS531->$0;
  _M0L8old__capS532 = Moonbit_array_length(_M0L8old__bufS530);
  if (_M0L8old__capS532 < _M0L13new__capacityS534) {
    _M0L9copy__lenS533 = _M0L8old__capS532;
  } else {
    _M0L9copy__lenS533 = _M0L13new__capacityS534;
  }
  moonbit_incref(_M0L8old__bufS530);
  #line 193 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS535
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(_M0L8old__bufS530, _M0L13new__capacityS534, _M0L9copy__lenS533, 0, 0);
  moonbit_decref(_M0L8old__bufS530);
  _M0L6_2aoldS5088 = _M0L4selfS531->$0;
  moonbit_decref(_M0L6_2aoldS5088);
  _M0L4selfS531->$0 = _M0L8new__bufS535;
  return 0;
}

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE* _M0L4selfS515) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS515->$1;
}

int32_t _M0MPC15array5Array6lengthGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS516
) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS516->$1;
}

int32_t _M0MPC15array5Array6lengthGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS517
) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS517->$1;
}

int64_t _M0MPC16string6String8find__by(
  moonbit_string_t _M0L4selfS513,
  struct _M0TWcEb* _M0L4predS514
) {
  int32_t _M0L6_2atmpS2874;
  struct _M0TPC16string10StringView _M0L6_2atmpS2873;
  int64_t _result_5666;
  #line 144 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L6_2atmpS2874 = Moonbit_array_length(_M0L4selfS513);
  moonbit_incref(_M0L4selfS513);
  _M0L6_2atmpS2873
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L4selfS513, .$1 = 0, .$2 = _M0L6_2atmpS2874
  };
  #line 145 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _result_5666
  = _M0MPC16string10StringView8find__by(_M0L6_2atmpS2873, _M0L4predS514);
  moonbit_decref(_M0L6_2atmpS2873.$0);
  return _result_5666;
}

int64_t _M0MPC16string10StringView8find__by(
  struct _M0TPC16string10StringView _M0L4selfS500,
  struct _M0TWcEb* _M0L4predS509
) {
  moonbit_string_t _M0L7_2abindS499;
  int32_t _M0L7_2abindS501;
  int32_t _M0L7_2abindS502;
  int32_t _M0L16_2astring__indexS503;
  int32_t _M0L1iS504;
  #line 131 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L7_2abindS499 = _M0L4selfS500.$0;
  _M0L7_2abindS501 = _M0L4selfS500.$1;
  _M0L7_2abindS502 = _M0L4selfS500.$2;
  moonbit_incref(_M0L7_2abindS499);
  _M0L16_2astring__indexS503 = _M0L7_2abindS501;
  _M0L1iS504 = 0;
  while (1) {
    if (_M0L16_2astring__indexS503 < _M0L7_2abindS502) {
      int32_t _M0L31_2adecoded__next__string__indexS506;
      int32_t _M0L16_2adecoded__charS507;
      int32_t _M0L7_2abindS511 = _M0L7_2abindS499[_M0L16_2astring__indexS503];
      int32_t _M0L6_2atmpS2854 = (int32_t)_M0L7_2abindS511;
      int32_t _if__result_5669;
      int32_t _if__result_5670;
      int32_t _M0L20_2anext__char__indexS508;
      if (_M0L6_2atmpS2854 >= 55296) {
        int32_t _M0L6_2atmpS2853 = (int32_t)_M0L7_2abindS511;
        _if__result_5669 = _M0L6_2atmpS2853 <= 56319;
      } else {
        _if__result_5669 = 0;
      }
      if (_if__result_5669) {
        int32_t _M0L6_2atmpS2852 = _M0L16_2astring__indexS503 + 1;
        _if__result_5670 = _M0L6_2atmpS2852 < _M0L7_2abindS502;
      } else {
        _if__result_5670 = 0;
      }
      if (_if__result_5670) {
        int32_t _M0L6_2atmpS2869 = _M0L16_2astring__indexS503 + 1;
        int32_t _M0L7_2abindS512 = _M0L7_2abindS499[_M0L6_2atmpS2869];
        int32_t _M0L6_2atmpS2856 = (int32_t)_M0L7_2abindS512;
        int32_t _if__result_5671;
        if (_M0L6_2atmpS2856 >= 56320) {
          int32_t _M0L6_2atmpS2855 = (int32_t)_M0L7_2abindS512;
          _if__result_5671 = _M0L6_2atmpS2855 <= 57343;
        } else {
          _if__result_5671 = 0;
        }
        if (_if__result_5671) {
          int32_t _M0L6_2atmpS2857 = _M0L16_2astring__indexS503 + 2;
          int32_t _M0L6_2atmpS2865 = (int32_t)_M0L7_2abindS511;
          int32_t _M0L6_2atmpS2864 = _M0L6_2atmpS2865 - 55296;
          int32_t _M0L6_2atmpS2862 = _M0L6_2atmpS2864 * 1024;
          int32_t _M0L6_2atmpS2863 = (int32_t)_M0L7_2abindS512;
          int32_t _M0L6_2atmpS2861 = _M0L6_2atmpS2862 + _M0L6_2atmpS2863;
          int32_t _M0L6_2atmpS2860 = _M0L6_2atmpS2861 - 56320;
          int32_t _M0L6_2atmpS2859 = _M0L6_2atmpS2860 + 65536;
          int32_t _M0L6_2atmpS2858;
          #line 133 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
          _M0L6_2atmpS2858
          = _M0MPC13int3Int16unsafe__to__char(_M0L6_2atmpS2859);
          _M0L31_2adecoded__next__string__indexS506 = _M0L6_2atmpS2857;
          _M0L16_2adecoded__charS507 = _M0L6_2atmpS2858;
          goto join_505;
        } else {
          int32_t _M0L6_2atmpS2866 = _M0L16_2astring__indexS503 + 1;
          int32_t _M0L6_2atmpS2868 = (int32_t)_M0L7_2abindS511;
          int32_t _M0L6_2atmpS2867;
          #line 133 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
          _M0L6_2atmpS2867
          = _M0MPC13int3Int16unsafe__to__char(_M0L6_2atmpS2868);
          _M0L31_2adecoded__next__string__indexS506 = _M0L6_2atmpS2866;
          _M0L16_2adecoded__charS507 = _M0L6_2atmpS2867;
          goto join_505;
        }
      } else {
        int32_t _M0L6_2atmpS2870 = _M0L16_2astring__indexS503 + 1;
        int32_t _M0L6_2atmpS2872 = (int32_t)_M0L7_2abindS511;
        int32_t _M0L6_2atmpS2871;
        #line 133 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
        _M0L6_2atmpS2871
        = _M0MPC13int3Int16unsafe__to__char(_M0L6_2atmpS2872);
        _M0L31_2adecoded__next__string__indexS506 = _M0L6_2atmpS2870;
        _M0L16_2adecoded__charS507 = _M0L6_2atmpS2871;
        goto join_505;
      }
      goto joinlet_5668;
      join_505:;
      _M0L20_2anext__char__indexS508 = _M0L1iS504 + 1;
      #line 134 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
      if (_M0L4predS509->code(_M0L4predS509, _M0L16_2adecoded__charS507)) {
        moonbit_decref(_M0L7_2abindS499);
        return (int64_t)_M0L1iS504;
      }
      _M0L16_2astring__indexS503 = _M0L31_2adecoded__next__string__indexS506;
      _M0L1iS504 = _M0L20_2anext__char__indexS508;
      continue;
      joinlet_5668:;
    } else {
      moonbit_decref(_M0L7_2abindS499);
    }
    break;
  }
  return 4294967296ll;
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS498
) {
  #line 222 "/home/developer/.moon/lib/core/builtin/show.mbt"
  moonbit_incref(_M0L4selfS498);
  return _M0L4selfS498;
}

int64_t _M0MPC16string10StringView4find(
  struct _M0TPC16string10StringView _M0L4selfS497,
  struct _M0TPC16string10StringView _M0L3strS496
) {
  int32_t _M0L3endS2850;
  int32_t _M0L5startS2851;
  int32_t _M0L6_2atmpS2849;
  #line 18 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L3endS2850 = _M0L3strS496.$2;
  _M0L5startS2851 = _M0L3strS496.$1;
  _M0L6_2atmpS2849 = _M0L3endS2850 - _M0L5startS2851;
  if (_M0L6_2atmpS2849 <= 4) {
    #line 20 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    return _M0FPB18brute__force__find(_M0L4selfS497, _M0L3strS496);
  } else {
    #line 22 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
    return _M0FPB28boyer__moore__horspool__find(_M0L4selfS497, _M0L3strS496);
  }
}

int64_t _M0FPB18brute__force__find(
  struct _M0TPC16string10StringView _M0L8haystackS486,
  struct _M0TPC16string10StringView _M0L6needleS488
) {
  int32_t _M0L3endS2847;
  int32_t _M0L5startS2848;
  int32_t _M0L13haystack__lenS485;
  int32_t _M0L3endS2845;
  int32_t _M0L5startS2846;
  int32_t _M0L11needle__lenS487;
  #line 31 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L3endS2847 = _M0L8haystackS486.$2;
  _M0L5startS2848 = _M0L8haystackS486.$1;
  _M0L13haystack__lenS485 = _M0L3endS2847 - _M0L5startS2848;
  _M0L3endS2845 = _M0L6needleS488.$2;
  _M0L5startS2846 = _M0L6needleS488.$1;
  _M0L11needle__lenS487 = _M0L3endS2845 - _M0L5startS2846;
  if (_M0L11needle__lenS487 > 0) {
    if (_M0L13haystack__lenS485 >= _M0L11needle__lenS487) {
      moonbit_string_t _M0L3strS2843 = _M0L6needleS488.$0;
      int32_t _M0L5startS2844 = _M0L6needleS488.$1;
      int32_t _M0L13needle__firstS489 = _M0L3strS2843[_M0L5startS2844];
      int32_t _M0L12forward__lenS490 =
        _M0L13haystack__lenS485 - _M0L11needle__lenS487;
      int32_t _M0L1iS491 = 0;
      while (1) {
        if (_M0L1iS491 <= _M0L12forward__lenS490) {
          moonbit_string_t _M0L3strS2830 = _M0L8haystackS486.$0;
          int32_t _M0L5startS2832 = _M0L8haystackS486.$1;
          int32_t _M0L6_2atmpS2831 = _M0L5startS2832 + _M0L1iS491;
          int32_t _M0L6_2atmpS2829 = _M0L3strS2830[_M0L6_2atmpS2831];
          int32_t _M0L1jS494;
          int32_t _M0L6_2atmpS2828;
          if (_M0L6_2atmpS2829 != _M0L13needle__firstS489) {
            goto join_492;
          }
          _M0L1jS494 = 1;
          while (1) {
            if (_M0L1jS494 < _M0L11needle__lenS487) {
              moonbit_string_t _M0L3strS2838 = _M0L8haystackS486.$0;
              int32_t _M0L5startS2840 = _M0L8haystackS486.$1;
              int32_t _M0L6_2atmpS2841 = _M0L1iS491 + _M0L1jS494;
              int32_t _M0L6_2atmpS2839 = _M0L5startS2840 + _M0L6_2atmpS2841;
              int32_t _M0L6_2atmpS2833 = _M0L3strS2838[_M0L6_2atmpS2839];
              moonbit_string_t _M0L3strS2835 = _M0L6needleS488.$0;
              int32_t _M0L5startS2837 = _M0L6needleS488.$1;
              int32_t _M0L6_2atmpS2836 = _M0L5startS2837 + _M0L1jS494;
              int32_t _M0L6_2atmpS2834 = _M0L3strS2835[_M0L6_2atmpS2836];
              int32_t _M0L6_2atmpS2842;
              if (_M0L6_2atmpS2833 != _M0L6_2atmpS2834) {
                break;
              }
              _M0L6_2atmpS2842 = _M0L1jS494 + 1;
              _M0L1jS494 = _M0L6_2atmpS2842;
              continue;
            } else {
              return (int64_t)_M0L1iS491;
            }
            break;
          }
          goto join_492;
          goto joinlet_5673;
          join_492:;
          _M0L6_2atmpS2828 = _M0L1iS491 + 1;
          _M0L1iS491 = _M0L6_2atmpS2828;
          continue;
          joinlet_5673:;
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
  struct _M0TPC16string10StringView _M0L8haystackS473,
  struct _M0TPC16string10StringView _M0L6needleS475
) {
  int32_t _M0L3endS2826;
  int32_t _M0L5startS2827;
  int32_t _M0L13haystack__lenS472;
  int32_t _M0L3endS2824;
  int32_t _M0L5startS2825;
  int32_t _M0L11needle__lenS474;
  #line 57 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
  _M0L3endS2826 = _M0L8haystackS473.$2;
  _M0L5startS2827 = _M0L8haystackS473.$1;
  _M0L13haystack__lenS472 = _M0L3endS2826 - _M0L5startS2827;
  _M0L3endS2824 = _M0L6needleS475.$2;
  _M0L5startS2825 = _M0L6needleS475.$1;
  _M0L11needle__lenS474 = _M0L3endS2824 - _M0L5startS2825;
  if (_M0L11needle__lenS474 > 0) {
    if (_M0L13haystack__lenS472 >= _M0L11needle__lenS474) {
      int32_t* _M0L11skip__tableS476 =
        (int32_t*)moonbit_make_int32_array(256, _M0L11needle__lenS474);
      int32_t _M0L7_2abindS477 = _M0L11needle__lenS474 - 1;
      int32_t _M0L1iS478 = 0;
      int32_t _M0L1iS480;
      while (1) {
        if (_M0L1iS478 < _M0L7_2abindS477) {
          moonbit_string_t _M0L3strS2799 = _M0L6needleS475.$0;
          int32_t _M0L5startS2801 = _M0L6needleS475.$1;
          int32_t _M0L6_2atmpS2800 = _M0L5startS2801 + _M0L1iS478;
          int32_t _M0L6_2atmpS2798 = _M0L3strS2799[_M0L6_2atmpS2800];
          int32_t _M0L6_2atmpS2797 = (int32_t)_M0L6_2atmpS2798;
          int32_t _M0L6_2atmpS2794 = _M0L6_2atmpS2797 & 255;
          int32_t _M0L6_2atmpS2796 = _M0L11needle__lenS474 - 1;
          int32_t _M0L6_2atmpS2795 = _M0L6_2atmpS2796 - _M0L1iS478;
          int32_t _M0L6_2atmpS2802;
          if (
            _M0L6_2atmpS2794 < 0
            || _M0L6_2atmpS2794
               >= Moonbit_array_length(_M0L11skip__tableS476)
          ) {
            #line 68 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
            moonbit_panic();
          }
          _M0L11skip__tableS476[_M0L6_2atmpS2794] = _M0L6_2atmpS2795;
          _M0L6_2atmpS2802 = _M0L1iS478 + 1;
          _M0L1iS478 = _M0L6_2atmpS2802;
          continue;
        }
        break;
      }
      _M0L1iS480 = 0;
      while (1) {
        int32_t _M0L6_2atmpS2803 =
          _M0L13haystack__lenS472 - _M0L11needle__lenS474;
        if (_M0L1iS480 <= _M0L6_2atmpS2803) {
          int32_t _M0L7_2abindS481 = _M0L11needle__lenS474 - 1;
          int32_t _M0L1jS482 = 0;
          moonbit_string_t _M0L3strS2819;
          int32_t _M0L5startS2821;
          int32_t _M0L6_2atmpS2823;
          int32_t _M0L6_2atmpS2822;
          int32_t _M0L6_2atmpS2820;
          int32_t _M0L6_2atmpS2818;
          int32_t _M0L6_2atmpS2817;
          int32_t _M0L6_2atmpS2816;
          int32_t _M0L6_2atmpS2815;
          int32_t _M0L6_2atmpS2814;
          while (1) {
            if (_M0L1jS482 <= _M0L7_2abindS481) {
              moonbit_string_t _M0L3strS2809 = _M0L8haystackS473.$0;
              int32_t _M0L5startS2811 = _M0L8haystackS473.$1;
              int32_t _M0L6_2atmpS2812 = _M0L1iS480 + _M0L1jS482;
              int32_t _M0L6_2atmpS2810 = _M0L5startS2811 + _M0L6_2atmpS2812;
              int32_t _M0L6_2atmpS2804 = _M0L3strS2809[_M0L6_2atmpS2810];
              moonbit_string_t _M0L3strS2806 = _M0L6needleS475.$0;
              int32_t _M0L5startS2808 = _M0L6needleS475.$1;
              int32_t _M0L6_2atmpS2807 = _M0L5startS2808 + _M0L1jS482;
              int32_t _M0L6_2atmpS2805 = _M0L3strS2806[_M0L6_2atmpS2807];
              int32_t _M0L6_2atmpS2813;
              if (_M0L6_2atmpS2804 != _M0L6_2atmpS2805) {
                break;
              }
              _M0L6_2atmpS2813 = _M0L1jS482 + 1;
              _M0L1jS482 = _M0L6_2atmpS2813;
              continue;
            } else {
              moonbit_decref(_M0L11skip__tableS476);
              return (int64_t)_M0L1iS480;
            }
            break;
          }
          _M0L3strS2819 = _M0L8haystackS473.$0;
          _M0L5startS2821 = _M0L8haystackS473.$1;
          _M0L6_2atmpS2823 = _M0L1iS480 + _M0L11needle__lenS474;
          _M0L6_2atmpS2822 = _M0L6_2atmpS2823 - 1;
          _M0L6_2atmpS2820 = _M0L5startS2821 + _M0L6_2atmpS2822;
          _M0L6_2atmpS2818 = _M0L3strS2819[_M0L6_2atmpS2820];
          _M0L6_2atmpS2817 = (int32_t)_M0L6_2atmpS2818;
          _M0L6_2atmpS2816 = _M0L6_2atmpS2817 & 255;
          if (
            _M0L6_2atmpS2816 < 0
            || _M0L6_2atmpS2816
               >= Moonbit_array_length(_M0L11skip__tableS476)
          ) {
            #line 73 "/home/developer/.moon/lib/core/builtin/string_methods.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS2815 = (int32_t)_M0L11skip__tableS476[_M0L6_2atmpS2816];
          _M0L6_2atmpS2814 = _M0L1iS480 + _M0L6_2atmpS2815;
          _M0L1iS480 = _M0L6_2atmpS2814;
          continue;
        } else {
          moonbit_decref(_M0L11skip__tableS476);
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
  struct _M0TPB13StringBuilder* _M0L4selfS471,
  struct _M0TPC16string10StringView _M0L3strS470
) {
  int32_t _M0L3endS2792;
  int32_t _M0L5startS2793;
  int32_t _M0L8str__lenS469;
  int32_t _M0L3lenS2785;
  int32_t _M0L6_2atmpS2784;
  uint16_t* _M0L4dataS2786;
  int32_t _M0L3lenS2787;
  moonbit_string_t _M0L6_2atmpS2788;
  int32_t _M0L6_2atmpS2789;
  int32_t _M0L3lenS2791;
  int32_t _M0L6_2atmpS2790;
  #line 131 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3endS2792 = _M0L3strS470.$2;
  _M0L5startS2793 = _M0L3strS470.$1;
  _M0L8str__lenS469 = _M0L3endS2792 - _M0L5startS2793;
  _M0L3lenS2785 = _M0L4selfS471->$1;
  _M0L6_2atmpS2784 = _M0L3lenS2785 + _M0L8str__lenS469;
  #line 136 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS471, _M0L6_2atmpS2784);
  _M0L4dataS2786 = _M0L4selfS471->$0;
  _M0L3lenS2787 = _M0L4selfS471->$1;
  moonbit_incref(_M0L4dataS2786);
  #line 139 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2788 = _M0MPC16string10StringView4data(_M0L3strS470);
  #line 140 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2789 = _M0MPC16string10StringView13start__offset(_M0L3strS470);
  #line 137 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2786, _M0L3lenS2787, _M0L6_2atmpS2788, _M0L6_2atmpS2789, _M0L8str__lenS469);
  moonbit_decref(_M0L4dataS2786);
  moonbit_decref(_M0L6_2atmpS2788);
  _M0L3lenS2791 = _M0L4selfS471->$1;
  _M0L6_2atmpS2790 = _M0L3lenS2791 + _M0L8str__lenS469;
  _M0L4selfS471->$1 = _M0L6_2atmpS2790;
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS467,
  int32_t _M0L13start__offsetS468,
  int64_t _M0L11end__offsetS465
) {
  int32_t _M0L11end__offsetS464;
  int32_t _if__result_5678;
  #line 614 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  if (_M0L11end__offsetS465 == 4294967296ll) {
    _M0L11end__offsetS464 = Moonbit_array_length(_M0L4selfS467);
  } else {
    int64_t _M0L7_2aSomeS466 = _M0L11end__offsetS465;
    _M0L11end__offsetS464 = (int32_t)_M0L7_2aSomeS466;
  }
  if (_M0L13start__offsetS468 >= 0) {
    if (_M0L13start__offsetS468 <= _M0L11end__offsetS464) {
      int32_t _M0L6_2atmpS2783 = Moonbit_array_length(_M0L4selfS467);
      _if__result_5678 = _M0L11end__offsetS464 <= _M0L6_2atmpS2783;
    } else {
      _if__result_5678 = 0;
    }
  } else {
    _if__result_5678 = 0;
  }
  if (_if__result_5678) {
    moonbit_incref(_M0L4selfS467);
    return (struct _M0TPC16string10StringView){.$0 = _M0L4selfS467,
                                                 .$1 = _M0L13start__offsetS468,
                                                 .$2 = _M0L11end__offsetS464};
  } else {
    #line 623 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
    return _M0FPC15abort5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_145.data);
  }
}

struct _M0TPB4IterGcE* _M0MPC16string10StringView4iter(
  struct _M0TPC16string10StringView _M0L4selfS459
) {
  int32_t _M0L5startS458;
  int32_t _M0L3endS460;
  struct _M0TPB8MutLocalGiE* _M0L5indexS461;
  struct _M0R42StringView_3a_3aiter_2eanon__u2763__l219__* _closure_5679;
  struct _M0TWEOc* _M0L6_2atmpS2762;
  struct _M0TPB4IterGcE* _result_5680;
  #line 214 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L5startS458 = _M0L4selfS459.$1;
  _M0L3endS460 = _M0L4selfS459.$2;
  _M0L5indexS461
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5indexS461)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5indexS461->$0 = _M0L5startS458;
  moonbit_incref(_M0L4selfS459.$0);
  _closure_5679
  = (struct _M0R42StringView_3a_3aiter_2eanon__u2763__l219__*)moonbit_malloc(sizeof(struct _M0R42StringView_3a_3aiter_2eanon__u2763__l219__));
  Moonbit_object_header(_closure_5679)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 146, 0);
  _closure_5679->code = &_M0MPC16string10StringView4iterC2763l219;
  _closure_5679->$0 = _M0L5indexS461;
  _closure_5679->$1 = _M0L3endS460;
  _closure_5679->$2 = _M0L4selfS459;
  _M0L6_2atmpS2762 = (struct _M0TWEOc*)_closure_5679;
  #line 219 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _result_5680 = _M0MPB4Iter3newGcE(_M0L6_2atmpS2762, 4294967296ll);
  moonbit_decref(_M0L6_2atmpS2762);
  return _result_5680;
}

int32_t _M0MPC16string10StringView4iterC2763l219(
  struct _M0TWEOc* _M0L6_2aenvS2764
) {
  struct _M0R42StringView_3a_3aiter_2eanon__u2763__l219__* _M0L14_2acasted__envS2765;
  struct _M0TPC16string10StringView _M0L4selfS459;
  int32_t _M0L3endS460;
  struct _M0TPB8MutLocalGiE* _M0L5indexS461;
  int32_t _M0L3valS2766;
  #line 219 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L14_2acasted__envS2765
  = (struct _M0R42StringView_3a_3aiter_2eanon__u2763__l219__*)_M0L6_2aenvS2764;
  _M0L4selfS459 = _M0L14_2acasted__envS2765->$2;
  _M0L3endS460 = _M0L14_2acasted__envS2765->$1;
  _M0L5indexS461 = _M0L14_2acasted__envS2765->$0;
  _M0L3valS2766 = _M0L5indexS461->$0;
  if (_M0L3valS2766 < _M0L3endS460) {
    moonbit_string_t _M0L3strS2781 = _M0L4selfS459.$0;
    int32_t _M0L3valS2782 = _M0L5indexS461->$0;
    int32_t _M0L2c1S462 = _M0L3strS2781[_M0L3valS2782];
    int32_t _if__result_5681;
    int32_t _M0L3valS2779;
    int32_t _M0L6_2atmpS2778;
    int32_t _M0L6_2atmpS2780;
    #line 222 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S462)) {
      int32_t _M0L3valS2769 = _M0L5indexS461->$0;
      int32_t _M0L6_2atmpS2767 = _M0L3valS2769 + 1;
      int32_t _M0L3endS2768 = _M0L4selfS459.$2;
      _if__result_5681 = _M0L6_2atmpS2767 < _M0L3endS2768;
    } else {
      _if__result_5681 = 0;
    }
    if (_if__result_5681) {
      moonbit_string_t _M0L3strS2775 = _M0L4selfS459.$0;
      int32_t _M0L3valS2777 = _M0L5indexS461->$0;
      int32_t _M0L6_2atmpS2776 = _M0L3valS2777 + 1;
      int32_t _M0L2c2S463 = _M0L3strS2775[_M0L6_2atmpS2776];
      #line 224 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S463)) {
        int32_t _M0L3valS2771 = _M0L5indexS461->$0;
        int32_t _M0L6_2atmpS2770 = _M0L3valS2771 + 2;
        int32_t _M0L6_2atmpS2773;
        int32_t _M0L6_2atmpS2774;
        int32_t _M0L6_2atmpS2772;
        _M0L5indexS461->$0 = _M0L6_2atmpS2770;
        _M0L6_2atmpS2773 = (int32_t)_M0L2c1S462;
        _M0L6_2atmpS2774 = (int32_t)_M0L2c2S463;
        #line 226 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
        _M0L6_2atmpS2772
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2773, _M0L6_2atmpS2774);
        return _M0L6_2atmpS2772;
      }
    }
    _M0L3valS2779 = _M0L5indexS461->$0;
    _M0L6_2atmpS2778 = _M0L3valS2779 + 1;
    _M0L5indexS461->$0 = _M0L6_2atmpS2778;
    #line 230 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
    _M0L6_2atmpS2780 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S462);
    return _M0L6_2atmpS2780;
  } else {
    return -1;
  }
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS457,
  struct _M0TPB6Logger _M0L6loggerS456
) {
  #line 203 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  #line 204 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L6loggerS456.$0->$method_2(_M0L6loggerS456.$1, _M0L4selfS457);
  return 0;
}

moonbit_string_t _M0MPC16string10StringView9to__owned(
  struct _M0TPC16string10StringView _M0L4selfS455
) {
  moonbit_string_t _M0L3strS2759;
  int32_t _M0L5startS2760;
  int32_t _M0L3endS2761;
  moonbit_string_t _result_5682;
  #line 196 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L3strS2759 = _M0L4selfS455.$0;
  _M0L5startS2760 = _M0L4selfS455.$1;
  _M0L3endS2761 = _M0L4selfS455.$2;
  moonbit_incref(_M0L3strS2759);
  #line 199 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _result_5682
  = _M0MPC16string6String17unsafe__substring(_M0L3strS2759, _M0L5startS2760, _M0L3endS2761);
  moonbit_decref(_M0L3strS2759);
  return _result_5682;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS452,
  int32_t _M0L5startS450,
  int32_t _M0L3endS451
) {
  int32_t _if__result_5683;
  int32_t _M0L3lenS453;
  int32_t _M0L6_2atmpS2757;
  int32_t _M0L6_2atmpS2758;
  moonbit_bytes_t _M0L5bytesS454;
  moonbit_bytes_t _M0L6_2atmpS2756;
  moonbit_string_t _result_5684;
  #line 91 "/home/developer/.moon/lib/core/builtin/string.mbt"
  if (_M0L5startS450 == 0) {
    int32_t _M0L6_2atmpS2755 = Moonbit_array_length(_M0L3strS452);
    _if__result_5683 = _M0L3endS451 == _M0L6_2atmpS2755;
  } else {
    _if__result_5683 = 0;
  }
  if (_if__result_5683) {
    moonbit_incref(_M0L3strS452);
    return _M0L3strS452;
  }
  _M0L3lenS453 = _M0L3endS451 - _M0L5startS450;
  _M0L6_2atmpS2757 = _M0L3lenS453 * 2;
  #line 101 "/home/developer/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS2758 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS454
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS2757, _M0L6_2atmpS2758);
  #line 102 "/home/developer/.moon/lib/core/builtin/string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS454, 0, _M0L3strS452, _M0L5startS450, _M0L3lenS453);
  _M0L6_2atmpS2756 = _M0L5bytesS454;
  #line 103 "/home/developer/.moon/lib/core/builtin/string.mbt"
  _result_5684
  = _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS2756, 0, 4294967296ll);
  moonbit_decref(_M0L6_2atmpS2756);
  return _result_5684;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/home/developer/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS445,
  int32_t _M0L6offsetS449,
  int64_t _M0L6lengthS447
) {
  int32_t _M0L3lenS444;
  int32_t _M0L6lengthS446;
  int32_t _if__result_5685;
  #line 77 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
  _M0L3lenS444 = Moonbit_array_length(_M0L4selfS445);
  if (_M0L6lengthS447 == 4294967296ll) {
    _M0L6lengthS446 = _M0L3lenS444 - _M0L6offsetS449;
  } else {
    int64_t _M0L7_2aSomeS448 = _M0L6lengthS447;
    _M0L6lengthS446 = (int32_t)_M0L7_2aSomeS448;
  }
  if (_M0L6offsetS449 >= 0) {
    if (_M0L6lengthS446 >= 0) {
      int32_t _M0L6_2atmpS2754 = _M0L6offsetS449 + _M0L6lengthS446;
      _if__result_5685 = _M0L6_2atmpS2754 <= _M0L3lenS444;
    } else {
      _if__result_5685 = 0;
    }
  } else {
    _if__result_5685 = 0;
  }
  if (_if__result_5685) {
    moonbit_incref(_M0L4selfS445);
    #line 85 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS445, _M0L6offsetS449, _M0L6lengthS446);
  } else {
    #line 84 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS436,
  int32_t _M0L13bytes__offsetS431,
  moonbit_string_t _M0L3strS438,
  int32_t _M0L11str__offsetS434,
  int32_t _M0L6lengthS432
) {
  int32_t _M0L6_2atmpS2753;
  int32_t _M0L6_2atmpS2752;
  int32_t _M0L2e1S430;
  int32_t _M0L6_2atmpS2751;
  int32_t _M0L2e2S433;
  int32_t _M0L4len1S435;
  int32_t _M0L4len2S437;
  int32_t _if__result_5686;
  #line 125 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
  _M0L6_2atmpS2753 = _M0L6lengthS432 * 2;
  _M0L6_2atmpS2752 = _M0L13bytes__offsetS431 + _M0L6_2atmpS2753;
  _M0L2e1S430 = _M0L6_2atmpS2752 - 1;
  _M0L6_2atmpS2751 = _M0L11str__offsetS434 + _M0L6lengthS432;
  _M0L2e2S433 = _M0L6_2atmpS2751 - 1;
  _M0L4len1S435 = Moonbit_array_length(_M0L4selfS436);
  _M0L4len2S437 = Moonbit_array_length(_M0L3strS438);
  if (_M0L6lengthS432 >= 0) {
    if (_M0L13bytes__offsetS431 >= 0) {
      if (_M0L2e1S430 < _M0L4len1S435) {
        if (_M0L11str__offsetS434 >= 0) {
          _if__result_5686 = _M0L2e2S433 < _M0L4len2S437;
        } else {
          _if__result_5686 = 0;
        }
      } else {
        _if__result_5686 = 0;
      }
    } else {
      _if__result_5686 = 0;
    }
  } else {
    _if__result_5686 = 0;
  }
  if (_if__result_5686) {
    int32_t _M0L16end__str__offsetS439 =
      _M0L11str__offsetS434 + _M0L6lengthS432;
    int32_t _M0L1iS440 = _M0L11str__offsetS434;
    int32_t _M0L1jS441 = _M0L13bytes__offsetS431;
    while (1) {
      if (_M0L1iS440 < _M0L16end__str__offsetS439) {
        int32_t _M0L6_2atmpS2748 = _M0L3strS438[_M0L1iS440];
        int32_t _M0L6_2atmpS2747 = (int32_t)_M0L6_2atmpS2748;
        uint32_t _M0L1cS442 = *(uint32_t*)&_M0L6_2atmpS2747;
        uint32_t _M0L6_2atmpS2743 = _M0L1cS442 & 255u;
        int32_t _M0L6_2atmpS2742;
        int32_t _M0L6_2atmpS2744;
        uint32_t _M0L6_2atmpS2746;
        int32_t _M0L6_2atmpS2745;
        int32_t _M0L6_2atmpS2749;
        int32_t _M0L6_2atmpS2750;
        #line 142 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS2742 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2743);
        if (
          _M0L1jS441 < 0 || _M0L1jS441 >= Moonbit_array_length(_M0L4selfS436)
        ) {
          #line 142 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS436[_M0L1jS441] = _M0L6_2atmpS2742;
        _M0L6_2atmpS2744 = _M0L1jS441 + 1;
        _M0L6_2atmpS2746 = _M0L1cS442 >> 8;
        #line 143 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
        _M0L6_2atmpS2745 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2746);
        if (
          _M0L6_2atmpS2744 < 0
          || _M0L6_2atmpS2744 >= Moonbit_array_length(_M0L4selfS436)
        ) {
          #line 143 "/home/developer/.moon/lib/core/builtin/bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS436[_M0L6_2atmpS2744] = _M0L6_2atmpS2745;
        _M0L6_2atmpS2749 = _M0L1iS440 + 1;
        _M0L6_2atmpS2750 = _M0L1jS441 + 2;
        _M0L1iS440 = _M0L6_2atmpS2749;
        _M0L1jS441 = _M0L6_2atmpS2750;
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

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS429) {
  int32_t _M0L6_2atmpS2741;
  #line 2519 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2741 = *(int32_t*)&_M0L4selfS429;
  return _M0L6_2atmpS2741 & 0xff;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS426
) {
  moonbit_string_t* _M0L8_2afieldS5103;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS5103 = _M0L4selfS426->$0;
  moonbit_incref(_M0L8_2afieldS5103);
  return _M0L8_2afieldS5103;
}

moonbit_string_t* _M0MPC15array5Array6bufferGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS427
) {
  moonbit_string_t* _M0L8_2afieldS5104;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS5104 = _M0L4selfS427->$0;
  moonbit_incref(_M0L8_2afieldS5104);
  return _M0L8_2afieldS5104;
}

struct _M0TUsfE** _M0MPC15array5Array6bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS428
) {
  struct _M0TUsfE** _M0L8_2afieldS5105;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS5105 = _M0L4selfS428->$0;
  moonbit_incref(_M0L8_2afieldS5105);
  return _M0L8_2afieldS5105;
}

struct _M0TPC16string10StringView _M0MPC16string10StringView12view_2einner(
  struct _M0TPC16string10StringView _M0L4selfS424,
  int32_t _M0L13start__offsetS425,
  int64_t _M0L11end__offsetS422
) {
  int32_t _M0L11end__offsetS421;
  int32_t _if__result_5688;
  #line 105 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  if (_M0L11end__offsetS422 == 4294967296ll) {
    int32_t _M0L3endS2739 = _M0L4selfS424.$2;
    int32_t _M0L5startS2740 = _M0L4selfS424.$1;
    _M0L11end__offsetS421 = _M0L3endS2739 - _M0L5startS2740;
  } else {
    int64_t _M0L7_2aSomeS423 = _M0L11end__offsetS422;
    _M0L11end__offsetS421 = (int32_t)_M0L7_2aSomeS423;
  }
  if (_M0L13start__offsetS425 >= 0) {
    if (_M0L13start__offsetS425 <= _M0L11end__offsetS421) {
      int32_t _M0L3endS2732 = _M0L4selfS424.$2;
      int32_t _M0L5startS2733 = _M0L4selfS424.$1;
      int32_t _M0L6_2atmpS2731 = _M0L3endS2732 - _M0L5startS2733;
      _if__result_5688 = _M0L11end__offsetS421 <= _M0L6_2atmpS2731;
    } else {
      _if__result_5688 = 0;
    }
  } else {
    _if__result_5688 = 0;
  }
  if (_if__result_5688) {
    moonbit_string_t _M0L3strS2734 = _M0L4selfS424.$0;
    int32_t _M0L5startS2738 = _M0L4selfS424.$1;
    int32_t _M0L6_2atmpS2735 = _M0L5startS2738 + _M0L13start__offsetS425;
    int32_t _M0L5startS2737 = _M0L4selfS424.$1;
    int32_t _M0L6_2atmpS2736 = _M0L5startS2737 + _M0L11end__offsetS421;
    moonbit_incref(_M0L3strS2734);
    return (struct _M0TPC16string10StringView){.$0 = _M0L3strS2734,
                                                 .$1 = _M0L6_2atmpS2735,
                                                 .$2 = _M0L6_2atmpS2736};
  } else {
    #line 114 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
    return _M0FPC15abort5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_145.data);
  }
}

struct _M0TPB4IterGUssEE* _M0MPB4Iter3newGUssEE(
  struct _M0TWEOUssE* _M0L1fS390,
  int64_t _M0L10size__hintS387
) {
  int64_t _M0L10size__hintS386;
  struct _M0TPB4IterGUssEE* _block_5689;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS387 == 4294967296ll) {
    _M0L10size__hintS386 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS388 = _M0L10size__hintS387;
    int32_t _M0L4_2anS389 = (int32_t)_M0L7_2aSomeS388;
    if (_M0L4_2anS389 > 0) {
      _M0L10size__hintS386 = (int64_t)_M0L4_2anS389;
    } else {
      _M0L10size__hintS386 = _M0MPB4Iter3newN6constrS9988GUssEE;
    }
  }
  moonbit_incref(_M0L1fS390);
  _block_5689
  = (struct _M0TPB4IterGUssEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUssEE));
  Moonbit_object_header(_block_5689)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 150, 0);
  _block_5689->$0 = _M0L1fS390;
  _block_5689->$1 = _M0L10size__hintS386;
  return _block_5689;
}

struct _M0TPB4IterGUsbEE* _M0MPB4Iter3newGUsbEE(
  struct _M0TWEOUsbE* _M0L1fS395,
  int64_t _M0L10size__hintS392
) {
  int64_t _M0L10size__hintS391;
  struct _M0TPB4IterGUsbEE* _block_5690;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS392 == 4294967296ll) {
    _M0L10size__hintS391 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS393 = _M0L10size__hintS392;
    int32_t _M0L4_2anS394 = (int32_t)_M0L7_2aSomeS393;
    if (_M0L4_2anS394 > 0) {
      _M0L10size__hintS391 = (int64_t)_M0L4_2anS394;
    } else {
      _M0L10size__hintS391 = _M0MPB4Iter3newN6constrS9988GUsbEE;
    }
  }
  moonbit_incref(_M0L1fS395);
  _block_5690
  = (struct _M0TPB4IterGUsbEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsbEE));
  Moonbit_object_header(_block_5690)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 153, 0);
  _block_5690->$0 = _M0L1fS395;
  _block_5690->$1 = _M0L10size__hintS391;
  return _block_5690;
}

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB4Iter3newGUsRP19moonbitDB10RedisValueEE(
  struct _M0TWEOUsRP19moonbitDB10RedisValueE* _M0L1fS400,
  int64_t _M0L10size__hintS397
) {
  int64_t _M0L10size__hintS396;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _block_5691;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS397 == 4294967296ll) {
    _M0L10size__hintS396 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS398 = _M0L10size__hintS397;
    int32_t _M0L4_2anS399 = (int32_t)_M0L7_2aSomeS398;
    if (_M0L4_2anS399 > 0) {
      _M0L10size__hintS396 = (int64_t)_M0L4_2anS399;
    } else {
      _M0L10size__hintS396
      = _M0MPB4Iter3newN6constrS9988GUsRP19moonbitDB10RedisValueEE;
    }
  }
  moonbit_incref(_M0L1fS400);
  _block_5691
  = (struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE));
  Moonbit_object_header(_block_5691)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 156, 0);
  _block_5691->$0 = _M0L1fS400;
  _block_5691->$1 = _M0L10size__hintS396;
  return _block_5691;
}

struct _M0TPB4IterGRPC16string10StringViewE* _M0MPB4Iter3newGRPC16string10StringViewE(
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L1fS405,
  int64_t _M0L10size__hintS402
) {
  int64_t _M0L10size__hintS401;
  struct _M0TPB4IterGRPC16string10StringViewE* _block_5692;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS402 == 4294967296ll) {
    _M0L10size__hintS401 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS403 = _M0L10size__hintS402;
    int32_t _M0L4_2anS404 = (int32_t)_M0L7_2aSomeS403;
    if (_M0L4_2anS404 > 0) {
      _M0L10size__hintS401 = (int64_t)_M0L4_2anS404;
    } else {
      _M0L10size__hintS401
      = _M0MPB4Iter3newN6constrS9988GRPC16string10StringViewE;
    }
  }
  moonbit_incref(_M0L1fS405);
  _block_5692
  = (struct _M0TPB4IterGRPC16string10StringViewE*)moonbit_malloc(sizeof(struct _M0TPB4IterGRPC16string10StringViewE));
  Moonbit_object_header(_block_5692)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 143, 0);
  _block_5692->$0 = _M0L1fS405;
  _block_5692->$1 = _M0L10size__hintS401;
  return _block_5692;
}

struct _M0TPB4IterGUsfEE* _M0MPB4Iter3newGUsfEE(
  struct _M0TWEOUsfE* _M0L1fS410,
  int64_t _M0L10size__hintS407
) {
  int64_t _M0L10size__hintS406;
  struct _M0TPB4IterGUsfEE* _block_5693;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS407 == 4294967296ll) {
    _M0L10size__hintS406 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS408 = _M0L10size__hintS407;
    int32_t _M0L4_2anS409 = (int32_t)_M0L7_2aSomeS408;
    if (_M0L4_2anS409 > 0) {
      _M0L10size__hintS406 = (int64_t)_M0L4_2anS409;
    } else {
      _M0L10size__hintS406 = _M0MPB4Iter3newN6constrS9988GUsfEE;
    }
  }
  moonbit_incref(_M0L1fS410);
  _block_5693
  = (struct _M0TPB4IterGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsfEE));
  Moonbit_object_header(_block_5693)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 159, 0);
  _block_5693->$0 = _M0L1fS410;
  _block_5693->$1 = _M0L10size__hintS406;
  return _block_5693;
}

struct _M0TPB4IterGUsiEE* _M0MPB4Iter3newGUsiEE(
  struct _M0TWEOUsiE* _M0L1fS415,
  int64_t _M0L10size__hintS412
) {
  int64_t _M0L10size__hintS411;
  struct _M0TPB4IterGUsiEE* _block_5694;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS412 == 4294967296ll) {
    _M0L10size__hintS411 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS413 = _M0L10size__hintS412;
    int32_t _M0L4_2anS414 = (int32_t)_M0L7_2aSomeS413;
    if (_M0L4_2anS414 > 0) {
      _M0L10size__hintS411 = (int64_t)_M0L4_2anS414;
    } else {
      _M0L10size__hintS411 = _M0MPB4Iter3newN6constrS9988GUsiEE;
    }
  }
  moonbit_incref(_M0L1fS415);
  _block_5694
  = (struct _M0TPB4IterGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsiEE));
  Moonbit_object_header(_block_5694)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 162, 0);
  _block_5694->$0 = _M0L1fS415;
  _block_5694->$1 = _M0L10size__hintS411;
  return _block_5694;
}

struct _M0TPB4IterGcE* _M0MPB4Iter3newGcE(
  struct _M0TWEOc* _M0L1fS420,
  int64_t _M0L10size__hintS417
) {
  int64_t _M0L10size__hintS416;
  struct _M0TPB4IterGcE* _block_5695;
  #line 270 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  if (_M0L10size__hintS417 == 4294967296ll) {
    _M0L10size__hintS416 = 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS418 = _M0L10size__hintS417;
    int32_t _M0L4_2anS419 = (int32_t)_M0L7_2aSomeS418;
    if (_M0L4_2anS419 > 0) {
      _M0L10size__hintS416 = (int64_t)_M0L4_2anS419;
    } else {
      _M0L10size__hintS416 = _M0MPB4Iter3newN6constrS9988GcE;
    }
  }
  moonbit_incref(_M0L1fS420);
  _block_5695
  = (struct _M0TPB4IterGcE*)moonbit_malloc(sizeof(struct _M0TPB4IterGcE));
  Moonbit_object_header(_block_5695)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 165, 0);
  _block_5695->$0 = _M0L1fS420;
  _block_5695->$1 = _M0L10size__hintS416;
  return _block_5695;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS378,
  int32_t _M0L5radixS377
) {
  int32_t _if__result_5696;
  uint16_t* _M0L6bufferS379;
  #line 607 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS377 < 2) {
    _if__result_5696 = 1;
  } else {
    _if__result_5696 = _M0L5radixS377 > 36;
  }
  if (_if__result_5696) {
    #line 611 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_146.data);
  }
  if (_M0L4selfS378 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_122.data;
  }
  switch (_M0L5radixS377) {
    case 10: {
      int32_t _M0L3lenS380;
      uint16_t* _M0L6bufferS381;
      #line 622 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L3lenS380 = _M0FPB12dec__count64(_M0L4selfS378);
      _M0L6bufferS381 = (uint16_t*)moonbit_make_string(_M0L3lenS380, 0);
      #line 624 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS381, _M0L4selfS378, 0, _M0L3lenS380);
      _M0L6bufferS379 = _M0L6bufferS381;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS382;
      uint16_t* _M0L6bufferS383;
      #line 628 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L3lenS382 = _M0FPB12hex__count64(_M0L4selfS378);
      _M0L6bufferS383 = (uint16_t*)moonbit_make_string(_M0L3lenS382, 0);
      #line 630 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS383, _M0L4selfS378, 0, _M0L3lenS382);
      _M0L6bufferS379 = _M0L6bufferS383;
      break;
    }
    default: {
      int32_t _M0L3lenS384;
      uint16_t* _M0L6bufferS385;
      #line 634 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L3lenS384 = _M0FPB14radix__count64(_M0L4selfS378, _M0L5radixS377);
      _M0L6bufferS385 = (uint16_t*)moonbit_make_string(_M0L3lenS384, 0);
      #line 636 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS385, _M0L4selfS378, 0, _M0L3lenS384, _M0L5radixS377);
      _M0L6bufferS379 = _M0L6bufferS385;
      break;
    }
  }
  return _M0L6bufferS379;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS363,
  uint64_t _M0L3numS375,
  int32_t _M0L12digit__startS364,
  int32_t _M0L10total__lenS376
) {
  int32_t _M0L6_2atmpS2730;
  uint64_t _M0L3numS353;
  int32_t _M0L6offsetS354;
  #line 493 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2730 = _M0L10total__lenS376 - _M0L12digit__startS364;
  _M0L3numS353 = _M0L3numS375;
  _M0L6offsetS354 = _M0L6_2atmpS2730;
  while (1) {
    if (_M0L3numS353 >= 10000ull) {
      uint64_t _M0L1tS355 = _M0L3numS353 / 10000ull;
      uint64_t _M0L6_2atmpS2707 = _M0L3numS353 % 10000ull;
      int32_t _M0L1rS356 = (int32_t)_M0L6_2atmpS2707;
      int32_t _M0L2d1S357 = _M0L1rS356 / 100;
      int32_t _M0L2d2S358 = _M0L1rS356 % 100;
      int32_t _M0L6_2atmpS2706 = _M0L2d1S357 / 10;
      int32_t _M0L6_2atmpS2705 = 48 + _M0L6_2atmpS2706;
      int32_t _M0L6d1__hiS359 = (uint16_t)_M0L6_2atmpS2705;
      int32_t _M0L6_2atmpS2704 = _M0L2d1S357 % 10;
      int32_t _M0L6_2atmpS2703 = 48 + _M0L6_2atmpS2704;
      int32_t _M0L6d1__loS360 = (uint16_t)_M0L6_2atmpS2703;
      int32_t _M0L6_2atmpS2702 = _M0L2d2S358 / 10;
      int32_t _M0L6_2atmpS2701 = 48 + _M0L6_2atmpS2702;
      int32_t _M0L6d2__hiS361 = (uint16_t)_M0L6_2atmpS2701;
      int32_t _M0L6_2atmpS2700 = _M0L2d2S358 % 10;
      int32_t _M0L6_2atmpS2699 = 48 + _M0L6_2atmpS2700;
      int32_t _M0L6d2__loS362 = (uint16_t)_M0L6_2atmpS2699;
      int32_t _M0L6_2atmpS2691 = _M0L12digit__startS364 + _M0L6offsetS354;
      int32_t _M0L6_2atmpS2690 = _M0L6_2atmpS2691 - 4;
      int32_t _M0L6_2atmpS2693;
      int32_t _M0L6_2atmpS2692;
      int32_t _M0L6_2atmpS2695;
      int32_t _M0L6_2atmpS2694;
      int32_t _M0L6_2atmpS2697;
      int32_t _M0L6_2atmpS2696;
      int32_t _M0L6_2atmpS2698;
      _M0L6bufferS363[_M0L6_2atmpS2690] = _M0L6d1__hiS359;
      _M0L6_2atmpS2693 = _M0L12digit__startS364 + _M0L6offsetS354;
      _M0L6_2atmpS2692 = _M0L6_2atmpS2693 - 3;
      _M0L6bufferS363[_M0L6_2atmpS2692] = _M0L6d1__loS360;
      _M0L6_2atmpS2695 = _M0L12digit__startS364 + _M0L6offsetS354;
      _M0L6_2atmpS2694 = _M0L6_2atmpS2695 - 2;
      _M0L6bufferS363[_M0L6_2atmpS2694] = _M0L6d2__hiS361;
      _M0L6_2atmpS2697 = _M0L12digit__startS364 + _M0L6offsetS354;
      _M0L6_2atmpS2696 = _M0L6_2atmpS2697 - 1;
      _M0L6bufferS363[_M0L6_2atmpS2696] = _M0L6d2__loS362;
      _M0L6_2atmpS2698 = _M0L6offsetS354 - 4;
      _M0L3numS353 = _M0L1tS355;
      _M0L6offsetS354 = _M0L6_2atmpS2698;
      continue;
    } else {
      int32_t _M0L6_2atmpS2729 = (int32_t)_M0L3numS353;
      int32_t _M0L9remainingS366 = _M0L6_2atmpS2729;
      int32_t _M0L6offsetS367 = _M0L6offsetS354;
      while (1) {
        if (_M0L9remainingS366 >= 100) {
          int32_t _M0L1tS368 = _M0L9remainingS366 / 100;
          int32_t _M0L1dS369 = _M0L9remainingS366 % 100;
          int32_t _M0L6_2atmpS2716 = _M0L1dS369 / 10;
          int32_t _M0L6_2atmpS2715 = 48 + _M0L6_2atmpS2716;
          int32_t _M0L5d__hiS370 = (uint16_t)_M0L6_2atmpS2715;
          int32_t _M0L6_2atmpS2714 = _M0L1dS369 % 10;
          int32_t _M0L6_2atmpS2713 = 48 + _M0L6_2atmpS2714;
          int32_t _M0L5d__loS371 = (uint16_t)_M0L6_2atmpS2713;
          int32_t _M0L6_2atmpS2709 = _M0L12digit__startS364 + _M0L6offsetS367;
          int32_t _M0L6_2atmpS2708 = _M0L6_2atmpS2709 - 2;
          int32_t _M0L6_2atmpS2711;
          int32_t _M0L6_2atmpS2710;
          int32_t _M0L6_2atmpS2712;
          _M0L6bufferS363[_M0L6_2atmpS2708] = _M0L5d__hiS370;
          _M0L6_2atmpS2711 = _M0L12digit__startS364 + _M0L6offsetS367;
          _M0L6_2atmpS2710 = _M0L6_2atmpS2711 - 1;
          _M0L6bufferS363[_M0L6_2atmpS2710] = _M0L5d__loS371;
          _M0L6_2atmpS2712 = _M0L6offsetS367 - 2;
          _M0L9remainingS366 = _M0L1tS368;
          _M0L6offsetS367 = _M0L6_2atmpS2712;
          continue;
        } else if (_M0L9remainingS366 >= 10) {
          int32_t _M0L6_2atmpS2724 = _M0L9remainingS366 / 10;
          int32_t _M0L6_2atmpS2723 = 48 + _M0L6_2atmpS2724;
          int32_t _M0L5d__hiS373 = (uint16_t)_M0L6_2atmpS2723;
          int32_t _M0L6_2atmpS2722 = _M0L9remainingS366 % 10;
          int32_t _M0L6_2atmpS2721 = 48 + _M0L6_2atmpS2722;
          int32_t _M0L5d__loS374 = (uint16_t)_M0L6_2atmpS2721;
          int32_t _M0L6_2atmpS2718 = _M0L12digit__startS364 + _M0L6offsetS367;
          int32_t _M0L6_2atmpS2717 = _M0L6_2atmpS2718 - 2;
          int32_t _M0L6_2atmpS2720;
          int32_t _M0L6_2atmpS2719;
          _M0L6bufferS363[_M0L6_2atmpS2717] = _M0L5d__hiS373;
          _M0L6_2atmpS2720 = _M0L12digit__startS364 + _M0L6offsetS367;
          _M0L6_2atmpS2719 = _M0L6_2atmpS2720 - 1;
          _M0L6bufferS363[_M0L6_2atmpS2719] = _M0L5d__loS374;
        } else {
          int32_t _M0L6_2atmpS2728 = _M0L12digit__startS364 + _M0L6offsetS367;
          int32_t _M0L6_2atmpS2725 = _M0L6_2atmpS2728 - 1;
          int32_t _M0L6_2atmpS2727 = 48 + _M0L9remainingS366;
          int32_t _M0L6_2atmpS2726 = (uint16_t)_M0L6_2atmpS2727;
          _M0L6bufferS363[_M0L6_2atmpS2725] = _M0L6_2atmpS2726;
        }
        break;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS343,
  uint64_t _M0L3numS347,
  int32_t _M0L12digit__startS344,
  int32_t _M0L10total__lenS346,
  int32_t _M0L5radixS337
) {
  uint64_t _M0L4baseS336;
  int32_t _M0L6_2atmpS2675;
  int32_t _M0L6_2atmpS2674;
  #line 462 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  #line 470 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS336 = _M0MPC13int3Int10to__uint64(_M0L5radixS337);
  _M0L6_2atmpS2675 = _M0L5radixS337 - 1;
  _M0L6_2atmpS2674 = _M0L5radixS337 & _M0L6_2atmpS2675;
  if (_M0L6_2atmpS2674 == 0) {
    int32_t _M0L5shiftS338;
    uint64_t _M0L4maskS339;
    int32_t _M0L6_2atmpS2682;
    int32_t _M0L6offsetS340;
    uint64_t _M0L1nS341;
    #line 473 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS338 = moonbit_ctz32(_M0L5radixS337);
    _M0L4maskS339 = _M0L4baseS336 - 1ull;
    _M0L6_2atmpS2682 = _M0L10total__lenS346 - _M0L12digit__startS344;
    _M0L6offsetS340 = _M0L6_2atmpS2682;
    _M0L1nS341 = _M0L3numS347;
    while (1) {
      if (_M0L1nS341 > 0ull) {
        uint64_t _M0L6_2atmpS2681 = _M0L1nS341 & _M0L4maskS339;
        int32_t _M0L5digitS342 = (int32_t)_M0L6_2atmpS2681;
        int32_t _M0L6_2atmpS2678 = _M0L12digit__startS344 + _M0L6offsetS340;
        int32_t _M0L6_2atmpS2676 = _M0L6_2atmpS2678 - 1;
        int32_t _M0L6_2atmpS2677 =
          ((moonbit_string_t)moonbit_string_literal_147.data)[_M0L5digitS342];
        int32_t _M0L6_2atmpS2679;
        uint64_t _M0L6_2atmpS2680;
        _M0L6bufferS343[_M0L6_2atmpS2676] = _M0L6_2atmpS2677;
        _M0L6_2atmpS2679 = _M0L6offsetS340 - 1;
        _M0L6_2atmpS2680 = _M0L1nS341 >> (_M0L5shiftS338 & 63);
        _M0L6offsetS340 = _M0L6_2atmpS2679;
        _M0L1nS341 = _M0L6_2atmpS2680;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2689 = _M0L10total__lenS346 - _M0L12digit__startS344;
    int32_t _M0L6offsetS348 = _M0L6_2atmpS2689;
    uint64_t _M0L1nS349 = _M0L3numS347;
    while (1) {
      if (_M0L1nS349 > 0ull) {
        uint64_t _M0L1qS350 = _M0L1nS349 / _M0L4baseS336;
        uint64_t _M0L6_2atmpS2688 = _M0L1qS350 * _M0L4baseS336;
        uint64_t _M0L6_2atmpS2687 = _M0L1nS349 - _M0L6_2atmpS2688;
        int32_t _M0L5digitS351 = (int32_t)_M0L6_2atmpS2687;
        int32_t _M0L6_2atmpS2685 = _M0L12digit__startS344 + _M0L6offsetS348;
        int32_t _M0L6_2atmpS2683 = _M0L6_2atmpS2685 - 1;
        int32_t _M0L6_2atmpS2684 =
          ((moonbit_string_t)moonbit_string_literal_147.data)[_M0L5digitS351];
        int32_t _M0L6_2atmpS2686;
        _M0L6bufferS343[_M0L6_2atmpS2683] = _M0L6_2atmpS2684;
        _M0L6_2atmpS2686 = _M0L6offsetS348 - 1;
        _M0L6offsetS348 = _M0L6_2atmpS2686;
        _M0L1nS349 = _M0L1qS350;
        continue;
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS330,
  uint64_t _M0L3numS335,
  int32_t _M0L12digit__startS331,
  int32_t _M0L10total__lenS334
) {
  int32_t _M0L6_2atmpS2673;
  int32_t _M0L6offsetS325;
  uint64_t _M0L1nS326;
  #line 434 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2673 = _M0L10total__lenS334 - _M0L12digit__startS331;
  _M0L6offsetS325 = _M0L6_2atmpS2673;
  _M0L1nS326 = _M0L3numS335;
  while (1) {
    if (_M0L6offsetS325 >= 2) {
      uint64_t _M0L6_2atmpS2670 = _M0L1nS326 & 255ull;
      int32_t _M0L9byte__valS327 = (int32_t)_M0L6_2atmpS2670;
      int32_t _M0L2hiS328 = _M0L9byte__valS327 / 16;
      int32_t _M0L2loS329 = _M0L9byte__valS327 % 16;
      int32_t _M0L6_2atmpS2664 = _M0L12digit__startS331 + _M0L6offsetS325;
      int32_t _M0L6_2atmpS2662 = _M0L6_2atmpS2664 - 2;
      int32_t _M0L6_2atmpS2663 =
        ((moonbit_string_t)moonbit_string_literal_147.data)[_M0L2hiS328];
      int32_t _M0L6_2atmpS2667;
      int32_t _M0L6_2atmpS2665;
      int32_t _M0L6_2atmpS2666;
      int32_t _M0L6_2atmpS2668;
      uint64_t _M0L6_2atmpS2669;
      _M0L6bufferS330[_M0L6_2atmpS2662] = _M0L6_2atmpS2663;
      _M0L6_2atmpS2667 = _M0L12digit__startS331 + _M0L6offsetS325;
      _M0L6_2atmpS2665 = _M0L6_2atmpS2667 - 1;
      _M0L6_2atmpS2666
      = ((moonbit_string_t)moonbit_string_literal_147.data)[
        _M0L2loS329
      ];
      _M0L6bufferS330[_M0L6_2atmpS2665] = _M0L6_2atmpS2666;
      _M0L6_2atmpS2668 = _M0L6offsetS325 - 2;
      _M0L6_2atmpS2669 = _M0L1nS326 >> 8;
      _M0L6offsetS325 = _M0L6_2atmpS2668;
      _M0L1nS326 = _M0L6_2atmpS2669;
      continue;
    } else if (_M0L6offsetS325 == 1) {
      uint64_t _M0L6_2atmpS2672 = _M0L1nS326 & 15ull;
      int32_t _M0L6nibbleS333 = (int32_t)_M0L6_2atmpS2672;
      int32_t _M0L6_2atmpS2671 =
        ((moonbit_string_t)moonbit_string_literal_147.data)[_M0L6nibbleS333];
      _M0L6bufferS330[_M0L12digit__startS331] = _M0L6_2atmpS2671;
    }
    break;
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS319,
  int32_t _M0L5radixS321
) {
  uint64_t _M0L4baseS320;
  uint64_t _M0L3numS322;
  int32_t _M0L5countS323;
  #line 419 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS319 == 0ull) {
    return 1;
  }
  #line 424 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS320 = _M0MPC13int3Int10to__uint64(_M0L5radixS321);
  _M0L3numS322 = _M0L5valueS319;
  _M0L5countS323 = 0;
  while (1) {
    if (_M0L3numS322 > 0ull) {
      uint64_t _M0L6_2atmpS2660 = _M0L3numS322 / _M0L4baseS320;
      int32_t _M0L6_2atmpS2661 = _M0L5countS323 + 1;
      _M0L3numS322 = _M0L6_2atmpS2660;
      _M0L5countS323 = _M0L6_2atmpS2661;
      continue;
    } else {
      return _M0L5countS323;
    }
    break;
  }
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS317) {
  #line 407 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS317 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS318;
    int32_t _M0L6_2atmpS2659;
    int32_t _M0L6_2atmpS2658;
    #line 412 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS318 = moonbit_clz64(_M0L5valueS317);
    _M0L6_2atmpS2659 = 63 - _M0L14leading__zerosS318;
    _M0L6_2atmpS2658 = _M0L6_2atmpS2659 / 4;
    return _M0L6_2atmpS2658 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS316) {
  #line 343 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS316 >= 10000000000ull) {
    if (_M0L5valueS316 >= 100000000000000ull) {
      if (_M0L5valueS316 >= 10000000000000000ull) {
        if (_M0L5valueS316 >= 1000000000000000000ull) {
          if (_M0L5valueS316 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS316 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS316 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS316 >= 1000000000000ull) {
      if (_M0L5valueS316 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS316 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS316 >= 100000ull) {
    if (_M0L5valueS316 >= 10000000ull) {
      if (_M0L5valueS316 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS316 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS316 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS316 >= 1000ull) {
    if (_M0L5valueS316 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS316 >= 100ull) {
    return 3;
  } else if (_M0L5valueS316 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS300,
  int32_t _M0L5radixS299
) {
  int32_t _if__result_5703;
  int32_t _M0L12is__negativeS301;
  uint32_t _M0L3numS302;
  uint16_t* _M0L6bufferS303;
  #line 209 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS299 < 2) {
    _if__result_5703 = 1;
  } else {
    _if__result_5703 = _M0L5radixS299 > 36;
  }
  if (_if__result_5703) {
    #line 213 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_146.data);
  }
  if (_M0L4selfS300 == 0) {
    return (moonbit_string_t)moonbit_string_literal_122.data;
  }
  _M0L12is__negativeS301 = _M0L4selfS300 < 0;
  if (_M0L12is__negativeS301) {
    int32_t _M0L6_2atmpS2657 = -_M0L4selfS300;
    _M0L3numS302 = *(uint32_t*)&_M0L6_2atmpS2657;
  } else {
    _M0L3numS302 = *(uint32_t*)&_M0L4selfS300;
  }
  switch (_M0L5radixS299) {
    case 10: {
      int32_t _M0L10digit__lenS304;
      int32_t _M0L6_2atmpS2654;
      int32_t _M0L10total__lenS305;
      uint16_t* _M0L6bufferS306;
      int32_t _M0L12digit__startS307;
      #line 235 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS304 = _M0FPB12dec__count32(_M0L3numS302);
      if (_M0L12is__negativeS301) {
        _M0L6_2atmpS2654 = 1;
      } else {
        _M0L6_2atmpS2654 = 0;
      }
      _M0L10total__lenS305 = _M0L10digit__lenS304 + _M0L6_2atmpS2654;
      _M0L6bufferS306
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS305, 0);
      if (_M0L12is__negativeS301) {
        _M0L12digit__startS307 = 1;
      } else {
        _M0L12digit__startS307 = 0;
      }
      #line 239 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS306, _M0L3numS302, _M0L12digit__startS307, _M0L10total__lenS305);
      _M0L6bufferS303 = _M0L6bufferS306;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS308;
      int32_t _M0L6_2atmpS2655;
      int32_t _M0L10total__lenS309;
      uint16_t* _M0L6bufferS310;
      int32_t _M0L12digit__startS311;
      #line 243 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS308 = _M0FPB12hex__count32(_M0L3numS302);
      if (_M0L12is__negativeS301) {
        _M0L6_2atmpS2655 = 1;
      } else {
        _M0L6_2atmpS2655 = 0;
      }
      _M0L10total__lenS309 = _M0L10digit__lenS308 + _M0L6_2atmpS2655;
      _M0L6bufferS310
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS309, 0);
      if (_M0L12is__negativeS301) {
        _M0L12digit__startS311 = 1;
      } else {
        _M0L12digit__startS311 = 0;
      }
      #line 247 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS310, _M0L3numS302, _M0L12digit__startS311, _M0L10total__lenS309);
      _M0L6bufferS303 = _M0L6bufferS310;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS312;
      int32_t _M0L6_2atmpS2656;
      int32_t _M0L10total__lenS313;
      uint16_t* _M0L6bufferS314;
      int32_t _M0L12digit__startS315;
      #line 251 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS312
      = _M0FPB14radix__count32(_M0L3numS302, _M0L5radixS299);
      if (_M0L12is__negativeS301) {
        _M0L6_2atmpS2656 = 1;
      } else {
        _M0L6_2atmpS2656 = 0;
      }
      _M0L10total__lenS313 = _M0L10digit__lenS312 + _M0L6_2atmpS2656;
      _M0L6bufferS314
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS313, 0);
      if (_M0L12is__negativeS301) {
        _M0L12digit__startS315 = 1;
      } else {
        _M0L12digit__startS315 = 0;
      }
      #line 255 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS314, _M0L3numS302, _M0L12digit__startS315, _M0L10total__lenS313, _M0L5radixS299);
      _M0L6bufferS303 = _M0L6bufferS314;
      break;
    }
  }
  if (_M0L12is__negativeS301) {
    _M0L6bufferS303[0] = 45;
  }
  return _M0L6bufferS303;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS293,
  int32_t _M0L5radixS295
) {
  uint32_t _M0L4baseS294;
  uint32_t _M0L3numS296;
  int32_t _M0L5countS297;
  #line 189 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS293 == 0u) {
    return 1;
  }
  _M0L4baseS294 = *(uint32_t*)&_M0L5radixS295;
  _M0L3numS296 = _M0L5valueS293;
  _M0L5countS297 = 0;
  while (1) {
    if (_M0L3numS296 > 0u) {
      uint32_t _M0L6_2atmpS2652 = _M0L3numS296 / _M0L4baseS294;
      int32_t _M0L6_2atmpS2653 = _M0L5countS297 + 1;
      _M0L3numS296 = _M0L6_2atmpS2652;
      _M0L5countS297 = _M0L6_2atmpS2653;
      continue;
    } else {
      return _M0L5countS297;
    }
    break;
  }
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS291) {
  #line 177 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS291 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS292;
    int32_t _M0L6_2atmpS2651;
    int32_t _M0L6_2atmpS2650;
    #line 182 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS292 = moonbit_clz32(_M0L5valueS291);
    _M0L6_2atmpS2651 = 31 - _M0L14leading__zerosS292;
    _M0L6_2atmpS2650 = _M0L6_2atmpS2651 / 4;
    return _M0L6_2atmpS2650 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS290) {
  #line 143 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5valueS290 >= 100000u) {
    if (_M0L5valueS290 >= 10000000u) {
      if (_M0L5valueS290 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS290 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS290 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS290 >= 1000u) {
    if (_M0L5valueS290 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS290 >= 100u) {
    return 3;
  } else if (_M0L5valueS290 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS276,
  uint32_t _M0L3numS288,
  int32_t _M0L12digit__startS277,
  int32_t _M0L10total__lenS289
) {
  int32_t _M0L6_2atmpS2649;
  uint32_t _M0L3numS266;
  int32_t _M0L6offsetS267;
  #line 88 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2649 = _M0L10total__lenS289 - _M0L12digit__startS277;
  _M0L3numS266 = _M0L3numS288;
  _M0L6offsetS267 = _M0L6_2atmpS2649;
  while (1) {
    if (_M0L3numS266 >= 10000u) {
      uint32_t _M0L1tS268 = _M0L3numS266 / 10000u;
      uint32_t _M0L6_2atmpS2626 = _M0L3numS266 % 10000u;
      int32_t _M0L1rS269 = *(int32_t*)&_M0L6_2atmpS2626;
      int32_t _M0L2d1S270 = _M0L1rS269 / 100;
      int32_t _M0L2d2S271 = _M0L1rS269 % 100;
      int32_t _M0L6_2atmpS2625 = _M0L2d1S270 / 10;
      int32_t _M0L6_2atmpS2624 = 48 + _M0L6_2atmpS2625;
      int32_t _M0L6d1__hiS272 = (uint16_t)_M0L6_2atmpS2624;
      int32_t _M0L6_2atmpS2623 = _M0L2d1S270 % 10;
      int32_t _M0L6_2atmpS2622 = 48 + _M0L6_2atmpS2623;
      int32_t _M0L6d1__loS273 = (uint16_t)_M0L6_2atmpS2622;
      int32_t _M0L6_2atmpS2621 = _M0L2d2S271 / 10;
      int32_t _M0L6_2atmpS2620 = 48 + _M0L6_2atmpS2621;
      int32_t _M0L6d2__hiS274 = (uint16_t)_M0L6_2atmpS2620;
      int32_t _M0L6_2atmpS2619 = _M0L2d2S271 % 10;
      int32_t _M0L6_2atmpS2618 = 48 + _M0L6_2atmpS2619;
      int32_t _M0L6d2__loS275 = (uint16_t)_M0L6_2atmpS2618;
      int32_t _M0L6_2atmpS2610 = _M0L12digit__startS277 + _M0L6offsetS267;
      int32_t _M0L6_2atmpS2609 = _M0L6_2atmpS2610 - 4;
      int32_t _M0L6_2atmpS2612;
      int32_t _M0L6_2atmpS2611;
      int32_t _M0L6_2atmpS2614;
      int32_t _M0L6_2atmpS2613;
      int32_t _M0L6_2atmpS2616;
      int32_t _M0L6_2atmpS2615;
      int32_t _M0L6_2atmpS2617;
      _M0L6bufferS276[_M0L6_2atmpS2609] = _M0L6d1__hiS272;
      _M0L6_2atmpS2612 = _M0L12digit__startS277 + _M0L6offsetS267;
      _M0L6_2atmpS2611 = _M0L6_2atmpS2612 - 3;
      _M0L6bufferS276[_M0L6_2atmpS2611] = _M0L6d1__loS273;
      _M0L6_2atmpS2614 = _M0L12digit__startS277 + _M0L6offsetS267;
      _M0L6_2atmpS2613 = _M0L6_2atmpS2614 - 2;
      _M0L6bufferS276[_M0L6_2atmpS2613] = _M0L6d2__hiS274;
      _M0L6_2atmpS2616 = _M0L12digit__startS277 + _M0L6offsetS267;
      _M0L6_2atmpS2615 = _M0L6_2atmpS2616 - 1;
      _M0L6bufferS276[_M0L6_2atmpS2615] = _M0L6d2__loS275;
      _M0L6_2atmpS2617 = _M0L6offsetS267 - 4;
      _M0L3numS266 = _M0L1tS268;
      _M0L6offsetS267 = _M0L6_2atmpS2617;
      continue;
    } else {
      int32_t _M0L6_2atmpS2648 = *(int32_t*)&_M0L3numS266;
      int32_t _M0L9remainingS279 = _M0L6_2atmpS2648;
      int32_t _M0L6offsetS280 = _M0L6offsetS267;
      while (1) {
        if (_M0L9remainingS279 >= 100) {
          int32_t _M0L1tS281 = _M0L9remainingS279 / 100;
          int32_t _M0L1dS282 = _M0L9remainingS279 % 100;
          int32_t _M0L6_2atmpS2635 = _M0L1dS282 / 10;
          int32_t _M0L6_2atmpS2634 = 48 + _M0L6_2atmpS2635;
          int32_t _M0L5d__hiS283 = (uint16_t)_M0L6_2atmpS2634;
          int32_t _M0L6_2atmpS2633 = _M0L1dS282 % 10;
          int32_t _M0L6_2atmpS2632 = 48 + _M0L6_2atmpS2633;
          int32_t _M0L5d__loS284 = (uint16_t)_M0L6_2atmpS2632;
          int32_t _M0L6_2atmpS2628 = _M0L12digit__startS277 + _M0L6offsetS280;
          int32_t _M0L6_2atmpS2627 = _M0L6_2atmpS2628 - 2;
          int32_t _M0L6_2atmpS2630;
          int32_t _M0L6_2atmpS2629;
          int32_t _M0L6_2atmpS2631;
          _M0L6bufferS276[_M0L6_2atmpS2627] = _M0L5d__hiS283;
          _M0L6_2atmpS2630 = _M0L12digit__startS277 + _M0L6offsetS280;
          _M0L6_2atmpS2629 = _M0L6_2atmpS2630 - 1;
          _M0L6bufferS276[_M0L6_2atmpS2629] = _M0L5d__loS284;
          _M0L6_2atmpS2631 = _M0L6offsetS280 - 2;
          _M0L9remainingS279 = _M0L1tS281;
          _M0L6offsetS280 = _M0L6_2atmpS2631;
          continue;
        } else if (_M0L9remainingS279 >= 10) {
          int32_t _M0L6_2atmpS2643 = _M0L9remainingS279 / 10;
          int32_t _M0L6_2atmpS2642 = 48 + _M0L6_2atmpS2643;
          int32_t _M0L5d__hiS286 = (uint16_t)_M0L6_2atmpS2642;
          int32_t _M0L6_2atmpS2641 = _M0L9remainingS279 % 10;
          int32_t _M0L6_2atmpS2640 = 48 + _M0L6_2atmpS2641;
          int32_t _M0L5d__loS287 = (uint16_t)_M0L6_2atmpS2640;
          int32_t _M0L6_2atmpS2637 = _M0L12digit__startS277 + _M0L6offsetS280;
          int32_t _M0L6_2atmpS2636 = _M0L6_2atmpS2637 - 2;
          int32_t _M0L6_2atmpS2639;
          int32_t _M0L6_2atmpS2638;
          _M0L6bufferS276[_M0L6_2atmpS2636] = _M0L5d__hiS286;
          _M0L6_2atmpS2639 = _M0L12digit__startS277 + _M0L6offsetS280;
          _M0L6_2atmpS2638 = _M0L6_2atmpS2639 - 1;
          _M0L6bufferS276[_M0L6_2atmpS2638] = _M0L5d__loS287;
        } else {
          int32_t _M0L6_2atmpS2647 = _M0L12digit__startS277 + _M0L6offsetS280;
          int32_t _M0L6_2atmpS2644 = _M0L6_2atmpS2647 - 1;
          int32_t _M0L6_2atmpS2646 = 48 + _M0L9remainingS279;
          int32_t _M0L6_2atmpS2645 = (uint16_t)_M0L6_2atmpS2646;
          _M0L6bufferS276[_M0L6_2atmpS2644] = _M0L6_2atmpS2645;
        }
        break;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS256,
  uint32_t _M0L3numS260,
  int32_t _M0L12digit__startS257,
  int32_t _M0L10total__lenS259,
  int32_t _M0L5radixS250
) {
  uint32_t _M0L4baseS249;
  int32_t _M0L6_2atmpS2594;
  int32_t _M0L6_2atmpS2593;
  #line 57 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS249 = *(uint32_t*)&_M0L5radixS250;
  _M0L6_2atmpS2594 = _M0L5radixS250 - 1;
  _M0L6_2atmpS2593 = _M0L5radixS250 & _M0L6_2atmpS2594;
  if (_M0L6_2atmpS2593 == 0) {
    int32_t _M0L5shiftS251;
    uint32_t _M0L4maskS252;
    int32_t _M0L6_2atmpS2601;
    int32_t _M0L6offsetS253;
    uint32_t _M0L1nS254;
    #line 68 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS251 = moonbit_ctz32(_M0L5radixS250);
    _M0L4maskS252 = _M0L4baseS249 - 1u;
    _M0L6_2atmpS2601 = _M0L10total__lenS259 - _M0L12digit__startS257;
    _M0L6offsetS253 = _M0L6_2atmpS2601;
    _M0L1nS254 = _M0L3numS260;
    while (1) {
      if (_M0L1nS254 > 0u) {
        uint32_t _M0L6_2atmpS2600 = _M0L1nS254 & _M0L4maskS252;
        int32_t _M0L5digitS255 = *(int32_t*)&_M0L6_2atmpS2600;
        int32_t _M0L6_2atmpS2597 = _M0L12digit__startS257 + _M0L6offsetS253;
        int32_t _M0L6_2atmpS2595 = _M0L6_2atmpS2597 - 1;
        int32_t _M0L6_2atmpS2596 =
          ((moonbit_string_t)moonbit_string_literal_147.data)[_M0L5digitS255];
        int32_t _M0L6_2atmpS2598;
        uint32_t _M0L6_2atmpS2599;
        _M0L6bufferS256[_M0L6_2atmpS2595] = _M0L6_2atmpS2596;
        _M0L6_2atmpS2598 = _M0L6offsetS253 - 1;
        _M0L6_2atmpS2599 = _M0L1nS254 >> (_M0L5shiftS251 & 31);
        _M0L6offsetS253 = _M0L6_2atmpS2598;
        _M0L1nS254 = _M0L6_2atmpS2599;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2608 = _M0L10total__lenS259 - _M0L12digit__startS257;
    int32_t _M0L6offsetS261 = _M0L6_2atmpS2608;
    uint32_t _M0L1nS262 = _M0L3numS260;
    while (1) {
      if (_M0L1nS262 > 0u) {
        uint32_t _M0L1qS263 = _M0L1nS262 / _M0L4baseS249;
        uint32_t _M0L6_2atmpS2607 = _M0L1qS263 * _M0L4baseS249;
        uint32_t _M0L6_2atmpS2606 = _M0L1nS262 - _M0L6_2atmpS2607;
        int32_t _M0L5digitS264 = *(int32_t*)&_M0L6_2atmpS2606;
        int32_t _M0L6_2atmpS2604 = _M0L12digit__startS257 + _M0L6offsetS261;
        int32_t _M0L6_2atmpS2602 = _M0L6_2atmpS2604 - 1;
        int32_t _M0L6_2atmpS2603 =
          ((moonbit_string_t)moonbit_string_literal_147.data)[_M0L5digitS264];
        int32_t _M0L6_2atmpS2605;
        _M0L6bufferS256[_M0L6_2atmpS2602] = _M0L6_2atmpS2603;
        _M0L6_2atmpS2605 = _M0L6offsetS261 - 1;
        _M0L6offsetS261 = _M0L6_2atmpS2605;
        _M0L1nS262 = _M0L1qS263;
        continue;
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS243,
  uint32_t _M0L3numS248,
  int32_t _M0L12digit__startS244,
  int32_t _M0L10total__lenS247
) {
  int32_t _M0L6_2atmpS2592;
  int32_t _M0L6offsetS238;
  uint32_t _M0L1nS239;
  #line 29 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2592 = _M0L10total__lenS247 - _M0L12digit__startS244;
  _M0L6offsetS238 = _M0L6_2atmpS2592;
  _M0L1nS239 = _M0L3numS248;
  while (1) {
    if (_M0L6offsetS238 >= 2) {
      uint32_t _M0L6_2atmpS2589 = _M0L1nS239 & 255u;
      int32_t _M0L9byte__valS240 = *(int32_t*)&_M0L6_2atmpS2589;
      int32_t _M0L2hiS241 = _M0L9byte__valS240 / 16;
      int32_t _M0L2loS242 = _M0L9byte__valS240 % 16;
      int32_t _M0L6_2atmpS2583 = _M0L12digit__startS244 + _M0L6offsetS238;
      int32_t _M0L6_2atmpS2581 = _M0L6_2atmpS2583 - 2;
      int32_t _M0L6_2atmpS2582 =
        ((moonbit_string_t)moonbit_string_literal_147.data)[_M0L2hiS241];
      int32_t _M0L6_2atmpS2586;
      int32_t _M0L6_2atmpS2584;
      int32_t _M0L6_2atmpS2585;
      int32_t _M0L6_2atmpS2587;
      uint32_t _M0L6_2atmpS2588;
      _M0L6bufferS243[_M0L6_2atmpS2581] = _M0L6_2atmpS2582;
      _M0L6_2atmpS2586 = _M0L12digit__startS244 + _M0L6offsetS238;
      _M0L6_2atmpS2584 = _M0L6_2atmpS2586 - 1;
      _M0L6_2atmpS2585
      = ((moonbit_string_t)moonbit_string_literal_147.data)[
        _M0L2loS242
      ];
      _M0L6bufferS243[_M0L6_2atmpS2584] = _M0L6_2atmpS2585;
      _M0L6_2atmpS2587 = _M0L6offsetS238 - 2;
      _M0L6_2atmpS2588 = _M0L1nS239 >> 8;
      _M0L6offsetS238 = _M0L6_2atmpS2587;
      _M0L1nS239 = _M0L6_2atmpS2588;
      continue;
    } else if (_M0L6offsetS238 == 1) {
      uint32_t _M0L6_2atmpS2591 = _M0L1nS239 & 15u;
      int32_t _M0L6nibbleS246 = *(int32_t*)&_M0L6_2atmpS2591;
      int32_t _M0L6_2atmpS2590 =
        ((moonbit_string_t)moonbit_string_literal_147.data)[_M0L6nibbleS246];
      _M0L6bufferS243[_M0L12digit__startS244] = _M0L6_2atmpS2590;
    }
    break;
  }
  return 0;
}

struct _M0TUssE* _M0MPB4Iter4nextGUssEE(
  struct _M0TPB4IterGUssEE* _M0L4selfS197
) {
  struct _M0TWEOUssE* _M0L7_2afuncS196;
  struct _M0TUssE* _M0L6resultS198;
  int64_t _M0L7_2abindS199;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS196 = _M0L4selfS197->$0;
  moonbit_incref(_M0L7_2afuncS196);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS198 = _M0L7_2afuncS196->code(_M0L7_2afuncS196);
  moonbit_decref(_M0L7_2afuncS196);
  _M0L7_2abindS199 = _M0L4selfS197->$1;
  if (_M0L6resultS198 == 0) {
    _M0L4selfS197->$1 = _M0MPB4Iter4nextN6constrS9981GUssEE;
  } else if (_M0L7_2abindS199 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS200 = _M0L7_2abindS199;
    int32_t _M0L4_2anS201 = (int32_t)_M0L7_2aSomeS200;
    int64_t _M0L6_2atmpS2567;
    if (_M0L4_2anS201 > 0) {
      int32_t _M0L6_2atmpS2568 = _M0L4_2anS201 - 1;
      _M0L6_2atmpS2567 = (int64_t)_M0L6_2atmpS2568;
    } else {
      _M0L6_2atmpS2567 = _M0MPB4Iter4nextN6constrS9980GUssEE;
    }
    _M0L4selfS197->$1 = _M0L6_2atmpS2567;
  }
  return _M0L6resultS198;
}

void* _M0MPB4Iter4nextGRPC16string10StringViewE(
  struct _M0TPB4IterGRPC16string10StringViewE* _M0L4selfS203
) {
  struct _M0TWERPC16option6OptionGRPC16string10StringViewE* _M0L7_2afuncS202;
  void* _M0L6resultS204;
  int64_t _M0L7_2abindS205;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS202 = _M0L4selfS203->$0;
  moonbit_incref(_M0L7_2afuncS202);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS204 = _M0L7_2afuncS202->code(_M0L7_2afuncS202);
  moonbit_decref(_M0L7_2afuncS202);
  _M0L7_2abindS205 = _M0L4selfS203->$1;
  switch (Moonbit_object_tag(_M0L6resultS204)) {
    case 1: {
      if (_M0L7_2abindS205 == 4294967296ll) {
        
      } else {
        int64_t _M0L7_2aSomeS206 = _M0L7_2abindS205;
        int32_t _M0L4_2anS207 = (int32_t)_M0L7_2aSomeS206;
        int64_t _M0L6_2atmpS2569;
        if (_M0L4_2anS207 > 0) {
          int32_t _M0L6_2atmpS2570 = _M0L4_2anS207 - 1;
          _M0L6_2atmpS2569 = (int64_t)_M0L6_2atmpS2570;
        } else {
          _M0L6_2atmpS2569
          = _M0MPB4Iter4nextN6constrS9980GRPC16string10StringViewE;
        }
        _M0L4selfS203->$1 = _M0L6_2atmpS2569;
      }
      break;
    }
    default: {
      _M0L4selfS203->$1
      = _M0MPB4Iter4nextN6constrS9981GRPC16string10StringViewE;
      break;
    }
  }
  return _M0L6resultS204;
}

struct _M0TUsbE* _M0MPB4Iter4nextGUsbEE(
  struct _M0TPB4IterGUsbEE* _M0L4selfS209
) {
  struct _M0TWEOUsbE* _M0L7_2afuncS208;
  struct _M0TUsbE* _M0L6resultS210;
  int64_t _M0L7_2abindS211;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS208 = _M0L4selfS209->$0;
  moonbit_incref(_M0L7_2afuncS208);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS210 = _M0L7_2afuncS208->code(_M0L7_2afuncS208);
  moonbit_decref(_M0L7_2afuncS208);
  _M0L7_2abindS211 = _M0L4selfS209->$1;
  if (_M0L6resultS210 == 0) {
    _M0L4selfS209->$1 = _M0MPB4Iter4nextN6constrS9981GUsbEE;
  } else if (_M0L7_2abindS211 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS212 = _M0L7_2abindS211;
    int32_t _M0L4_2anS213 = (int32_t)_M0L7_2aSomeS212;
    int64_t _M0L6_2atmpS2571;
    if (_M0L4_2anS213 > 0) {
      int32_t _M0L6_2atmpS2572 = _M0L4_2anS213 - 1;
      _M0L6_2atmpS2571 = (int64_t)_M0L6_2atmpS2572;
    } else {
      _M0L6_2atmpS2571 = _M0MPB4Iter4nextN6constrS9980GUsbEE;
    }
    _M0L4selfS209->$1 = _M0L6_2atmpS2571;
  }
  return _M0L6resultS210;
}

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB4Iter4nextGUsRP19moonbitDB10RedisValueEE(
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L4selfS215
) {
  struct _M0TWEOUsRP19moonbitDB10RedisValueE* _M0L7_2afuncS214;
  struct _M0TUsRP19moonbitDB10RedisValueE* _M0L6resultS216;
  int64_t _M0L7_2abindS217;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS214 = _M0L4selfS215->$0;
  moonbit_incref(_M0L7_2afuncS214);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS216 = _M0L7_2afuncS214->code(_M0L7_2afuncS214);
  moonbit_decref(_M0L7_2afuncS214);
  _M0L7_2abindS217 = _M0L4selfS215->$1;
  if (_M0L6resultS216 == 0) {
    _M0L4selfS215->$1
    = _M0MPB4Iter4nextN6constrS9981GUsRP19moonbitDB10RedisValueEE;
  } else if (_M0L7_2abindS217 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS218 = _M0L7_2abindS217;
    int32_t _M0L4_2anS219 = (int32_t)_M0L7_2aSomeS218;
    int64_t _M0L6_2atmpS2573;
    if (_M0L4_2anS219 > 0) {
      int32_t _M0L6_2atmpS2574 = _M0L4_2anS219 - 1;
      _M0L6_2atmpS2573 = (int64_t)_M0L6_2atmpS2574;
    } else {
      _M0L6_2atmpS2573
      = _M0MPB4Iter4nextN6constrS9980GUsRP19moonbitDB10RedisValueEE;
    }
    _M0L4selfS215->$1 = _M0L6_2atmpS2573;
  }
  return _M0L6resultS216;
}

struct _M0TUsfE* _M0MPB4Iter4nextGUsfEE(
  struct _M0TPB4IterGUsfEE* _M0L4selfS221
) {
  struct _M0TWEOUsfE* _M0L7_2afuncS220;
  struct _M0TUsfE* _M0L6resultS222;
  int64_t _M0L7_2abindS223;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS220 = _M0L4selfS221->$0;
  moonbit_incref(_M0L7_2afuncS220);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS222 = _M0L7_2afuncS220->code(_M0L7_2afuncS220);
  moonbit_decref(_M0L7_2afuncS220);
  _M0L7_2abindS223 = _M0L4selfS221->$1;
  if (_M0L6resultS222 == 0) {
    _M0L4selfS221->$1 = _M0MPB4Iter4nextN6constrS9981GUsfEE;
  } else if (_M0L7_2abindS223 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS224 = _M0L7_2abindS223;
    int32_t _M0L4_2anS225 = (int32_t)_M0L7_2aSomeS224;
    int64_t _M0L6_2atmpS2575;
    if (_M0L4_2anS225 > 0) {
      int32_t _M0L6_2atmpS2576 = _M0L4_2anS225 - 1;
      _M0L6_2atmpS2575 = (int64_t)_M0L6_2atmpS2576;
    } else {
      _M0L6_2atmpS2575 = _M0MPB4Iter4nextN6constrS9980GUsfEE;
    }
    _M0L4selfS221->$1 = _M0L6_2atmpS2575;
  }
  return _M0L6resultS222;
}

struct _M0TUsiE* _M0MPB4Iter4nextGUsiEE(
  struct _M0TPB4IterGUsiEE* _M0L4selfS227
) {
  struct _M0TWEOUsiE* _M0L7_2afuncS226;
  struct _M0TUsiE* _M0L6resultS228;
  int64_t _M0L7_2abindS229;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS226 = _M0L4selfS227->$0;
  moonbit_incref(_M0L7_2afuncS226);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS228 = _M0L7_2afuncS226->code(_M0L7_2afuncS226);
  moonbit_decref(_M0L7_2afuncS226);
  _M0L7_2abindS229 = _M0L4selfS227->$1;
  if (_M0L6resultS228 == 0) {
    _M0L4selfS227->$1 = _M0MPB4Iter4nextN6constrS9981GUsiEE;
  } else if (_M0L7_2abindS229 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS230 = _M0L7_2abindS229;
    int32_t _M0L4_2anS231 = (int32_t)_M0L7_2aSomeS230;
    int64_t _M0L6_2atmpS2577;
    if (_M0L4_2anS231 > 0) {
      int32_t _M0L6_2atmpS2578 = _M0L4_2anS231 - 1;
      _M0L6_2atmpS2577 = (int64_t)_M0L6_2atmpS2578;
    } else {
      _M0L6_2atmpS2577 = _M0MPB4Iter4nextN6constrS9980GUsiEE;
    }
    _M0L4selfS227->$1 = _M0L6_2atmpS2577;
  }
  return _M0L6resultS228;
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TPB4IterGcE* _M0L4selfS233) {
  struct _M0TWEOc* _M0L7_2afuncS232;
  int32_t _M0L6resultS234;
  int64_t _M0L7_2abindS235;
  #line 38 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L7_2afuncS232 = _M0L4selfS233->$0;
  moonbit_incref(_M0L7_2afuncS232);
  #line 41 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  _M0L6resultS234 = _M0L7_2afuncS232->code(_M0L7_2afuncS232);
  moonbit_decref(_M0L7_2afuncS232);
  _M0L7_2abindS235 = _M0L4selfS233->$1;
  if (_M0L6resultS234 == -1) {
    _M0L4selfS233->$1 = _M0MPB4Iter4nextN6constrS9981GcE;
  } else if (_M0L7_2abindS235 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS236 = _M0L7_2abindS235;
    int32_t _M0L4_2anS237 = (int32_t)_M0L7_2aSomeS236;
    int64_t _M0L6_2atmpS2579;
    if (_M0L4_2anS237 > 0) {
      int32_t _M0L6_2atmpS2580 = _M0L4_2anS237 - 1;
      _M0L6_2atmpS2579 = (int64_t)_M0L6_2atmpS2580;
    } else {
      _M0L6_2atmpS2579 = _M0MPB4Iter4nextN6constrS9980GcE;
    }
    _M0L4selfS233->$1 = _M0L6_2atmpS2579;
  }
  return _M0L6resultS234;
}

int32_t _M0IP016_24default__implPB4Show6outputGsE(
  moonbit_string_t _M0L4selfS187,
  struct _M0TPB6Logger _M0L6loggerS186
) {
  moonbit_string_t _M0L6_2atmpS2562;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2562 = _M0IPC16string6StringPB4Show10to__string(_M0L4selfS187);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS186.$0->$method_0(_M0L6loggerS186.$1, _M0L6_2atmpS2562);
  moonbit_decref(_M0L6_2atmpS2562);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGiE(
  int32_t _M0L4selfS189,
  struct _M0TPB6Logger _M0L6loggerS188
) {
  moonbit_string_t _M0L6_2atmpS2563;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2563 = _M0IPC13int3IntPB4Show10to__string(_M0L4selfS189);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS188.$0->$method_0(_M0L6loggerS188.$1, _M0L6_2atmpS2563);
  moonbit_decref(_M0L6_2atmpS2563);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGbE(
  int32_t _M0L4selfS191,
  struct _M0TPB6Logger _M0L6loggerS190
) {
  moonbit_string_t _M0L6_2atmpS2564;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2564 = _M0IPC14bool4BoolPB4Show10to__string(_M0L4selfS191);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS190.$0->$method_0(_M0L6loggerS190.$1, _M0L6_2atmpS2564);
  moonbit_decref(_M0L6_2atmpS2564);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGfE(
  float _M0L4selfS193,
  struct _M0TPB6Logger _M0L6loggerS192
) {
  moonbit_string_t _M0L6_2atmpS2565;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2565 = _M0IPC15float5FloatPB4Show10to__string(_M0L4selfS193);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS192.$0->$method_0(_M0L6loggerS192.$1, _M0L6_2atmpS2565);
  moonbit_decref(_M0L6_2atmpS2565);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGmE(
  uint64_t _M0L4selfS195,
  struct _M0TPB6Logger _M0L6loggerS194
) {
  moonbit_string_t _M0L6_2atmpS2566;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2566 = _M0IPC16uint646UInt64PB4Show10to__string(_M0L4selfS195);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS194.$0->$method_0(_M0L6loggerS194.$1, _M0L6_2atmpS2566);
  moonbit_decref(_M0L6_2atmpS2566);
  return 0;
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS185
) {
  #line 99 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  return _M0L4selfS185.$1;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS184
) {
  moonbit_string_t _M0L8_2afieldS5114;
  #line 92 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L8_2afieldS5114 = _M0L4selfS184.$0;
  moonbit_incref(_M0L8_2afieldS5114);
  return _M0L8_2afieldS5114;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS180,
  moonbit_string_t _M0L5valueS181,
  int32_t _M0L5startS182,
  int32_t _M0L3lenS183
) {
  int32_t _M0L6_2atmpS2561;
  int64_t _M0L6_2atmpS2560;
  struct _M0TPC16string10StringView _M0L6_2atmpS2559;
  #line 122 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2561 = _M0L5startS182 + _M0L3lenS183;
  _M0L6_2atmpS2560 = (int64_t)_M0L6_2atmpS2561;
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2559
  = _M0MPC16string6String11sub_2einner(_M0L5valueS181, _M0L5startS182, _M0L6_2atmpS2560);
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS180, _M0L6_2atmpS2559);
  moonbit_decref(_M0L6_2atmpS2559.$0);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS173,
  int32_t _M0L5startS179,
  int64_t _M0L3endS175
) {
  int32_t _M0L3lenS172;
  int32_t _M0L3endS174;
  int32_t _M0L5startS178;
  int32_t _if__result_5710;
  #line 755 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L3lenS172 = Moonbit_array_length(_M0L4selfS173);
  if (_M0L3endS175 == 4294967296ll) {
    _M0L3endS174 = _M0L3lenS172;
  } else {
    int64_t _M0L7_2aSomeS176 = _M0L3endS175;
    int32_t _M0L6_2aendS177 = (int32_t)_M0L7_2aSomeS176;
    if (_M0L6_2aendS177 < 0) {
      _M0L3endS174 = _M0L3lenS172 + _M0L6_2aendS177;
    } else {
      _M0L3endS174 = _M0L6_2aendS177;
    }
  }
  if (_M0L5startS179 < 0) {
    _M0L5startS178 = _M0L3lenS172 + _M0L5startS179;
  } else {
    _M0L5startS178 = _M0L5startS179;
  }
  if (_M0L5startS178 >= 0) {
    if (_M0L5startS178 <= _M0L3endS174) {
      _if__result_5710 = _M0L3endS174 <= _M0L3lenS172;
    } else {
      _if__result_5710 = 0;
    }
  } else {
    _if__result_5710 = 0;
  }
  if (_if__result_5710) {
    if (_M0L5startS178 < _M0L3lenS172) {
      int32_t _M0L6_2atmpS2556 = _M0L4selfS173[_M0L5startS178];
      int32_t _M0L6_2atmpS2555;
      #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2555
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2556);
      if (!_M0L6_2atmpS2555) {
        
      } else {
        #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS174 < _M0L3lenS172) {
      int32_t _M0L6_2atmpS2558 = _M0L4selfS173[_M0L3endS174];
      int32_t _M0L6_2atmpS2557;
      #line 768 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2557
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2558);
      if (!_M0L6_2atmpS2557) {
        
      } else {
        #line 768 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    moonbit_incref(_M0L4selfS173);
    return (struct _M0TPC16string10StringView){.$0 = _M0L4selfS173,
                                                 .$1 = _M0L5startS178,
                                                 .$2 = _M0L3endS174};
  } else {
    #line 763 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB6Logger5writeGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS171,
  struct _M0TPB4Show _M0L4showS170
) {
  struct _M0TPB6Logger _M0L6_2atmpS2554;
  #line 116 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS171);
  _M0L6_2atmpS2554
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS171
  };
  #line 117 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS170.$0->$method_0(_M0L4showS170.$1, _M0L6_2atmpS2554);
  if (_M0L6_2atmpS2554.$1) {
    moonbit_decref(_M0L6_2atmpS2554.$1);
  }
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS169,
  struct _M0TPB4Show _M0L4showS168
) {
  struct _M0TPB6Logger _M0L6_2atmpS2553;
  #line 111 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS169);
  _M0L6_2atmpS2553
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS169
  };
  #line 112 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS168.$0->$method_0(_M0L4showS168.$1, _M0L6_2atmpS2553);
  if (_M0L6_2atmpS2553.$1) {
    moonbit_decref(_M0L6_2atmpS2553.$1);
  }
  return 0;
}

int32_t _M0FPB13finalize__acc(uint32_t _M0L3accS167) {
  uint32_t _M0L6_2atmpS2552;
  #line 444 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  #line 445 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2552 = _M0FPB14avalanche__acc(_M0L3accS167);
  return *(int32_t*)&_M0L6_2atmpS2552;
}

uint32_t _M0FPB14avalanche__acc(uint32_t _M0L3accS166) {
  uint32_t _M0Lm3accS165;
  uint32_t _M0L6_2atmpS2541;
  uint32_t _M0L6_2atmpS2543;
  uint32_t _M0L6_2atmpS2542;
  uint32_t _M0L6_2atmpS2544;
  uint32_t _M0L6_2atmpS2545;
  uint32_t _M0L6_2atmpS2547;
  uint32_t _M0L6_2atmpS2546;
  uint32_t _M0L6_2atmpS2548;
  uint32_t _M0L6_2atmpS2549;
  uint32_t _M0L6_2atmpS2551;
  uint32_t _M0L6_2atmpS2550;
  #line 449 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0Lm3accS165 = _M0L3accS166;
  _M0L6_2atmpS2541 = _M0Lm3accS165;
  _M0L6_2atmpS2543 = _M0Lm3accS165;
  _M0L6_2atmpS2542 = _M0L6_2atmpS2543 >> 15;
  _M0Lm3accS165 = _M0L6_2atmpS2541 ^ _M0L6_2atmpS2542;
  _M0L6_2atmpS2544 = _M0Lm3accS165;
  _M0Lm3accS165 = _M0L6_2atmpS2544 * 2246822519u;
  _M0L6_2atmpS2545 = _M0Lm3accS165;
  _M0L6_2atmpS2547 = _M0Lm3accS165;
  _M0L6_2atmpS2546 = _M0L6_2atmpS2547 >> 13;
  _M0Lm3accS165 = _M0L6_2atmpS2545 ^ _M0L6_2atmpS2546;
  _M0L6_2atmpS2548 = _M0Lm3accS165;
  _M0Lm3accS165 = _M0L6_2atmpS2548 * 3266489917u;
  _M0L6_2atmpS2549 = _M0Lm3accS165;
  _M0L6_2atmpS2551 = _M0Lm3accS165;
  _M0L6_2atmpS2550 = _M0L6_2atmpS2551 >> 16;
  _M0Lm3accS165 = _M0L6_2atmpS2549 ^ _M0L6_2atmpS2550;
  return _M0Lm3accS165;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS164) {
  int64_t _M0L6_2atmpS2540;
  #line 907 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2540 = (int64_t)_M0L4selfS164;
  return *(uint64_t*)&_M0L6_2atmpS2540;
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS163,
  moonbit_string_t _M0L3strS162
) {
  int32_t _M0L8str__lenS161;
  int32_t _M0L3lenS2535;
  int32_t _M0L6_2atmpS2534;
  uint16_t* _M0L4dataS2536;
  int32_t _M0L3lenS2537;
  int32_t _M0L3lenS2539;
  int32_t _M0L6_2atmpS2538;
  #line 86 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS161 = Moonbit_array_length(_M0L3strS162);
  _M0L3lenS2535 = _M0L4selfS163->$1;
  _M0L6_2atmpS2534 = _M0L3lenS2535 + _M0L8str__lenS161;
  #line 88 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS163, _M0L6_2atmpS2534);
  _M0L4dataS2536 = _M0L4selfS163->$0;
  _M0L3lenS2537 = _M0L4selfS163->$1;
  moonbit_incref(_M0L4dataS2536);
  #line 89 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2536, _M0L3lenS2537, _M0L3strS162, 0, _M0L8str__lenS161);
  moonbit_decref(_M0L4dataS2536);
  _M0L3lenS2539 = _M0L4selfS163->$1;
  _M0L6_2atmpS2538 = _M0L3lenS2539 + _M0L8str__lenS161;
  _M0L4selfS163->$1 = _M0L6_2atmpS2538;
  return 0;
}

int32_t _M0MPC15array10FixedArray26unsafe__blit__from__string(
  uint16_t* _M0L4selfS157,
  int32_t _M0L11dst__offsetS160,
  moonbit_string_t _M0L3strS158,
  int32_t _M0L11str__offsetS153,
  int32_t _M0L3lenS154
) {
  int32_t _M0L16end__str__offsetS152;
  int32_t _M0L1iS155;
  int32_t _M0L1jS156;
  #line 71 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L16end__str__offsetS152 = _M0L11str__offsetS153 + _M0L3lenS154;
  _M0L1iS155 = _M0L11str__offsetS153;
  _M0L1jS156 = _M0L11dst__offsetS160;
  while (1) {
    if (_M0L1iS155 < _M0L16end__str__offsetS152) {
      int32_t _M0L6_2atmpS2531 = _M0L3strS158[_M0L1iS155];
      int32_t _M0L6_2atmpS2532;
      int32_t _M0L6_2atmpS2533;
      if (
        _M0L1jS156 < 0 || _M0L1jS156 >= Moonbit_array_length(_M0L4selfS157)
      ) {
        #line 80 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
        moonbit_panic();
      }
      _M0L4selfS157[_M0L1jS156] = _M0L6_2atmpS2531;
      _M0L6_2atmpS2532 = _M0L1iS155 + 1;
      _M0L6_2atmpS2533 = _M0L1jS156 + 1;
      _M0L1iS155 = _M0L6_2atmpS2532;
      _M0L1jS156 = _M0L6_2atmpS2533;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS151) {
  int32_t _M0L6_2atmpS2530;
  #line 68 "/home/developer/.moon/lib/core/builtin/uint16_char.mbt"
  _M0L6_2atmpS2530 = (int32_t)_M0L4selfS151;
  return _M0L6_2atmpS2530;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS149,
  int32_t _M0L8trailingS150
) {
  int32_t _M0L6_2atmpS2529;
  int32_t _M0L6_2atmpS2528;
  int32_t _M0L6_2atmpS2527;
  int32_t _M0L6_2atmpS2526;
  int32_t _M0L6_2atmpS2525;
  #line 40 "/home/developer/.moon/lib/core/builtin/string.mbt"
  _M0L6_2atmpS2529 = _M0L7leadingS149 - 55296;
  _M0L6_2atmpS2528 = _M0L6_2atmpS2529 * 1024;
  _M0L6_2atmpS2527 = _M0L6_2atmpS2528 + _M0L8trailingS150;
  _M0L6_2atmpS2526 = _M0L6_2atmpS2527 - 56320;
  _M0L6_2atmpS2525 = _M0L6_2atmpS2526 + 65536;
  return _M0L6_2atmpS2525;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS148) {
  #line 45 "/home/developer/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS148 >= 56320) {
    return _M0L4selfS148 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS147) {
  #line 28 "/home/developer/.moon/lib/core/builtin/uint16_char.mbt"
  if (_M0L4selfS147 >= 55296) {
    return _M0L4selfS147 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS145,
  int32_t _M0L2chS144
) {
  uint32_t _M0L4codeS143;
  #line 95 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  #line 96 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4codeS143 = _M0MPC14char4Char8to__uint(_M0L2chS144);
  if (_M0L4codeS143 <= 65535u) {
    int32_t _M0L3lenS2504 = _M0L4selfS145->$1;
    int32_t _M0L6_2atmpS2503 = _M0L3lenS2504 + 1;
    uint16_t* _M0L4dataS2505;
    int32_t _M0L3lenS2506;
    int32_t _M0L6_2atmpS2507;
    int32_t _M0L3lenS2509;
    int32_t _M0L6_2atmpS2508;
    #line 98 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS145, _M0L6_2atmpS2503);
    _M0L4dataS2505 = _M0L4selfS145->$0;
    _M0L3lenS2506 = _M0L4selfS145->$1;
    moonbit_incref(_M0L4dataS2505);
    #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2507 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS143);
    if (
      _M0L3lenS2506 < 0
      || _M0L3lenS2506 >= Moonbit_array_length(_M0L4dataS2505)
    ) {
      #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2505[_M0L3lenS2506] = _M0L6_2atmpS2507;
    moonbit_decref(_M0L4dataS2505);
    _M0L3lenS2509 = _M0L4selfS145->$1;
    _M0L6_2atmpS2508 = _M0L3lenS2509 + 1;
    _M0L4selfS145->$1 = _M0L6_2atmpS2508;
  } else if (_M0L4codeS143 <= 1114111u) {
    int32_t _M0L3lenS2511 = _M0L4selfS145->$1;
    int32_t _M0L6_2atmpS2510 = _M0L3lenS2511 + 2;
    uint32_t _M0L4codeS146;
    uint16_t* _M0L4dataS2512;
    int32_t _M0L3lenS2513;
    uint32_t _M0L6_2atmpS2516;
    uint32_t _M0L6_2atmpS2515;
    int32_t _M0L6_2atmpS2514;
    uint16_t* _M0L4dataS2517;
    int32_t _M0L3lenS2522;
    int32_t _M0L6_2atmpS2518;
    uint32_t _M0L6_2atmpS2521;
    uint32_t _M0L6_2atmpS2520;
    int32_t _M0L6_2atmpS2519;
    int32_t _M0L3lenS2524;
    int32_t _M0L6_2atmpS2523;
    #line 102 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS145, _M0L6_2atmpS2510);
    _M0L4codeS146 = _M0L4codeS143 - 65536u;
    _M0L4dataS2512 = _M0L4selfS145->$0;
    _M0L3lenS2513 = _M0L4selfS145->$1;
    _M0L6_2atmpS2516 = _M0L4codeS146 >> 10;
    _M0L6_2atmpS2515 = 55296u + _M0L6_2atmpS2516;
    moonbit_incref(_M0L4dataS2512);
    #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2514 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2515);
    if (
      _M0L3lenS2513 < 0
      || _M0L3lenS2513 >= Moonbit_array_length(_M0L4dataS2512)
    ) {
      #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2512[_M0L3lenS2513] = _M0L6_2atmpS2514;
    moonbit_decref(_M0L4dataS2512);
    _M0L4dataS2517 = _M0L4selfS145->$0;
    _M0L3lenS2522 = _M0L4selfS145->$1;
    _M0L6_2atmpS2518 = _M0L3lenS2522 + 1;
    _M0L6_2atmpS2521 = _M0L4codeS146 & 1023u;
    _M0L6_2atmpS2520 = 56320u + _M0L6_2atmpS2521;
    moonbit_incref(_M0L4dataS2517);
    #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2519 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2520);
    if (
      _M0L6_2atmpS2518 < 0
      || _M0L6_2atmpS2518 >= Moonbit_array_length(_M0L4dataS2517)
    ) {
      #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2517[_M0L6_2atmpS2518] = _M0L6_2atmpS2519;
    moonbit_decref(_M0L4dataS2517);
    _M0L3lenS2524 = _M0L4selfS145->$1;
    _M0L6_2atmpS2523 = _M0L3lenS2524 + 2;
    _M0L4selfS145->$1 = _M0L6_2atmpS2523;
  } else {
    #line 108 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_148.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS137,
  int32_t _M0L8requiredS138
) {
  uint16_t* _M0L4dataS2502;
  int32_t _M0L12current__lenS136;
  int32_t _M0L13enough__spaceS139;
  int32_t _M0L13enough__spaceS140;
  uint16_t* _M0L4dataS2498;
  int32_t _M0L6_2atmpS2499;
  int32_t _M0L3lenS2500;
  uint16_t* _M0L9new__dataS142;
  uint16_t* _M0L6_2aoldS5119;
  #line 46 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS2502 = _M0L4selfS137->$0;
  _M0L12current__lenS136 = Moonbit_array_length(_M0L4dataS2502);
  if (_M0L8requiredS138 <= _M0L12current__lenS136) {
    return 0;
  }
  _M0L13enough__spaceS140 = _M0L12current__lenS136;
  while (1) {
    if (_M0L13enough__spaceS140 < _M0L8requiredS138) {
      int32_t _M0L6_2atmpS2501 = _M0L13enough__spaceS140 * 2;
      _M0L13enough__spaceS140 = _M0L6_2atmpS2501;
      continue;
    } else {
      _M0L13enough__spaceS139 = _M0L13enough__spaceS140;
    }
    break;
  }
  _M0L4dataS2498 = _M0L4selfS137->$0;
  moonbit_incref(_M0L4dataS2498);
  #line 64 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2499 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L3lenS2500 = _M0L4selfS137->$1;
  #line 61 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L9new__dataS142
  = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS2498, _M0L13enough__spaceS139, _M0L6_2atmpS2499, _M0L3lenS2500, 0, 0);
  moonbit_decref(_M0L4dataS2498);
  _M0L6_2aoldS5119 = _M0L4selfS137->$0;
  moonbit_decref(_M0L6_2aoldS5119);
  _M0L4selfS137->$0 = _M0L9new__dataS142;
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS135) {
  int32_t _M0L6_2atmpS2497;
  #line 2676 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2497 = *(int32_t*)&_M0L4selfS135;
  return (uint16_t)_M0L6_2atmpS2497;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS134) {
  int32_t _M0L6_2atmpS2496;
  #line 1254 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2496 = _M0L4selfS134;
  return *(uint32_t*)&_M0L6_2atmpS2496;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS132
) {
  int32_t _M0L3lenS2488;
  uint16_t* _M0L4dataS2490;
  int32_t _M0L6_2atmpS2489;
  #line 148 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS2488 = _M0L4selfS132->$1;
  _M0L4dataS2490 = _M0L4selfS132->$0;
  _M0L6_2atmpS2489 = Moonbit_array_length(_M0L4dataS2490);
  if (_M0L3lenS2488 == _M0L6_2atmpS2489) {
    uint16_t* _M0L4dataS2491 = _M0L4selfS132->$0;
    moonbit_incref(_M0L4dataS2491);
    return _M0L4dataS2491;
  } else {
    uint16_t* _M0L4dataS2492 = _M0L4selfS132->$0;
    int32_t _M0L3lenS2493 = _M0L4selfS132->$1;
    int32_t _M0L6_2atmpS2494;
    int32_t _M0L3lenS2495;
    uint16_t* _M0L4dataS133;
    moonbit_incref(_M0L4dataS2492);
    #line 155 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2494 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L3lenS2495 = _M0L4selfS132->$1;
    #line 152 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L4dataS133
    = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS2492, _M0L3lenS2493, _M0L6_2atmpS2494, _M0L3lenS2495, 0, 0);
    moonbit_decref(_M0L4dataS2492);
    return _M0L4dataS133;
  }
}

int32_t _M0IPC16uint166UInt16PB7Default7default() {
  #line 176 "/home/developer/.moon/lib/core/builtin/uint16_char.mbt"
  return 0;
}

uint16_t* _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(
  uint16_t* _M0L3srcS129,
  int32_t _M0L13allocate__lenS125,
  int32_t _M0L4initS130,
  int32_t _M0L3lenS126,
  int32_t _M0L11src__offsetS127,
  int32_t _M0L11dst__offsetS128
) {
  int32_t _if__result_5713;
  #line 97 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L13allocate__lenS125 >= 0) {
    if (_M0L3lenS126 >= 0) {
      if (_M0L11src__offsetS127 >= 0) {
        if (_M0L11dst__offsetS128 >= 0) {
          int32_t _M0L6_2atmpS2484 = _M0L11src__offsetS127 + _M0L3lenS126;
          int32_t _M0L6_2atmpS2485 = Moonbit_array_length(_M0L3srcS129);
          if (_M0L6_2atmpS2484 <= _M0L6_2atmpS2485) {
            int32_t _M0L6_2atmpS2483 = _M0L11dst__offsetS128 + _M0L3lenS126;
            _if__result_5713 = _M0L6_2atmpS2483 <= _M0L13allocate__lenS125;
          } else {
            _if__result_5713 = 0;
          }
        } else {
          _if__result_5713 = 0;
        }
      } else {
        _if__result_5713 = 0;
      }
    } else {
      _if__result_5713 = 0;
    }
  } else {
    _if__result_5713 = 0;
  }
  if (_if__result_5713) {
    moonbit_incref(_M0L3srcS129);
    #line 115 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    return _M0MPC15array10FixedArray23unsafe__make__and__blitGkE(_M0L3srcS129, _M0L13allocate__lenS125, _M0L4initS130, _M0L11src__offsetS127, _M0L11dst__offsetS128, _M0L3lenS126);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS131;
    int32_t _M0L6_2atmpS2487;
    moonbit_string_t _M0L6_2atmpS2486;
    uint16_t* _result_5714;
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L18_2astring__builderS131
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS131, (moonbit_string_t)moonbit_string_literal_149.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS131, _M0L13allocate__lenS125);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS131, (moonbit_string_t)moonbit_string_literal_150.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS131, _M0L11src__offsetS127);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS131, (moonbit_string_t)moonbit_string_literal_151.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS131, _M0L11dst__offsetS128);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS131, (moonbit_string_t)moonbit_string_literal_152.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS131, _M0L3lenS126);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS131, (moonbit_string_t)moonbit_string_literal_153.data);
    _M0L6_2atmpS2487 = Moonbit_array_length(_M0L3srcS129);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS131, _M0L6_2atmpS2487);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L6_2atmpS2486
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS131);
    moonbit_decref(_M0L18_2astring__builderS131);
    #line 111 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _result_5714 = _M0FPC15abort5abortGAkE(_M0L6_2atmpS2486);
    moonbit_decref(_M0L6_2atmpS2486);
    return _result_5714;
  }
}

uint16_t* _M0MPC15array10FixedArray23unsafe__make__and__blitGkE(
  uint16_t* _M0L3srcS122,
  int32_t _M0L13allocate__lenS119,
  int32_t _M0L4initS120,
  int32_t _M0L11src__offsetS123,
  int32_t _M0L11dst__offsetS121,
  int32_t _M0L9blit__lenS124
) {
  uint16_t* _M0L3dstS118;
  #line 79 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  _M0L3dstS118
  = (uint16_t*)moonbit_make_string(_M0L13allocate__lenS119, _M0L4initS120);
  moonbit_incref(_M0L3dstS118);
  #line 90 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  moonbit_unsafe_val_array_blit(_M0L3dstS118, _M0L11dst__offsetS121, _M0L3srcS122, _M0L11src__offsetS123, _M0L9blit__lenS124, sizeof(uint16_t));
  return _M0L3dstS118;
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder21StringBuilder_2einner(
  int32_t _M0L10size__hintS116
) {
  int32_t _M0L7initialS115;
  uint16_t* _M0L4dataS117;
  struct _M0TPB13StringBuilder* _block_5715;
  #line 32 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS116 < 1) {
    _M0L7initialS115 = 1;
  } else {
    int32_t _M0L6_2atmpS2482 = _M0L10size__hintS116 + 1;
    _M0L7initialS115 = _M0L6_2atmpS2482 / 2;
  }
  _M0L4dataS117 = (uint16_t*)moonbit_make_string(_M0L7initialS115, 0);
  _block_5715
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_5715)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 168, 0);
  _block_5715->$0 = _M0L4dataS117;
  _block_5715->$1 = 0;
  return _block_5715;
}

int32_t _M0MPC13int3Int16unsafe__to__char(int32_t _M0L4selfS114) {
  #line 1532 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  return _M0L4selfS114;
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(
  moonbit_string_t* _M0L3srcS100,
  int32_t _M0L13allocate__lenS96,
  int32_t _M0L3lenS97,
  int32_t _M0L11src__offsetS98,
  int32_t _M0L11dst__offsetS99
) {
  int32_t _if__result_5716;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS96 >= 0) {
    if (_M0L3lenS97 >= 0) {
      if (_M0L11src__offsetS98 >= 0) {
        if (_M0L11dst__offsetS99 >= 0) {
          int32_t _M0L6_2atmpS2468 = _M0L11src__offsetS98 + _M0L3lenS97;
          int32_t _M0L6_2atmpS2469;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2469
          = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS100);
          if (_M0L6_2atmpS2468 <= _M0L6_2atmpS2469) {
            int32_t _M0L6_2atmpS2467 = _M0L11dst__offsetS99 + _M0L3lenS97;
            _if__result_5716 = _M0L6_2atmpS2467 <= _M0L13allocate__lenS96;
          } else {
            _if__result_5716 = 0;
          }
        } else {
          _if__result_5716 = 0;
        }
      } else {
        _if__result_5716 = 0;
      }
    } else {
      _if__result_5716 = 0;
    }
  } else {
    _if__result_5716 = 0;
  }
  if (_if__result_5716) {
    moonbit_incref(_M0L3srcS100);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS96, (moonbit_string_t)moonbit_string_literal_75.data, _M0L3srcS100, _M0L11src__offsetS98, _M0L11dst__offsetS99, _M0L3lenS97);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS101;
    int32_t _M0L6_2atmpS2471;
    moonbit_string_t _M0L6_2atmpS2470;
    moonbit_string_t* _result_5717;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS101
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS101, (moonbit_string_t)moonbit_string_literal_149.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS101, _M0L13allocate__lenS96);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS101, (moonbit_string_t)moonbit_string_literal_150.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS101, _M0L11src__offsetS98);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS101, (moonbit_string_t)moonbit_string_literal_151.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS101, _M0L11dst__offsetS99);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS101, (moonbit_string_t)moonbit_string_literal_152.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS101, _M0L3lenS97);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS101, (moonbit_string_t)moonbit_string_literal_153.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2471 = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS100);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS101, _M0L6_2atmpS2471);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2470
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS101);
    moonbit_decref(_M0L18_2astring__builderS101);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_5717
    = _M0FPC15abort5abortGRPB18UninitializedArrayGsEE(_M0L6_2atmpS2470);
    moonbit_decref(_M0L6_2atmpS2470);
    return _result_5717;
  }
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGOsE(
  moonbit_string_t* _M0L3srcS106,
  int32_t _M0L13allocate__lenS102,
  int32_t _M0L3lenS103,
  int32_t _M0L11src__offsetS104,
  int32_t _M0L11dst__offsetS105
) {
  int32_t _if__result_5718;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS102 >= 0) {
    if (_M0L3lenS103 >= 0) {
      if (_M0L11src__offsetS104 >= 0) {
        if (_M0L11dst__offsetS105 >= 0) {
          int32_t _M0L6_2atmpS2473 = _M0L11src__offsetS104 + _M0L3lenS103;
          int32_t _M0L6_2atmpS2474;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2474
          = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS106);
          if (_M0L6_2atmpS2473 <= _M0L6_2atmpS2474) {
            int32_t _M0L6_2atmpS2472 = _M0L11dst__offsetS105 + _M0L3lenS103;
            _if__result_5718 = _M0L6_2atmpS2472 <= _M0L13allocate__lenS102;
          } else {
            _if__result_5718 = 0;
          }
        } else {
          _if__result_5718 = 0;
        }
      } else {
        _if__result_5718 = 0;
      }
    } else {
      _if__result_5718 = 0;
    }
  } else {
    _if__result_5718 = 0;
  }
  if (_if__result_5718) {
    moonbit_incref(_M0L3srcS106);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS102, 0, _M0L3srcS106, _M0L11src__offsetS104, _M0L11dst__offsetS105, _M0L3lenS103);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS107;
    int32_t _M0L6_2atmpS2476;
    moonbit_string_t _M0L6_2atmpS2475;
    moonbit_string_t* _result_5719;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS107
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS107, (moonbit_string_t)moonbit_string_literal_149.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS107, _M0L13allocate__lenS102);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS107, (moonbit_string_t)moonbit_string_literal_150.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS107, _M0L11src__offsetS104);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS107, (moonbit_string_t)moonbit_string_literal_151.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS107, _M0L11dst__offsetS105);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS107, (moonbit_string_t)moonbit_string_literal_152.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS107, _M0L3lenS103);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS107, (moonbit_string_t)moonbit_string_literal_153.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2476 = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS106);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS107, _M0L6_2atmpS2476);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2475
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS107);
    moonbit_decref(_M0L18_2astring__builderS107);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_5719
    = _M0FPC15abort5abortGRPB18UninitializedArrayGOsEE(_M0L6_2atmpS2475);
    moonbit_decref(_M0L6_2atmpS2475);
    return _result_5719;
  }
}

struct _M0TUsfE** _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(
  struct _M0TUsfE** _M0L3srcS112,
  int32_t _M0L13allocate__lenS108,
  int32_t _M0L3lenS109,
  int32_t _M0L11src__offsetS110,
  int32_t _M0L11dst__offsetS111
) {
  int32_t _if__result_5720;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS108 >= 0) {
    if (_M0L3lenS109 >= 0) {
      if (_M0L11src__offsetS110 >= 0) {
        if (_M0L11dst__offsetS111 >= 0) {
          int32_t _M0L6_2atmpS2478 = _M0L11src__offsetS110 + _M0L3lenS109;
          int32_t _M0L6_2atmpS2479;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2479
          = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS112);
          if (_M0L6_2atmpS2478 <= _M0L6_2atmpS2479) {
            int32_t _M0L6_2atmpS2477 = _M0L11dst__offsetS111 + _M0L3lenS109;
            _if__result_5720 = _M0L6_2atmpS2477 <= _M0L13allocate__lenS108;
          } else {
            _if__result_5720 = 0;
          }
        } else {
          _if__result_5720 = 0;
        }
      } else {
        _if__result_5720 = 0;
      }
    } else {
      _if__result_5720 = 0;
    }
  } else {
    _if__result_5720 = 0;
  }
  if (_if__result_5720) {
    moonbit_incref(_M0L3srcS112);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (struct _M0TUsfE**)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS108, 0, _M0L3srcS112, _M0L11src__offsetS110, _M0L11dst__offsetS111, _M0L3lenS109);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS113;
    int32_t _M0L6_2atmpS2481;
    moonbit_string_t _M0L6_2atmpS2480;
    struct _M0TUsfE** _result_5721;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS113
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS113, (moonbit_string_t)moonbit_string_literal_149.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS113, _M0L13allocate__lenS108);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS113, (moonbit_string_t)moonbit_string_literal_150.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS113, _M0L11src__offsetS110);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS113, (moonbit_string_t)moonbit_string_literal_151.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS113, _M0L11dst__offsetS111);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS113, (moonbit_string_t)moonbit_string_literal_152.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS113, _M0L3lenS109);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS113, (moonbit_string_t)moonbit_string_literal_153.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2481 = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS112);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS113, _M0L6_2atmpS2481);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2480
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS113);
    moonbit_decref(_M0L18_2astring__builderS113);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_5721
    = _M0FPC15abort5abortGRPB18UninitializedArrayGUsfEEE(_M0L6_2atmpS2480);
    moonbit_decref(_M0L6_2atmpS2480);
    return _result_5721;
  }
}

int32_t _M0MPB13StringBuilder13write__objectGsE(
  struct _M0TPB13StringBuilder* _M0L4selfS85,
  moonbit_string_t _M0L3objS84
) {
  struct _M0TPB6Logger _M0L6_2atmpS2461;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS85);
  _M0L6_2atmpS2461
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS85
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGsE(_M0L3objS84, _M0L6_2atmpS2461);
  if (_M0L6_2atmpS2461.$1) {
    moonbit_decref(_M0L6_2atmpS2461.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGiE(
  struct _M0TPB13StringBuilder* _M0L4selfS87,
  int32_t _M0L3objS86
) {
  struct _M0TPB6Logger _M0L6_2atmpS2462;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS87);
  _M0L6_2atmpS2462
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS87
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGiE(_M0L3objS86, _M0L6_2atmpS2462);
  if (_M0L6_2atmpS2462.$1) {
    moonbit_decref(_M0L6_2atmpS2462.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGbE(
  struct _M0TPB13StringBuilder* _M0L4selfS89,
  int32_t _M0L3objS88
) {
  struct _M0TPB6Logger _M0L6_2atmpS2463;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS89);
  _M0L6_2atmpS2463
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS89
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGbE(_M0L3objS88, _M0L6_2atmpS2463);
  if (_M0L6_2atmpS2463.$1) {
    moonbit_decref(_M0L6_2atmpS2463.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGfE(
  struct _M0TPB13StringBuilder* _M0L4selfS91,
  float _M0L3objS90
) {
  struct _M0TPB6Logger _M0L6_2atmpS2464;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS91);
  _M0L6_2atmpS2464
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS91
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGfE(_M0L3objS90, _M0L6_2atmpS2464);
  if (_M0L6_2atmpS2464.$1) {
    moonbit_decref(_M0L6_2atmpS2464.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS93,
  struct _M0TPC16string10StringView _M0L3objS92
) {
  struct _M0TPB6Logger _M0L6_2atmpS2465;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS93);
  _M0L6_2atmpS2465
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS93
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS92, _M0L6_2atmpS2465);
  if (_M0L6_2atmpS2465.$1) {
    moonbit_decref(_M0L6_2atmpS2465.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGmE(
  struct _M0TPB13StringBuilder* _M0L4selfS95,
  uint64_t _M0L3objS94
) {
  struct _M0TPB6Logger _M0L6_2atmpS2466;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS95);
  _M0L6_2atmpS2466
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS95
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGmE(_M0L3objS94, _M0L6_2atmpS2466);
  if (_M0L6_2atmpS2466.$1) {
    moonbit_decref(_M0L6_2atmpS2466.$1);
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

moonbit_string_t* _M0MPB18UninitializedArray23unsafe__make__and__blitGOsE(
  moonbit_string_t* _M0L3srcS75,
  int32_t _M0L13allocate__lenS73,
  int32_t _M0L11src__offsetS76,
  int32_t _M0L11dst__offsetS74,
  int32_t _M0L9blit__lenS77
) {
  moonbit_string_t* _M0L3dstS72;
  #line 133 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0L3dstS72
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13allocate__lenS73, 0);
  #line 143 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGOsE(_M0L3dstS72, _M0L11dst__offsetS74, _M0L3srcS75, _M0L11src__offsetS76, _M0L9blit__lenS77);
  moonbit_decref(_M0L3srcS75);
  return _M0L3dstS72;
}

struct _M0TUsfE** _M0MPB18UninitializedArray23unsafe__make__and__blitGUsfEE(
  struct _M0TUsfE** _M0L3srcS81,
  int32_t _M0L13allocate__lenS79,
  int32_t _M0L11src__offsetS82,
  int32_t _M0L11dst__offsetS80,
  int32_t _M0L9blit__lenS83
) {
  struct _M0TUsfE** _M0L3dstS78;
  #line 133 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0L3dstS78
  = (struct _M0TUsfE**)moonbit_make_ref_array(_M0L13allocate__lenS79, 0);
  #line 143 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsfEE(_M0L3dstS78, _M0L11dst__offsetS80, _M0L3srcS81, _M0L11src__offsetS82, _M0L9blit__lenS83);
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

int32_t _M0MPB18UninitializedArray12unsafe__blitGOsE(
  moonbit_string_t* _M0L3dstS56,
  int32_t _M0L11dst__offsetS57,
  moonbit_string_t* _M0L3srcS58,
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

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsfEE(
  struct _M0TUsfE** _M0L3dstS61,
  int32_t _M0L11dst__offsetS62,
  struct _M0TUsfE** _M0L3srcS63,
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
  int32_t _if__result_5722;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS15 == _M0L3srcS16) {
    _if__result_5722 = _M0L11dst__offsetS17 < _M0L11src__offsetS18;
  } else {
    _if__result_5722 = 0;
  }
  if (_if__result_5722) {
    int32_t _M0L1iS19 = 0;
    while (1) {
      if (_M0L1iS19 < _M0L3lenS20) {
        int32_t _M0L6_2atmpS2425 = _M0L11dst__offsetS17 + _M0L1iS19;
        int32_t _M0L6_2atmpS2427 = _M0L11src__offsetS18 + _M0L1iS19;
        int32_t _M0L6_2atmpS2426;
        int32_t _M0L6_2atmpS2428;
        if (
          _M0L6_2atmpS2427 < 0
          || _M0L6_2atmpS2427 >= Moonbit_array_length(_M0L3srcS16)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2426 = (int32_t)_M0L3srcS16[_M0L6_2atmpS2427];
        if (
          _M0L6_2atmpS2425 < 0
          || _M0L6_2atmpS2425 >= Moonbit_array_length(_M0L3dstS15)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS15[_M0L6_2atmpS2425] = _M0L6_2atmpS2426;
        _M0L6_2atmpS2428 = _M0L1iS19 + 1;
        _M0L1iS19 = _M0L6_2atmpS2428;
        continue;
      } else {
        moonbit_decref(_M0L3srcS16);
        moonbit_decref(_M0L3dstS15);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2433 = _M0L3lenS20 - 1;
    int32_t _M0L1iS22 = _M0L6_2atmpS2433;
    while (1) {
      if (_M0L1iS22 >= 0) {
        int32_t _M0L6_2atmpS2429 = _M0L11dst__offsetS17 + _M0L1iS22;
        int32_t _M0L6_2atmpS2431 = _M0L11src__offsetS18 + _M0L1iS22;
        int32_t _M0L6_2atmpS2430;
        int32_t _M0L6_2atmpS2432;
        if (
          _M0L6_2atmpS2431 < 0
          || _M0L6_2atmpS2431 >= Moonbit_array_length(_M0L3srcS16)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2430 = (int32_t)_M0L3srcS16[_M0L6_2atmpS2431];
        if (
          _M0L6_2atmpS2429 < 0
          || _M0L6_2atmpS2429 >= Moonbit_array_length(_M0L3dstS15)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS15[_M0L6_2atmpS2429] = _M0L6_2atmpS2430;
        _M0L6_2atmpS2432 = _M0L1iS22 - 1;
        _M0L1iS22 = _M0L6_2atmpS2432;
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
  int32_t _if__result_5725;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS24 == _M0L3srcS25) {
    _if__result_5725 = _M0L11dst__offsetS26 < _M0L11src__offsetS27;
  } else {
    _if__result_5725 = 0;
  }
  if (_if__result_5725) {
    int32_t _M0L1iS28 = 0;
    while (1) {
      if (_M0L1iS28 < _M0L3lenS29) {
        int32_t _M0L6_2atmpS2434 = _M0L11dst__offsetS26 + _M0L1iS28;
        int32_t _M0L6_2atmpS2436 = _M0L11src__offsetS27 + _M0L1iS28;
        moonbit_string_t _M0L6_2atmpS2435;
        moonbit_string_t _M0L6_2aoldS5125;
        int32_t _M0L6_2atmpS2437;
        if (
          _M0L6_2atmpS2436 < 0
          || _M0L6_2atmpS2436 >= Moonbit_array_length(_M0L3srcS25)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2435 = (moonbit_string_t)_M0L3srcS25[_M0L6_2atmpS2436];
        if (
          _M0L6_2atmpS2434 < 0
          || _M0L6_2atmpS2434 >= Moonbit_array_length(_M0L3dstS24)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS5125 = (moonbit_string_t)_M0L3dstS24[_M0L6_2atmpS2434];
        moonbit_incref(_M0L6_2atmpS2435);
        moonbit_decref(_M0L6_2aoldS5125);
        _M0L3dstS24[_M0L6_2atmpS2434] = _M0L6_2atmpS2435;
        _M0L6_2atmpS2437 = _M0L1iS28 + 1;
        _M0L1iS28 = _M0L6_2atmpS2437;
        continue;
      } else {
        moonbit_decref(_M0L3srcS25);
        moonbit_decref(_M0L3dstS24);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2442 = _M0L3lenS29 - 1;
    int32_t _M0L1iS31 = _M0L6_2atmpS2442;
    while (1) {
      if (_M0L1iS31 >= 0) {
        int32_t _M0L6_2atmpS2438 = _M0L11dst__offsetS26 + _M0L1iS31;
        int32_t _M0L6_2atmpS2440 = _M0L11src__offsetS27 + _M0L1iS31;
        moonbit_string_t _M0L6_2atmpS2439;
        moonbit_string_t _M0L6_2aoldS5127;
        int32_t _M0L6_2atmpS2441;
        if (
          _M0L6_2atmpS2440 < 0
          || _M0L6_2atmpS2440 >= Moonbit_array_length(_M0L3srcS25)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2439 = (moonbit_string_t)_M0L3srcS25[_M0L6_2atmpS2440];
        if (
          _M0L6_2atmpS2438 < 0
          || _M0L6_2atmpS2438 >= Moonbit_array_length(_M0L3dstS24)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS5127 = (moonbit_string_t)_M0L3dstS24[_M0L6_2atmpS2438];
        moonbit_incref(_M0L6_2atmpS2439);
        moonbit_decref(_M0L6_2aoldS5127);
        _M0L3dstS24[_M0L6_2atmpS2438] = _M0L6_2atmpS2439;
        _M0L6_2atmpS2441 = _M0L1iS31 - 1;
        _M0L1iS31 = _M0L6_2atmpS2441;
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGOsEE(
  moonbit_string_t* _M0L3dstS33,
  int32_t _M0L11dst__offsetS35,
  moonbit_string_t* _M0L3srcS34,
  int32_t _M0L11src__offsetS36,
  int32_t _M0L3lenS38
) {
  int32_t _if__result_5728;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS33 == _M0L3srcS34) {
    _if__result_5728 = _M0L11dst__offsetS35 < _M0L11src__offsetS36;
  } else {
    _if__result_5728 = 0;
  }
  if (_if__result_5728) {
    int32_t _M0L1iS37 = 0;
    while (1) {
      if (_M0L1iS37 < _M0L3lenS38) {
        int32_t _M0L6_2atmpS2443 = _M0L11dst__offsetS35 + _M0L1iS37;
        int32_t _M0L6_2atmpS2445 = _M0L11src__offsetS36 + _M0L1iS37;
        moonbit_string_t _M0L6_2atmpS2444;
        moonbit_string_t _M0L6_2aoldS5129;
        int32_t _M0L6_2atmpS2446;
        if (
          _M0L6_2atmpS2445 < 0
          || _M0L6_2atmpS2445 >= Moonbit_array_length(_M0L3srcS34)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2444 = (moonbit_string_t)_M0L3srcS34[_M0L6_2atmpS2445];
        if (
          _M0L6_2atmpS2443 < 0
          || _M0L6_2atmpS2443 >= Moonbit_array_length(_M0L3dstS33)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS5129 = (moonbit_string_t)_M0L3dstS33[_M0L6_2atmpS2443];
        if (_M0L6_2atmpS2444) {
          moonbit_incref(_M0L6_2atmpS2444);
        }
        if (_M0L6_2aoldS5129) {
          moonbit_decref(_M0L6_2aoldS5129);
        }
        _M0L3dstS33[_M0L6_2atmpS2443] = _M0L6_2atmpS2444;
        _M0L6_2atmpS2446 = _M0L1iS37 + 1;
        _M0L1iS37 = _M0L6_2atmpS2446;
        continue;
      } else {
        moonbit_decref(_M0L3srcS34);
        moonbit_decref(_M0L3dstS33);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2451 = _M0L3lenS38 - 1;
    int32_t _M0L1iS40 = _M0L6_2atmpS2451;
    while (1) {
      if (_M0L1iS40 >= 0) {
        int32_t _M0L6_2atmpS2447 = _M0L11dst__offsetS35 + _M0L1iS40;
        int32_t _M0L6_2atmpS2449 = _M0L11src__offsetS36 + _M0L1iS40;
        moonbit_string_t _M0L6_2atmpS2448;
        moonbit_string_t _M0L6_2aoldS5131;
        int32_t _M0L6_2atmpS2450;
        if (
          _M0L6_2atmpS2449 < 0
          || _M0L6_2atmpS2449 >= Moonbit_array_length(_M0L3srcS34)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2448 = (moonbit_string_t)_M0L3srcS34[_M0L6_2atmpS2449];
        if (
          _M0L6_2atmpS2447 < 0
          || _M0L6_2atmpS2447 >= Moonbit_array_length(_M0L3dstS33)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS5131 = (moonbit_string_t)_M0L3dstS33[_M0L6_2atmpS2447];
        if (_M0L6_2atmpS2448) {
          moonbit_incref(_M0L6_2atmpS2448);
        }
        if (_M0L6_2aoldS5131) {
          moonbit_decref(_M0L6_2aoldS5131);
        }
        _M0L3dstS33[_M0L6_2atmpS2447] = _M0L6_2atmpS2448;
        _M0L6_2atmpS2450 = _M0L1iS40 - 1;
        _M0L1iS40 = _M0L6_2atmpS2450;
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsfEEE(
  struct _M0TUsfE** _M0L3dstS42,
  int32_t _M0L11dst__offsetS44,
  struct _M0TUsfE** _M0L3srcS43,
  int32_t _M0L11src__offsetS45,
  int32_t _M0L3lenS47
) {
  int32_t _if__result_5731;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS42 == _M0L3srcS43) {
    _if__result_5731 = _M0L11dst__offsetS44 < _M0L11src__offsetS45;
  } else {
    _if__result_5731 = 0;
  }
  if (_if__result_5731) {
    int32_t _M0L1iS46 = 0;
    while (1) {
      if (_M0L1iS46 < _M0L3lenS47) {
        int32_t _M0L6_2atmpS2452 = _M0L11dst__offsetS44 + _M0L1iS46;
        int32_t _M0L6_2atmpS2454 = _M0L11src__offsetS45 + _M0L1iS46;
        struct _M0TUsfE* _M0L6_2atmpS2453;
        struct _M0TUsfE* _M0L6_2aoldS5133;
        int32_t _M0L6_2atmpS2455;
        if (
          _M0L6_2atmpS2454 < 0
          || _M0L6_2atmpS2454 >= Moonbit_array_length(_M0L3srcS43)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2453 = (struct _M0TUsfE*)_M0L3srcS43[_M0L6_2atmpS2454];
        if (
          _M0L6_2atmpS2452 < 0
          || _M0L6_2atmpS2452 >= Moonbit_array_length(_M0L3dstS42)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS5133 = (struct _M0TUsfE*)_M0L3dstS42[_M0L6_2atmpS2452];
        if (_M0L6_2atmpS2453) {
          moonbit_incref(_M0L6_2atmpS2453);
        }
        if (_M0L6_2aoldS5133) {
          moonbit_decref(_M0L6_2aoldS5133);
        }
        _M0L3dstS42[_M0L6_2atmpS2452] = _M0L6_2atmpS2453;
        _M0L6_2atmpS2455 = _M0L1iS46 + 1;
        _M0L1iS46 = _M0L6_2atmpS2455;
        continue;
      } else {
        moonbit_decref(_M0L3srcS43);
        moonbit_decref(_M0L3dstS42);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2460 = _M0L3lenS47 - 1;
    int32_t _M0L1iS49 = _M0L6_2atmpS2460;
    while (1) {
      if (_M0L1iS49 >= 0) {
        int32_t _M0L6_2atmpS2456 = _M0L11dst__offsetS44 + _M0L1iS49;
        int32_t _M0L6_2atmpS2458 = _M0L11src__offsetS45 + _M0L1iS49;
        struct _M0TUsfE* _M0L6_2atmpS2457;
        struct _M0TUsfE* _M0L6_2aoldS5135;
        int32_t _M0L6_2atmpS2459;
        if (
          _M0L6_2atmpS2458 < 0
          || _M0L6_2atmpS2458 >= Moonbit_array_length(_M0L3srcS43)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2457 = (struct _M0TUsfE*)_M0L3srcS43[_M0L6_2atmpS2458];
        if (
          _M0L6_2atmpS2456 < 0
          || _M0L6_2atmpS2456 >= Moonbit_array_length(_M0L3dstS42)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS5135 = (struct _M0TUsfE*)_M0L3dstS42[_M0L6_2atmpS2456];
        if (_M0L6_2atmpS2457) {
          moonbit_incref(_M0L6_2atmpS2457);
        }
        if (_M0L6_2aoldS5135) {
          moonbit_decref(_M0L6_2aoldS5135);
        }
        _M0L3dstS42[_M0L6_2atmpS2456] = _M0L6_2atmpS2457;
        _M0L6_2atmpS2459 = _M0L1iS49 - 1;
        _M0L1iS49 = _M0L6_2atmpS2459;
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

int32_t _M0MPB18UninitializedArray6lengthGOsE(moonbit_string_t* _M0L4selfS13) {
  #line 113 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  return Moonbit_array_length(_M0L4selfS13);
}

int32_t _M0MPB18UninitializedArray6lengthGUsfEE(
  struct _M0TUsfE** _M0L4selfS14
) {
  #line 113 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  return Moonbit_array_length(_M0L4selfS14);
}

uint32_t _M0FPB13consume4__acc(uint32_t _M0L3accS10, uint32_t _M0L5inputS11) {
  uint32_t _M0L6_2atmpS2424;
  uint32_t _M0L6_2atmpS2423;
  uint32_t _M0L6_2atmpS2422;
  #line 465 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2424 = _M0L5inputS11 * 3266489917u;
  _M0L6_2atmpS2423 = _M0L3accS10 + _M0L6_2atmpS2424;
  #line 466 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2422 = _M0FPB4rotl(_M0L6_2atmpS2423, 17);
  return _M0L6_2atmpS2422 * 668265263u;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS8, int32_t _M0L1rS9) {
  uint32_t _M0L6_2atmpS2419;
  int32_t _M0L6_2atmpS2421;
  uint32_t _M0L6_2atmpS2420;
  #line 475 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2419 = _M0L1xS8 << (_M0L1rS9 & 31);
  _M0L6_2atmpS2421 = 32 - _M0L1rS9;
  _M0L6_2atmpS2420 = _M0L1xS8 >> (_M0L6_2atmpS2421 & 31);
  return _M0L6_2atmpS2419 | _M0L6_2atmpS2420;
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

moonbit_string_t* _M0FPC15abort5abortGRPB18UninitializedArrayGOsEE(
  moonbit_string_t _M0L3msgS5
) {
  #line 47 "/home/developer/.moon/lib/core/abort/abort.mbt"
  #line 49 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_println(_M0L3msgS5);
  #line 50 "/home/developer/.moon/lib/core/abort/abort.mbt"
  moonbit_panic();
}

struct _M0TUsfE** _M0FPC15abort5abortGRPB18UninitializedArrayGUsfEEE(
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
  void* _M0L11_2aobj__ptrS2411,
  struct _M0TPB4Show _M0L8_2aparamS2410
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2409 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2411;
  _M0IP016_24default__implPB6Logger5writeGRPB13StringBuilderE(_M0L7_2aselfS2409, _M0L8_2aparamS2410);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger84write__string__interpolation_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS2408,
  struct _M0TPB4Show _M0L8_2aparamS2407
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2406 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2408;
  _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(_M0L7_2aselfS2406, _M0L8_2aparamS2407);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS2405,
  int32_t _M0L8_2aparamS2404
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2403 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2405;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS2403, _M0L8_2aparamS2404);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS2402,
  struct _M0TPC16string10StringView _M0L8_2aparamS2401
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2400 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2402;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS2400, _M0L8_2aparamS2401);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS2399,
  moonbit_string_t _M0L8_2aparamS2396,
  int32_t _M0L8_2aparamS2397,
  int32_t _M0L8_2aparamS2398
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2395 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2399;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS2395, _M0L8_2aparamS2396, _M0L8_2aparamS2397, _M0L8_2aparamS2398);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS2394,
  moonbit_string_t _M0L8_2aparamS2393
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2392 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2394;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS2392, _M0L8_2aparamS2393);
  return 0;
}

void moonbit_init() {
  moonbit_layout_table = moonbit_layout_table_data;
}

int main(int argc, char** argv) {
  struct _M0TP19moonbitDB8Database* _M0L2dbS2376;
  moonbit_string_t* _M0L6_2atmpS2418;
  struct _M0TPB5ArrayGsE* _M0L14demo__commandsS2377;
  int32_t _M0L7_2abindS2378;
  int32_t _M0L2__S2379;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  #line 432 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_154.data);
  #line 433 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_155.data);
  #line 434 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_154.data);
  #line 435 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_75.data);
  #line 437 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0L2dbS2376 = _M0MP19moonbitDB8Database3new();
  _M0L6_2atmpS2418 = (moonbit_string_t*)moonbit_make_ref_array_raw(46);
  _M0L6_2atmpS2418[0] = (moonbit_string_t)moonbit_string_literal_156.data;
  _M0L6_2atmpS2418[1] = (moonbit_string_t)moonbit_string_literal_157.data;
  _M0L6_2atmpS2418[2] = (moonbit_string_t)moonbit_string_literal_158.data;
  _M0L6_2atmpS2418[3] = (moonbit_string_t)moonbit_string_literal_159.data;
  _M0L6_2atmpS2418[4] = (moonbit_string_t)moonbit_string_literal_160.data;
  _M0L6_2atmpS2418[5] = (moonbit_string_t)moonbit_string_literal_159.data;
  _M0L6_2atmpS2418[6] = (moonbit_string_t)moonbit_string_literal_161.data;
  _M0L6_2atmpS2418[7] = (moonbit_string_t)moonbit_string_literal_158.data;
  _M0L6_2atmpS2418[8] = (moonbit_string_t)moonbit_string_literal_162.data;
  _M0L6_2atmpS2418[9] = (moonbit_string_t)moonbit_string_literal_163.data;
  _M0L6_2atmpS2418[10] = (moonbit_string_t)moonbit_string_literal_164.data;
  _M0L6_2atmpS2418[11] = (moonbit_string_t)moonbit_string_literal_165.data;
  _M0L6_2atmpS2418[12] = (moonbit_string_t)moonbit_string_literal_166.data;
  _M0L6_2atmpS2418[13] = (moonbit_string_t)moonbit_string_literal_167.data;
  _M0L6_2atmpS2418[14] = (moonbit_string_t)moonbit_string_literal_168.data;
  _M0L6_2atmpS2418[15] = (moonbit_string_t)moonbit_string_literal_169.data;
  _M0L6_2atmpS2418[16] = (moonbit_string_t)moonbit_string_literal_170.data;
  _M0L6_2atmpS2418[17] = (moonbit_string_t)moonbit_string_literal_171.data;
  _M0L6_2atmpS2418[18] = (moonbit_string_t)moonbit_string_literal_172.data;
  _M0L6_2atmpS2418[19] = (moonbit_string_t)moonbit_string_literal_173.data;
  _M0L6_2atmpS2418[20] = (moonbit_string_t)moonbit_string_literal_174.data;
  _M0L6_2atmpS2418[21] = (moonbit_string_t)moonbit_string_literal_175.data;
  _M0L6_2atmpS2418[22] = (moonbit_string_t)moonbit_string_literal_176.data;
  _M0L6_2atmpS2418[23] = (moonbit_string_t)moonbit_string_literal_177.data;
  _M0L6_2atmpS2418[24] = (moonbit_string_t)moonbit_string_literal_178.data;
  _M0L6_2atmpS2418[25] = (moonbit_string_t)moonbit_string_literal_179.data;
  _M0L6_2atmpS2418[26] = (moonbit_string_t)moonbit_string_literal_180.data;
  _M0L6_2atmpS2418[27] = (moonbit_string_t)moonbit_string_literal_181.data;
  _M0L6_2atmpS2418[28] = (moonbit_string_t)moonbit_string_literal_182.data;
  _M0L6_2atmpS2418[29] = (moonbit_string_t)moonbit_string_literal_183.data;
  _M0L6_2atmpS2418[30] = (moonbit_string_t)moonbit_string_literal_184.data;
  _M0L6_2atmpS2418[31] = (moonbit_string_t)moonbit_string_literal_185.data;
  _M0L6_2atmpS2418[32] = (moonbit_string_t)moonbit_string_literal_183.data;
  _M0L6_2atmpS2418[33] = (moonbit_string_t)moonbit_string_literal_186.data;
  _M0L6_2atmpS2418[34] = (moonbit_string_t)moonbit_string_literal_187.data;
  _M0L6_2atmpS2418[35] = (moonbit_string_t)moonbit_string_literal_69.data;
  _M0L6_2atmpS2418[36] = (moonbit_string_t)moonbit_string_literal_188.data;
  _M0L6_2atmpS2418[37] = (moonbit_string_t)moonbit_string_literal_189.data;
  _M0L6_2atmpS2418[38] = (moonbit_string_t)moonbit_string_literal_190.data;
  _M0L6_2atmpS2418[39] = (moonbit_string_t)moonbit_string_literal_191.data;
  _M0L6_2atmpS2418[40] = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L6_2atmpS2418[41] = (moonbit_string_t)moonbit_string_literal_192.data;
  _M0L6_2atmpS2418[42] = (moonbit_string_t)moonbit_string_literal_193.data;
  _M0L6_2atmpS2418[43] = (moonbit_string_t)moonbit_string_literal_164.data;
  _M0L6_2atmpS2418[44] = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L6_2atmpS2418[45] = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L14demo__commandsS2377
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L14demo__commandsS2377)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L14demo__commandsS2377->$0 = _M0L6_2atmpS2418;
  _M0L14demo__commandsS2377->$1 = 46;
  #line 488 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_194.data);
  #line 489 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_195.data);
  _M0L7_2abindS2378 = _M0L14demo__commandsS2377->$1;
  _M0L2__S2379 = 0;
  while (1) {
    if (_M0L2__S2379 < _M0L7_2abindS2378) {
      moonbit_string_t* _M0L3bufS2417 = _M0L14demo__commandsS2377->$0;
      moonbit_string_t _M0L3cmdS2380 =
        (moonbit_string_t)_M0L3bufS2417[_M0L2__S2379];
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2381;
      moonbit_string_t _M0L6_2atmpS2412;
      struct _M0TPB5ArrayGsE* _M0L6resultS2382;
      int32_t _M0L7_2abindS2383;
      int32_t _M0L2__S2384;
      int32_t _M0L6_2atmpS2416;
      moonbit_incref(_M0L3cmdS2380);
      #line 491 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_75.data);
      #line 492 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L18_2astring__builderS2381
      = _M0MPB13StringBuilder21StringBuilder_2einner(4);
      #line 492 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2381, (moonbit_string_t)moonbit_string_literal_196.data);
      #line 492 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2381, _M0L3cmdS2380);
      #line 492 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6_2atmpS2412
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2381);
      moonbit_decref(_M0L18_2astring__builderS2381);
      #line 492 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2412);
      moonbit_decref(_M0L6_2atmpS2412);
      #line 493 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
      _M0L6resultS2382
      = _M0FP39moonbitDB8examples9cli__repl16execute__command(_M0L2dbS2376, _M0L3cmdS2380);
      moonbit_decref(_M0L3cmdS2380);
      _M0L7_2abindS2383 = _M0L6resultS2382->$1;
      _M0L2__S2384 = 0;
      while (1) {
        if (_M0L2__S2384 < _M0L7_2abindS2383) {
          moonbit_string_t* _M0L3bufS2415 = _M0L6resultS2382->$0;
          moonbit_string_t _M0L4lineS2385 =
            (moonbit_string_t)_M0L3bufS2415[_M0L2__S2384];
          struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2386;
          moonbit_string_t _M0L6_2atmpS2413;
          int32_t _M0L6_2atmpS2414;
          moonbit_incref(_M0L4lineS2385);
          #line 495 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L18_2astring__builderS2386
          = _M0MPB13StringBuilder21StringBuilder_2einner(2);
          #line 495 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2386, (moonbit_string_t)moonbit_string_literal_23.data);
          #line 495 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2386, _M0L4lineS2385);
          moonbit_decref(_M0L4lineS2385);
          #line 495 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0L6_2atmpS2413
          = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2386);
          moonbit_decref(_M0L18_2astring__builderS2386);
          #line 495 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
          _M0FPB7printlnGsE(_M0L6_2atmpS2413);
          moonbit_decref(_M0L6_2atmpS2413);
          _M0L6_2atmpS2414 = _M0L2__S2384 + 1;
          _M0L2__S2384 = _M0L6_2atmpS2414;
          continue;
        } else {
          moonbit_decref(_M0L6resultS2382);
        }
        break;
      }
      _M0L6_2atmpS2416 = _M0L2__S2379 + 1;
      _M0L2__S2379 = _M0L6_2atmpS2416;
      continue;
    } else {
      moonbit_decref(_M0L14demo__commandsS2377);
      moonbit_decref(_M0L2dbS2376);
    }
    break;
  }
  #line 499 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_75.data);
  #line 500 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_154.data);
  #line 501 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_197.data);
  #line 502 "/home/developer/Documents2/moonbitDB/examples/cli_repl/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_154.data);
  return 0;
}