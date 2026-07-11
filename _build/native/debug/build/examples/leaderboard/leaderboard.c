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
struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3233__l711__;

struct _M0TPB9ArrayViewGUsfEE;

struct _M0TP19moonbitDB8Database;

struct _M0TWEOUssE;

struct _M0TWEOUsRP19moonbitDB10RedisValueE;

struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE;

struct _M0TUsiE;

struct _M0TPB3MapGsfE;

struct _M0TUsbE;

struct _M0TPB13StringBuilder;

struct _M0TPB9ArrayViewGUsiEE;

struct _M0TPB17FloatingDecimal64;

struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE;

struct _M0TPB5EntryGssE;

struct _M0TUssE;

struct _M0TPB3MapGsRP19moonbitDB10RedisValueE;

struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3243__l711__;

struct _M0BTPB6Logger;

struct _M0DTP19moonbitDB10RedisValue6String;

struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE;

struct _M0DTP19moonbitDB10RedisValue4Hash;

struct _M0TPB6Logger;

struct _M0TP19moonbitDB5Deque;

struct _M0TUsfE;

struct _M0TPB8MutLocalGORPB5EntryGsfEE;

struct _M0TPB5EntryGsbE;

struct _M0TPB8MutLocalGORPB5EntryGssEE;

struct _M0DTPC16option6OptionGfE4Some;

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3223__l711__;

struct _M0TPB19MulShiftAll64Result;

struct _M0DTP19moonbitDB10RedisValue4ZSet;

struct _M0TPB5ArrayGOsE;

struct _M0TWEOUsbE;

struct _M0DTP19moonbitDB10RedisValue4List;

struct _M0TPB5EntryGsfE;

struct _M0TPB8MutLocalGiE;

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE;

struct _M0TPB5ArrayGUsfEE;

struct _M0TPB3MapGssE;

struct _M0TUsRP19moonbitDB10RedisValueE;

struct _M0DTP19moonbitDB10RedisValue3Set;

struct _M0TPB4Show;

struct _M0TPB5ArrayGRP39moonbitDB8examples11leaderboard6PlayerE;

struct _M0TPB4IterGUsfEE;

struct _M0TWEOUsfE;

struct _M0BTPB4Show;

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3253__l711__;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB8MutLocalGORPB5EntryGsbEE;

struct _M0TPB3MapGsbE;

struct _M0TPB5ArrayGsE;

struct _M0TP39moonbitDB8examples11leaderboard6Player;

struct _M0TPB3MapGsiE;

struct _M0TPB9ArrayViewGUssEE;

struct _M0TPB9ArrayViewGUsbEE;

struct _M0TPB9ArrayViewGsE;

struct _M0TPB4IterGUsbEE;

struct _M0TPB4IterGUssEE;

struct _M0TPB5EntryGsiE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3233__l711__ {
  struct _M0TUsbE*(* code)(struct _M0TWEOUsbE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* $1;
  
};

struct _M0TPB9ArrayViewGUsfEE {
  struct _M0TUsfE** $0;
  int32_t $1;
  int32_t $2;
  
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

struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE {
  struct _M0TUsRP19moonbitDB10RedisValueE** $0;
  int32_t $1;
  int32_t $2;
  
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

struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3243__l711__ {
  struct _M0TUsRP19moonbitDB10RedisValueE*(* code)(
    struct _M0TWEOUsRP19moonbitDB10RedisValueE*
  );
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE* $1;
  
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

struct _M0TP19moonbitDB5Deque {
  struct _M0TPB5ArrayGsE* $0;
  struct _M0TPB5ArrayGsE* $1;
  
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

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3223__l711__ {
  struct _M0TUssE*(* code)(struct _M0TWEOUssE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* $1;
  
};

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0DTP19moonbitDB10RedisValue4ZSet {
  struct _M0TPB3MapGsfE* $0;
  
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

struct _M0TPB5ArrayGRP39moonbitDB8examples11leaderboard6PlayerE {
  struct _M0TP39moonbitDB8examples11leaderboard6Player** $0;
  int32_t $1;
  
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

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3253__l711__ {
  struct _M0TUsfE*(* code)(struct _M0TWEOUsfE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* $1;
  
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

struct _M0TP39moonbitDB8examples11leaderboard6Player {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
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

int32_t _M0FP39moonbitDB8examples11leaderboard19find__player__index(
  struct _M0TPB5ArrayGRP39moonbitDB8examples11leaderboard6PlayerE*,
  moonbit_string_t
);

struct _M0TP39moonbitDB8examples11leaderboard6Player* _M0MP39moonbitDB8examples11leaderboard6Player3new(
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0FP39moonbitDB8examples11leaderboard16show__opt__float(
  void*
);

moonbit_string_t _M0FP19moonbitDB16show__opt__float(void*);

moonbit_string_t _M0FP19moonbitDB14show__opt__int(int64_t);

moonbit_string_t _M0FP19moonbitDB9show__opt(moonbit_string_t);

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database7command();

moonbit_string_t _M0MP19moonbitDB8Database4ping();

moonbit_string_t _M0MP19moonbitDB8Database9randomkey(
  struct _M0TP19moonbitDB8Database*
);

int32_t _M0MP19moonbitDB8Database6dbsize(struct _M0TP19moonbitDB8Database*);

struct _M0TUsfE* _M0MP19moonbitDB8Database7zpopmax(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

float _M0MP19moonbitDB8Database7zincrby(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  float,
  moonbit_string_t
);

int64_t _M0MP19moonbitDB8Database8zrevrank(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
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

int32_t _M0MP19moonbitDB8Database6zcount(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  float,
  float
);

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database13zrangebyscore(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  float,
  float
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

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database5sdiff(
  struct _M0TP19moonbitDB8Database*,
  struct _M0TPB5ArrayGsE*
);

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database6sunion(
  struct _M0TP19moonbitDB8Database*,
  struct _M0TPB5ArrayGsE*
);

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database6sinter(
  struct _M0TP19moonbitDB8Database*,
  struct _M0TPB5ArrayGsE*
);

struct _M0TPB3MapGsbE* _M0MP19moonbitDB8Database17get__set__members(
  struct _M0TP19moonbitDB8Database*,
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

int32_t _M0MP19moonbitDB8Database4hlen(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

struct _M0TPB3MapGssE* _M0MP19moonbitDB8Database7hgetall(
  struct _M0TP19moonbitDB8Database*,
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

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database4keys(
  struct _M0TP19moonbitDB8Database*
);

int32_t _M0MP19moonbitDB8Database6exists(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

moonbit_string_t _M0MP19moonbitDB8Database3get(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
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

struct _M0TP39moonbitDB8examples11leaderboard6Player* _M0MPC15array5Array2atGRP39moonbitDB8examples11leaderboard6PlayerE(
  struct _M0TPB5ArrayGRP39moonbitDB8examples11leaderboard6PlayerE*,
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

struct _M0TPB4IterGUssEE* _M0MPB3Map5iter2GssE(struct _M0TPB3MapGssE*);

struct _M0TPB4IterGUsbEE* _M0MPB3Map5iter2GsbE(struct _M0TPB3MapGsbE*);

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*
);

struct _M0TPB4IterGUsfEE* _M0MPB3Map5iter2GsfE(struct _M0TPB3MapGsfE*);

struct _M0TPB4IterGUssEE* _M0MPB3Map4iterGssE(struct _M0TPB3MapGssE*);

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(struct _M0TPB3MapGsbE*);

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB3Map4iterGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*
);

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(struct _M0TPB3MapGsfE*);

struct _M0TUsfE* _M0MPB3Map4iterGsfEC3253l711(struct _M0TWEOUsfE*);

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB3Map4iterGsRP19moonbitDB10RedisValueEC3243l711(
  struct _M0TWEOUsRP19moonbitDB10RedisValueE*
);

struct _M0TUsbE* _M0MPB3Map4iterGsbEC3233l711(struct _M0TWEOUsbE*);

struct _M0TUssE* _M0MPB3Map4iterGssEC3223l711(struct _M0TWEOUssE*);

int32_t _M0MPB3Map6lengthGssE(struct _M0TPB3MapGssE*);

int32_t _M0MPB3Map6lengthGsbE(struct _M0TPB3MapGsbE*);

int32_t _M0MPB3Map6lengthGsfE(struct _M0TPB3MapGsfE*);

int32_t _M0MPB3Map6removeGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

int32_t _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  moonbit_string_t
);

int32_t _M0MPB3Map6removeGsfE(struct _M0TPB3MapGsfE*, moonbit_string_t);

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

int32_t _M0MPB3Map11shift__backGsiE(struct _M0TPB3MapGsiE*, int32_t);

int32_t _M0MPB3Map11shift__backGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  int32_t
);

int32_t _M0MPB3Map11shift__backGsfE(struct _M0TPB3MapGsfE*, int32_t);

int32_t _M0MPB3Map11shift__backGsbE(struct _M0TPB3MapGsbE*, int32_t);

int32_t _M0MPB3Map13remove__entryGsiE(
  struct _M0TPB3MapGsiE*,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPB3Map13remove__entryGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*
);

int32_t _M0MPB3Map13remove__entryGsfE(
  struct _M0TPB3MapGsfE*,
  struct _M0TPB5EntryGsfE*
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

int32_t _M0MPC15array5Array6lengthGRP39moonbitDB8examples11leaderboard6PlayerE(
  struct _M0TPB5ArrayGRP39moonbitDB8examples11leaderboard6PlayerE*
);

int32_t _M0MPC15array5Array6lengthGUsfEE(struct _M0TPB5ArrayGUsfEE*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder*,
  struct _M0TPC16string10StringView
);

int32_t _M0IPC14byte4BytePB7Default7default();

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

moonbit_string_t* _M0MPC15array5Array6bufferGOsE(struct _M0TPB5ArrayGOsE*);

struct _M0TP39moonbitDB8examples11leaderboard6Player** _M0MPC15array5Array6bufferGRP39moonbitDB8examples11leaderboard6PlayerE(
  struct _M0TPB5ArrayGRP39moonbitDB8examples11leaderboard6PlayerE*
);

struct _M0TUsfE** _M0MPC15array5Array6bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE*
);

struct _M0TPB4IterGUssEE* _M0MPB4Iter3newGUssEE(struct _M0TWEOUssE*, int64_t);

struct _M0TPB4IterGUsbEE* _M0MPB4Iter3newGUsbEE(struct _M0TWEOUsbE*, int64_t);

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB4Iter3newGUsRP19moonbitDB10RedisValueEE(
  struct _M0TWEOUsRP19moonbitDB10RedisValueE*,
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

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB4Iter4nextGUsRP19moonbitDB10RedisValueEE(
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE*
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

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_96 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 0, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_92 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 55, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_65 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 90, 82, 
    69, 86, 82, 65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_11 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 73, 78, 
    67, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_149 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 22686,
    21152, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_12 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 68, 69, 
    67, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_62 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 90, 82, 
    65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_73 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 68, 66, 
    83, 73, 90, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_138 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 20998, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_99 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 73, 110, 
    102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_98 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 78, 97, 78, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_45 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 83, 77, 
    69, 77, 66, 69, 82, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_23 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 68, 
    69, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_84 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 79, 
    78, 71, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_134 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 115, 99, 
    111, 114, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_123 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 54, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_176 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 102, 114, 
    105, 101, 110, 100, 115, 58, 112, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_135 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 32, 32, 
    29609, 23478, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_86 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_85 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_54 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 83, 68, 
    73, 70, 70, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_19 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 71, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_191 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    38431, 21015, 21097, 20313, 38271, 24230, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_41 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 76, 80, 
    85, 83, 72, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_37 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 76, 73, 
    78, 68, 69, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_57 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 90, 65, 
    68, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_184 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 26032, 29256,
    26412, 21457, 24067, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_64 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 90, 82, 
    69, 86, 82, 65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_89 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 52, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_179 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    20849, 21516, 22909, 21451, 73, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_156 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 8595, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_110 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 46, 108, 101, 110, 103, 116, 104, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_16 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 84, 
    84, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_107 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_155 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 8593, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_4 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 71, 69, 84, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_71 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 90, 80, 
    79, 80, 77, 73, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_97 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 25, 73, 108, 
    108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 116, 69, 
    120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_189 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 32, 32, 
    36880, 26465, 28040, 36153, 28040, 24687, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_161 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 32, 32, 
    50, 48, 48, 45, 52, 48, 48, 32, 20998, 21306, 38388, 29609, 23478, 
    25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_129 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 10, 55357,
    56522, 32, 38454, 27573, 49, 65306, 21021, 22987, 21270, 29609, 23478,
    20998, 25968, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_147 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 10, 55357,
    56580, 32, 38454, 27573, 51, 65306, 27169, 25311, 28216, 25103, 23545,
    23616, 26356, 26032, 20998, 25968, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_55 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 77, 
    79, 86, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_168 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 31186, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_150 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 32, 20998,
    65292, 26032, 24635, 20998, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_48 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 83, 73, 
    83, 77, 69, 77, 66, 69, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_193 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 32, 
    24635, 32, 75, 101, 121, 32, 25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_171 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 32, 21517, 
    58, 32, 26080, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_95 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_90 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 53, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_69 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 90, 82, 
    69, 77, 82, 65, 78, 71, 69, 66, 89, 82, 65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_28 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 86, 
    65, 76, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_22 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 71, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_10 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 83, 84, 
    82, 76, 69, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_142 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 85, 110, 
    107, 110, 111, 119, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_117 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 51, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_38 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_8 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 84, 89, 
    80, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_76 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 82, 65, 
    78, 68, 79, 77, 75, 69, 89, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_14 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 80, 69, 
    88, 80, 73, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_9 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 65, 80, 
    80, 69, 78, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_139 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 10, 55356,
    57286, 32, 38454, 27573, 50, 65306, 26597, 35810, 25490, 34892, 27036,
    32, 84, 79, 80, 32, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_128 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 101, 
    110, 114, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_47 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 67, 
    65, 82, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_46 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 82, 
    69, 77, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_33 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_152 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 30, 32, 32, 
    25490, 21517, 32, 124, 32, 29609, 23478, 73, 68, 32, 124, 32, 29609,
    23478, 21517, 31216, 32, 124, 32, 20998, 25968, 32, 124, 32, 25490,
    21517, 21464, 21270, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_119 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 52, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_111 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 40, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_67 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 90, 82, 
    69, 86, 82, 65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_182 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 10, 55357,
    56523, 32, 38454, 27573, 57, 65306, 20351, 29992, 76, 105, 115, 116,
    31649, 29702, 28040, 24687, 38431, 21015, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_177 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 102, 114, 
    105, 101, 110, 100, 115, 58, 112, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_165 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 108, 101, 
    97, 100, 101, 114, 98, 111, 97, 114, 100, 58, 100, 97, 105, 108, 
    121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_77 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 82, 69, 
    78, 65, 77, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_60 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 83, 
    67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_170 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 32, 
    31532, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_183 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 31995, 32479,
    32500, 25252, 36890, 30693, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_143 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 32, 
    32, 35, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_136 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 32, 40, 
    73, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_124 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 70, 114, 
    97, 110, 107, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_115 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_56 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[46]; 
} const moonbit_string_literal_153 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 45, 32, 32, 
    45, 45, 45, 45, 45, 124, 45, 45, 45, 45, 45, 45, 45, 45, 124, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 124, 45, 45, 45, 45, 45, 45, 
    124, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_144 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 32, 
    124, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_26 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 72, 69, 
    88, 73, 83, 84, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_1 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 40, 110, 
    105, 108, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_88 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 51, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_188 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    28040, 24687, 38431, 21015, 38271, 24230, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_174 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 41, 32, 
    45, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_116 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 66, 111, 98, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_81 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 73, 78, 
    70, 79, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_7 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 75, 69, 
    89, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_6 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 69, 88, 
    73, 83, 84, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_158 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 32, 32, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_66 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 90, 82, 
    65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_180 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 32, 32, 
    20004, 20154, 22909, 21451, 24635, 25968, 65288, 21435, 37325, 65289, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_83 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 67, 79, 
    77, 77, 65, 78, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_185 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 27963, 21160,
    22870, 21169, 21457, 25918, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_59 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 90, 67, 
    65, 82, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_126 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 71, 114, 
    97, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_163 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 44, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_15 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 84, 84, 76, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_194 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 32, 32, 
    25903, 25345, 21629, 20196, 25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_141 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 34, 32, 32, 
    45, 45, 45, 45, 45, 124, 45, 45, 45, 45, 45, 45, 45, 45, 124, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 124, 45, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_127 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 56, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_196 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 32, 32, 
    9989, 32, 25490, 34892, 27036, 31995, 32479, 31034, 20363, 36816, 
    34892, 23436, 25104, 65281, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_169 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 10, 55357,
    56401, 32, 38454, 27573, 55, 65306, 24377, 20986, 27599, 26085, 21069,
    19977, 21517, 39046, 22870, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_35 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 76, 
    69, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_103 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 30, 114, 97, 
    100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 98, 101, 
    116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_87 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_186 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 36187, 23395,
    26356, 26032, 39044, 21578, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_0 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 78, 47, 65, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_31 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 76, 80, 
    85, 83, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_25 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 76, 
    69, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_146 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 32, 32, 
    32, 32, 124, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_109 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 44, 32, 
    108, 101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_102 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 102, 97, 
    108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_106 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 98, 111, 
    117, 110, 100, 115, 32, 99, 104, 101, 99, 107, 32, 102, 97, 105, 
    108, 101, 100, 58, 32, 97, 108, 108, 111, 99, 97, 116, 101, 95, 108, 
    101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[42]; 
} const moonbit_string_literal_195 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 41, 10, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_61 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 90, 82, 
    69, 77, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_44 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 65, 
    68, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_72 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 90, 80, 
    79, 80, 77, 65, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_30 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 77, 
    71, 69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_58 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 82, 
    65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_91 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 54, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_82 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 84, 73, 
    77, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_104 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 48, 49, 
    50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 103, 104, 
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 
    118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_53 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 68, 
    73, 70, 70, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_43 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 82, 80, 
    79, 80, 76, 80, 85, 83, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_173 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 40, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_157 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 8212, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_137 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 41, 58, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_148 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_131 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 108, 101, 
    97, 100, 101, 114, 98, 111, 97, 114, 100, 58, 103, 108, 111, 98, 
    97, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_114 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 65, 108, 
    105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_68 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 90, 73, 
    78, 67, 82, 66, 89, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_122 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 69, 118, 101, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_32 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 82, 80, 
    85, 83, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_190 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 32, 
    32, 32, 45, 62, 32, 22788, 29702, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_70 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 90, 82, 
    69, 77, 82, 65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_192 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 10, 55357,
    56522, 32, 38454, 27573, 49, 48, 65306, 26381, 21153, 22120, 29366,
    24577, 32479, 35745, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_187 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 110, 111, 
    116, 105, 102, 105, 99, 97, 116, 105, 111, 110, 115, 58, 103, 108, 
    111, 98, 97, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_75 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 70, 76, 
    85, 83, 72, 65, 76, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_154 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 35, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_50 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 83, 73, 
    78, 84, 69, 82, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_49 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 83, 73, 
    78, 84, 69, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_52 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 83, 85, 
    78, 73, 79, 78, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_29 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 77, 
    83, 69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_162 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 32, 32, 
    49, 48, 48, 45, 51, 48, 48, 32, 20998, 21306, 38388, 29609, 23478, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_108 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    100, 115, 116, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_105 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 105, 110, 
    118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 111, 105, 
    110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_51 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 83, 85, 
    78, 73, 79, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_39 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 82, 
    69, 77, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_24 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 72, 71, 
    69, 84, 65, 76, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_20 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 68, 
    69, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_140 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 23, 32, 32, 
    25490, 21517, 32, 124, 32, 29609, 23478, 73, 68, 32, 124, 32, 29609,
    23478, 21517, 31216, 32, 124, 32, 20998, 25968, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_125 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 55, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_79 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 73, 
    78, 71, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_3 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 83, 69, 84, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_178 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 22, 32, 32, 
    65, 108, 105, 99, 101, 32, 21644, 32, 66, 111, 98, 32, 30340, 20849,
    21516, 22909, 21451, 25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_94 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 57, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_21 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_145 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 32, 32, 
    32, 32, 32, 124, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_27 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 75, 
    69, 89, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_120 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 68, 97, 
    118, 105, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_34 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 82, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_130 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 40, 45, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 
    45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_40 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 76, 73, 
    78, 83, 69, 82, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_13 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 69, 88, 
    80, 73, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_133 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 110, 97, 
    109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_159 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 32, 124, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_101 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 116, 114, 
    117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_160 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 10, 55357,
    56520, 32, 38454, 27573, 53, 65306, 20998, 25968, 21306, 38388, 32479,
    35745, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_18 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_172 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 21517, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_166 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 32, 32, 
    27599, 26085, 25490, 34892, 27036, 24050, 21019, 24314, 65292, 50, 
    52, 23567, 26102, 21518, 33258, 21160, 36807, 26399, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_2 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_121 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 53, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_93 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 56, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_181 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 32, 32, 
    20165, 32, 65, 108, 105, 99, 101, 32, 26377, 30340, 22909, 21451, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_151 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 10, 55356,
    57286, 32, 38454, 27573, 52, 65306, 26356, 26032, 21518, 30340, 23436,
    25972, 25490, 34892, 27036, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_132 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 112, 108, 
    97, 121, 101, 114, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_42 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 82, 80, 
    85, 83, 72, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_167 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 32, 32, 
    84, 84, 76, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_63 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 67, 
    79, 85, 78, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_118 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 67, 104, 
    97, 114, 108, 105, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_36 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 76, 82, 
    65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_78 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 82, 69, 
    78, 65, 77, 69, 78, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_5 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 68, 69, 76, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_175 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 10, 55357,
    56550, 32, 38454, 27573, 56, 65306, 20351, 29992, 83, 101, 116, 31649,
    29702, 22909, 21451, 20851, 31995, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_113 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 112, 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_112 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 32, 32, 
    28216, 25103, 25490, 34892, 27036, 31995, 32479, 32, 45, 32, 37096,
    32626, 31034, 20363, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_80 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 69, 67, 
    72, 79, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_164 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 10, 55356,
    57262, 32, 38454, 27573, 54, 65306, 27599, 26085, 25490, 34892, 27036,
    65288, 24102, 36807, 26399, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_100 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 48, 46, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_74 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 70, 76, 
    85, 83, 72, 68, 66, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_17 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 80, 69, 
    82, 83, 73, 83, 84, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_REGULAR),
    Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0)
  };

uint32_t const moonbit_layout_table_data[138] =
  {
    sizeof(struct _M0TP39moonbitDB8examples11leaderboard6Player) / 4, 
    2,
    offsetof(struct _M0TP39moonbitDB8examples11leaderboard6Player, $0) / 4,
    offsetof(struct _M0TP39moonbitDB8examples11leaderboard6Player, $1) / 4,
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
    sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3223__l711__)
    / 4, 2,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3223__l711__, $0)
    / 4,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3223__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsbEE, $0) / 4,
    sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3233__l711__)
    / 4, 2,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3233__l711__, $0)
    / 4,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3233__l711__, $1)
    / 4,
    sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE)
    / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE, $0)
    / 4,
    sizeof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3243__l711__)
    / 4, 2,
    offsetof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3243__l711__, $0)
    / 4,
    offsetof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3243__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsfEE, $0) / 4,
    sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3253__l711__)
    / 4, 2,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3253__l711__, $0)
    / 4,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3253__l711__, $1)
    / 4, sizeof(struct _M0TUsRP19moonbitDB10RedisValueE) / 4, 2,
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
    sizeof(struct _M0TPB4IterGUssEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUssEE, $0) / 4,
    sizeof(struct _M0TPB4IterGUsbEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUsbEE, $0) / 4,
    sizeof(struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE, $0) / 4,
    sizeof(struct _M0TPB4IterGUsfEE) / 4, 1,
    offsetof(struct _M0TPB4IterGUsfEE, $0) / 4,
    sizeof(struct _M0TPB13StringBuilder) / 4, 1,
    offsetof(struct _M0TPB13StringBuilder, $0) / 4,
    sizeof(struct _M0TPB5ArrayGRP39moonbitDB8examples11leaderboard6PlayerE)
    / 4, 1,
    offsetof(struct _M0TPB5ArrayGRP39moonbitDB8examples11leaderboard6PlayerE, $0)
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

int64_t _M0MPB4Iter4nextN6constrS9980GUsbEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsbEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsRP19moonbitDB10RedisValueEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsRP19moonbitDB10RedisValueEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsfEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsfEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUssEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsbEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsRP19moonbitDB10RedisValueEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsfEE = 0ll;

int32_t _M0FP39moonbitDB8examples11leaderboard19find__player__index(
  struct _M0TPB5ArrayGRP39moonbitDB8examples11leaderboard6PlayerE* _M0L7playersS2031,
  moonbit_string_t _M0L2idS2032
) {
  int32_t _M0L1iS2030;
  #line 172 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L1iS2030 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3943;
    #line 173 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
    _M0L6_2atmpS3943
    = _M0MPC15array5Array6lengthGRP39moonbitDB8examples11leaderboard6PlayerE(_M0L7playersS2031);
    if (_M0L1iS2030 < _M0L6_2atmpS3943) {
      struct _M0TP39moonbitDB8examples11leaderboard6Player* _M0L6_2atmpS3945;
      moonbit_string_t _M0L8_2afieldS3947;
      int32_t _M0L6_2acntS4377;
      moonbit_string_t _M0L2idS3944;
      int32_t _result_4477;
      int32_t _M0L6_2atmpS3946;
      #line 174 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS3945
      = _M0MPC15array5Array2atGRP39moonbitDB8examples11leaderboard6PlayerE(_M0L7playersS2031, _M0L1iS2030);
      _M0L8_2afieldS3947 = _M0L6_2atmpS3945->$0;
      _M0L6_2acntS4377
      = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3945));
      if (_M0L6_2acntS4377 > 1) {
        int32_t _M0L11_2anew__cntS4379 = _M0L6_2acntS4377 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3945), _M0L11_2anew__cntS4379);
        moonbit_incref(_M0L8_2afieldS3947);
      } else if (_M0L6_2acntS4377 == 1) {
        moonbit_string_t _M0L8_2afieldS4378 = _M0L6_2atmpS3945->$1;
        moonbit_decref(_M0L8_2afieldS4378);
        #line 174 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        moonbit_free(_M0L6_2atmpS3945);
      }
      _M0L2idS3944 = _M0L8_2afieldS3947;
      #line 174 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _result_4477
      = _M0L2idS3944 == _M0L2idS2032
        || Moonbit_array_length(_M0L2idS3944)
           == Moonbit_array_length(_M0L2idS2032)
           && 0
              == memcmp(_M0L2idS3944, _M0L2idS2032, Moonbit_array_length(_M0L2idS3944) * 2);
      moonbit_decref(_M0L2idS3944);
      if (_result_4477) {
        return _M0L1iS2030;
      }
      _M0L6_2atmpS3946 = _M0L1iS2030 + 1;
      _M0L1iS2030 = _M0L6_2atmpS3946;
      continue;
    }
    break;
  }
  return -1;
}

struct _M0TP39moonbitDB8examples11leaderboard6Player* _M0MP39moonbitDB8examples11leaderboard6Player3new(
  moonbit_string_t _M0L2idS2028,
  moonbit_string_t _M0L4nameS2029
) {
  struct _M0TP39moonbitDB8examples11leaderboard6Player* _block_4478;
  #line 13 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  moonbit_incref(_M0L2idS2028);
  moonbit_incref(_M0L4nameS2029);
  _block_4478
  = (struct _M0TP39moonbitDB8examples11leaderboard6Player*)moonbit_malloc(sizeof(struct _M0TP39moonbitDB8examples11leaderboard6Player));
  Moonbit_object_header(_block_4478)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _block_4478->$0 = _M0L2idS2028;
  _block_4478->$1 = _M0L4nameS2029;
  return _block_4478;
}

moonbit_string_t _M0FP39moonbitDB8examples11leaderboard16show__opt__float(
  void* _M0L3optS2025
) {
  float _M0L1vS2023;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2024;
  moonbit_string_t _result_4480;
  #line 1 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  switch (Moonbit_object_tag(_M0L3optS2025)) {
    case 1: {
      struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS2026 =
        (struct _M0DTPC16option6OptionGfE4Some*)_M0L3optS2025;
      float _M0L4_2avS2027 = _M0L7_2aSomeS2026->$0;
      _M0L1vS2023 = _M0L4_2avS2027;
      goto join_2022;
      break;
    }
    default: {
      return (moonbit_string_t)moonbit_string_literal_0.data;
      break;
    }
  }
  join_2022:;
  #line 3 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS2024
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 3 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS2024, _M0L1vS2023);
  #line 3 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _result_4480
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2024);
  moonbit_decref(_M0L18_2astring__builderS2024);
  return _result_4480;
}

moonbit_string_t _M0FP19moonbitDB16show__opt__float(void* _M0L3optS1958) {
  float _M0L1vS1956;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1957;
  moonbit_string_t _result_4482;
  #line 15 "/home/developer/Documents2/moonbitDB/demo.mbt"
  switch (Moonbit_object_tag(_M0L3optS1958)) {
    case 1: {
      struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS1959 =
        (struct _M0DTPC16option6OptionGfE4Some*)_M0L3optS1958;
      float _M0L4_2avS1960 = _M0L7_2aSomeS1959->$0;
      _M0L1vS1956 = _M0L4_2avS1960;
      goto join_1955;
      break;
    }
    default: {
      return (moonbit_string_t)moonbit_string_literal_1.data;
      break;
    }
  }
  join_1955:;
  #line 17 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1957
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 17 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1957, _M0L1vS1956);
  #line 17 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _result_4482
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1957);
  moonbit_decref(_M0L18_2astring__builderS1957);
  return _result_4482;
}

moonbit_string_t _M0FP19moonbitDB14show__opt__int(int64_t _M0L3optS1952) {
  int32_t _M0L1vS1950;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1951;
  moonbit_string_t _result_4484;
  #line 8 "/home/developer/Documents2/moonbitDB/demo.mbt"
  if (_M0L3optS1952 == 4294967296ll) {
    return (moonbit_string_t)moonbit_string_literal_1.data;
  } else {
    int64_t _M0L7_2aSomeS1953 = _M0L3optS1952;
    int32_t _M0L4_2avS1954 = (int32_t)_M0L7_2aSomeS1953;
    _M0L1vS1950 = _M0L4_2avS1954;
    goto join_1949;
  }
  join_1949:;
  #line 10 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1951
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 10 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1951, _M0L1vS1950);
  #line 10 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _result_4484
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1951);
  moonbit_decref(_M0L18_2astring__builderS1951);
  return _result_4484;
}

moonbit_string_t _M0FP19moonbitDB9show__opt(moonbit_string_t _M0L3optS1946) {
  moonbit_string_t _M0L1vS1944;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1945;
  moonbit_string_t _result_4486;
  #line 1 "/home/developer/Documents2/moonbitDB/demo.mbt"
  if (_M0L3optS1946 == 0) {
    return (moonbit_string_t)moonbit_string_literal_1.data;
  } else {
    moonbit_string_t _M0L7_2aSomeS1947 = _M0L3optS1946;
    moonbit_string_t _M0L4_2avS1948 = _M0L7_2aSomeS1947;
    moonbit_incref(_M0L4_2avS1948);
    _M0L1vS1944 = _M0L4_2avS1948;
    goto join_1943;
  }
  join_1943:;
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1945
  = _M0MPB13StringBuilder21StringBuilder_2einner(2);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1945, (moonbit_string_t)moonbit_string_literal_2.data);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1945, _M0L1vS1944);
  moonbit_decref(_M0L1vS1944);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1945, (moonbit_string_t)moonbit_string_literal_2.data);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _result_4486
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1945);
  moonbit_decref(_M0L18_2astring__builderS1945);
  return _result_4486;
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database7command() {
  moonbit_string_t* _M0L6_2atmpS3942;
  struct _M0TPB5ArrayGsE* _block_4487;
  #line 1594 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3942 = (moonbit_string_t*)moonbit_make_ref_array_raw(81);
  _M0L6_2atmpS3942[0] = (moonbit_string_t)moonbit_string_literal_3.data;
  _M0L6_2atmpS3942[1] = (moonbit_string_t)moonbit_string_literal_4.data;
  _M0L6_2atmpS3942[2] = (moonbit_string_t)moonbit_string_literal_5.data;
  _M0L6_2atmpS3942[3] = (moonbit_string_t)moonbit_string_literal_6.data;
  _M0L6_2atmpS3942[4] = (moonbit_string_t)moonbit_string_literal_7.data;
  _M0L6_2atmpS3942[5] = (moonbit_string_t)moonbit_string_literal_8.data;
  _M0L6_2atmpS3942[6] = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3942[7] = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3942[8] = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS3942[9] = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS3942[10] = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS3942[11] = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS3942[12] = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS3942[13] = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS3942[14] = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS3942[15] = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS3942[16] = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L6_2atmpS3942[17] = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L6_2atmpS3942[18] = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS3942[19] = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS3942[20] = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L6_2atmpS3942[21] = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L6_2atmpS3942[22] = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS3942[23] = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS3942[24] = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L6_2atmpS3942[25] = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L6_2atmpS3942[26] = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L6_2atmpS3942[27] = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS3942[28] = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS3942[29] = (moonbit_string_t)moonbit_string_literal_32.data;
  _M0L6_2atmpS3942[30] = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L6_2atmpS3942[31] = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L6_2atmpS3942[32] = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L6_2atmpS3942[33] = (moonbit_string_t)moonbit_string_literal_36.data;
  _M0L6_2atmpS3942[34] = (moonbit_string_t)moonbit_string_literal_37.data;
  _M0L6_2atmpS3942[35] = (moonbit_string_t)moonbit_string_literal_38.data;
  _M0L6_2atmpS3942[36] = (moonbit_string_t)moonbit_string_literal_39.data;
  _M0L6_2atmpS3942[37] = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L6_2atmpS3942[38] = (moonbit_string_t)moonbit_string_literal_41.data;
  _M0L6_2atmpS3942[39] = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS3942[40] = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS3942[41] = (moonbit_string_t)moonbit_string_literal_44.data;
  _M0L6_2atmpS3942[42] = (moonbit_string_t)moonbit_string_literal_45.data;
  _M0L6_2atmpS3942[43] = (moonbit_string_t)moonbit_string_literal_46.data;
  _M0L6_2atmpS3942[44] = (moonbit_string_t)moonbit_string_literal_47.data;
  _M0L6_2atmpS3942[45] = (moonbit_string_t)moonbit_string_literal_48.data;
  _M0L6_2atmpS3942[46] = (moonbit_string_t)moonbit_string_literal_49.data;
  _M0L6_2atmpS3942[47] = (moonbit_string_t)moonbit_string_literal_50.data;
  _M0L6_2atmpS3942[48] = (moonbit_string_t)moonbit_string_literal_51.data;
  _M0L6_2atmpS3942[49] = (moonbit_string_t)moonbit_string_literal_52.data;
  _M0L6_2atmpS3942[50] = (moonbit_string_t)moonbit_string_literal_53.data;
  _M0L6_2atmpS3942[51] = (moonbit_string_t)moonbit_string_literal_54.data;
  _M0L6_2atmpS3942[52] = (moonbit_string_t)moonbit_string_literal_55.data;
  _M0L6_2atmpS3942[53] = (moonbit_string_t)moonbit_string_literal_56.data;
  _M0L6_2atmpS3942[54] = (moonbit_string_t)moonbit_string_literal_57.data;
  _M0L6_2atmpS3942[55] = (moonbit_string_t)moonbit_string_literal_58.data;
  _M0L6_2atmpS3942[56] = (moonbit_string_t)moonbit_string_literal_59.data;
  _M0L6_2atmpS3942[57] = (moonbit_string_t)moonbit_string_literal_60.data;
  _M0L6_2atmpS3942[58] = (moonbit_string_t)moonbit_string_literal_61.data;
  _M0L6_2atmpS3942[59] = (moonbit_string_t)moonbit_string_literal_62.data;
  _M0L6_2atmpS3942[60] = (moonbit_string_t)moonbit_string_literal_63.data;
  _M0L6_2atmpS3942[61] = (moonbit_string_t)moonbit_string_literal_64.data;
  _M0L6_2atmpS3942[62] = (moonbit_string_t)moonbit_string_literal_65.data;
  _M0L6_2atmpS3942[63] = (moonbit_string_t)moonbit_string_literal_66.data;
  _M0L6_2atmpS3942[64] = (moonbit_string_t)moonbit_string_literal_67.data;
  _M0L6_2atmpS3942[65] = (moonbit_string_t)moonbit_string_literal_68.data;
  _M0L6_2atmpS3942[66] = (moonbit_string_t)moonbit_string_literal_69.data;
  _M0L6_2atmpS3942[67] = (moonbit_string_t)moonbit_string_literal_70.data;
  _M0L6_2atmpS3942[68] = (moonbit_string_t)moonbit_string_literal_71.data;
  _M0L6_2atmpS3942[69] = (moonbit_string_t)moonbit_string_literal_72.data;
  _M0L6_2atmpS3942[70] = (moonbit_string_t)moonbit_string_literal_73.data;
  _M0L6_2atmpS3942[71] = (moonbit_string_t)moonbit_string_literal_74.data;
  _M0L6_2atmpS3942[72] = (moonbit_string_t)moonbit_string_literal_75.data;
  _M0L6_2atmpS3942[73] = (moonbit_string_t)moonbit_string_literal_76.data;
  _M0L6_2atmpS3942[74] = (moonbit_string_t)moonbit_string_literal_77.data;
  _M0L6_2atmpS3942[75] = (moonbit_string_t)moonbit_string_literal_78.data;
  _M0L6_2atmpS3942[76] = (moonbit_string_t)moonbit_string_literal_79.data;
  _M0L6_2atmpS3942[77] = (moonbit_string_t)moonbit_string_literal_80.data;
  _M0L6_2atmpS3942[78] = (moonbit_string_t)moonbit_string_literal_81.data;
  _M0L6_2atmpS3942[79] = (moonbit_string_t)moonbit_string_literal_82.data;
  _M0L6_2atmpS3942[80] = (moonbit_string_t)moonbit_string_literal_83.data;
  _block_4487
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_block_4487)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _block_4487->$0 = _M0L6_2atmpS3942;
  _block_4487->$1 = 81;
  return _block_4487;
}

moonbit_string_t _M0MP19moonbitDB8Database4ping() {
  #line 1556 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  return (moonbit_string_t)moonbit_string_literal_84.data;
}

moonbit_string_t _M0MP19moonbitDB8Database9randomkey(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1933
) {
  moonbit_string_t* _M0L6_2atmpS3941;
  struct _M0TPB5ArrayGsE* _M0L9all__keysS1931;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3936;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L5_2aitS1932;
  int32_t _M0L6_2atmpS3937;
  #line 1503 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3941 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L9all__keysS1931
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L9all__keysS1931)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L9all__keysS1931->$0 = _M0L6_2atmpS3941;
  _M0L9all__keysS1931->$1 = 0;
  _M0L4dataS3936 = _M0L4selfS1933->$0;
  moonbit_incref(_M0L4dataS3936);
  #line 1504 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1932
  = _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(_M0L4dataS3936);
  moonbit_decref(_M0L4dataS3936);
  while (1) {
    moonbit_string_t _M0L3keyS1935;
    struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2abindS1937;
    int32_t _M0L6_2atmpS3935;
    #line 1505 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1937
    = _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(_M0L5_2aitS1932);
    if (_M0L7_2abindS1937 == 0) {
      if (_M0L7_2abindS1937) {
        moonbit_decref(_M0L7_2abindS1937);
      }
      moonbit_decref(_M0L5_2aitS1932);
    } else {
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1938 =
        _M0L7_2abindS1937;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L4_2axS1939 =
        _M0L7_2aSomeS1938;
      moonbit_string_t _M0L8_2afieldS3948 = _M0L4_2axS1939->$0;
      int32_t _M0L6_2acntS4380 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1939));
      moonbit_string_t _M0L6_2akeyS1940;
      if (_M0L6_2acntS4380 > 1) {
        int32_t _M0L11_2anew__cntS4382 = _M0L6_2acntS4380 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1939), _M0L11_2anew__cntS4382);
        moonbit_incref(_M0L8_2afieldS3948);
      } else if (_M0L6_2acntS4380 == 1) {
        void* _M0L8_2afieldS4381 = _M0L4_2axS1939->$1;
        moonbit_decref(_M0L8_2afieldS4381);
        #line 1505 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1939);
      }
      _M0L6_2akeyS1940 = _M0L8_2afieldS3948;
      _M0L3keyS1935 = _M0L6_2akeyS1940;
      goto join_1934;
    }
    goto joinlet_4489;
    join_1934:;
    #line 1506 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3935
    = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1933, _M0L3keyS1935);
    if (!_M0L6_2atmpS3935) {
      #line 1507 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L9all__keysS1931, _M0L3keyS1935);
      moonbit_decref(_M0L3keyS1935);
    } else {
      moonbit_decref(_M0L3keyS1935);
    }
    continue;
    joinlet_4489:;
    break;
  }
  #line 1510 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3937 = _M0MPC15array5Array6lengthGsE(_M0L9all__keysS1931);
  if (_M0L6_2atmpS3937 == 0) {
    moonbit_decref(_M0L9all__keysS1931);
    return 0;
  } else {
    int32_t _M0L13current__timeS3939 = _M0L4selfS1933->$2;
    int32_t _M0L6_2atmpS3940;
    int32_t _M0L3idxS1941;
    int32_t _M0L9safe__idxS1942;
    moonbit_string_t _M0L6_2atmpS3938;
    #line 1513 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3940 = _M0MPC15array5Array6lengthGsE(_M0L9all__keysS1931);
    _M0L3idxS1941 = _M0L13current__timeS3939 % _M0L6_2atmpS3940;
    if (_M0L3idxS1941 < 0) {
      _M0L9safe__idxS1942 = -_M0L3idxS1941;
    } else {
      _M0L9safe__idxS1942 = _M0L3idxS1941;
    }
    #line 1515 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3938
    = _M0MPC15array5Array2atGsE(_M0L9all__keysS1931, _M0L9safe__idxS1942);
    moonbit_decref(_M0L9all__keysS1931);
    return _M0L6_2atmpS3938;
  }
}

int32_t _M0MP19moonbitDB8Database6dbsize(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1923
) {
  struct _M0TPB8MutLocalGiE* _M0L5countS1921;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3934;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L5_2aitS1922;
  int32_t _result_4492;
  #line 1484 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5countS1921
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5countS1921)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5countS1921->$0 = 0;
  _M0L4dataS3934 = _M0L4selfS1923->$0;
  moonbit_incref(_M0L4dataS3934);
  #line 1485 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1922
  = _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(_M0L4dataS3934);
  moonbit_decref(_M0L4dataS3934);
  while (1) {
    moonbit_string_t _M0L3keyS1925;
    struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2abindS1927;
    int32_t _M0L6_2atmpS3931;
    #line 1486 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1927
    = _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(_M0L5_2aitS1922);
    if (_M0L7_2abindS1927 == 0) {
      if (_M0L7_2abindS1927) {
        moonbit_decref(_M0L7_2abindS1927);
      }
      moonbit_decref(_M0L5_2aitS1922);
    } else {
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1928 =
        _M0L7_2abindS1927;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L4_2axS1929 =
        _M0L7_2aSomeS1928;
      moonbit_string_t _M0L8_2afieldS3950 = _M0L4_2axS1929->$0;
      int32_t _M0L6_2acntS4383 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1929));
      moonbit_string_t _M0L6_2akeyS1930;
      if (_M0L6_2acntS4383 > 1) {
        int32_t _M0L11_2anew__cntS4385 = _M0L6_2acntS4383 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1929), _M0L11_2anew__cntS4385);
        moonbit_incref(_M0L8_2afieldS3950);
      } else if (_M0L6_2acntS4383 == 1) {
        void* _M0L8_2afieldS4384 = _M0L4_2axS1929->$1;
        moonbit_decref(_M0L8_2afieldS4384);
        #line 1486 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1929);
      }
      _M0L6_2akeyS1930 = _M0L8_2afieldS3950;
      _M0L3keyS1925 = _M0L6_2akeyS1930;
      goto join_1924;
    }
    goto joinlet_4491;
    join_1924:;
    #line 1487 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3931
    = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1923, _M0L3keyS1925);
    moonbit_decref(_M0L3keyS1925);
    if (!_M0L6_2atmpS3931) {
      int32_t _M0L3valS3933 = _M0L5countS1921->$0;
      int32_t _M0L6_2atmpS3932 = _M0L3valS3933 + 1;
      _M0L5countS1921->$0 = _M0L6_2atmpS3932;
    }
    continue;
    joinlet_4491:;
    break;
  }
  _result_4492 = _M0L5countS1921->$0;
  moonbit_decref(_M0L5countS1921);
  return _result_4492;
}

struct _M0TUsfE* _M0MP19moonbitDB8Database7zpopmax(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1908,
  moonbit_string_t _M0L3keyS1909
) {
  #line 1462 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1463 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1908, _M0L3keyS1909)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1912;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3930 =
      _M0L4selfS1908->$0;
    void* _M0L7_2abindS1916;
    struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1913;
    int32_t _M0L3lenS1914;
    moonbit_incref(_M0L4dataS3930);
    #line 1466 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1916
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3930, _M0L3keyS1909);
    moonbit_decref(_M0L4dataS3930);
    if (_M0L7_2abindS1916 == 0) {
      if (_M0L7_2abindS1916) {
        moonbit_decref(_M0L7_2abindS1916);
      }
      goto join_1910;
    } else {
      void* _M0L7_2aSomeS1917 = _M0L7_2abindS1916;
      void* _M0L4_2axS1918 = _M0L7_2aSomeS1917;
      switch (Moonbit_object_tag(_M0L4_2axS1918)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1919 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1918;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3954 = _M0L7_2aZSetS1919->$0;
          int32_t _M0L6_2acntS4386 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1919));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1920;
          if (_M0L6_2acntS4386 > 1) {
            int32_t _M0L11_2anew__cntS4387 = _M0L6_2acntS4386 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1919), _M0L11_2anew__cntS4387);
            moonbit_incref(_M0L8_2afieldS3954);
          } else if (_M0L6_2acntS4386 == 1) {
            #line 1466 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1919);
          }
          _M0L7_2azsetS1920 = _M0L8_2afieldS3954;
          _M0L4zsetS1912 = _M0L7_2azsetS1920;
          goto join_1911;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1918);
          goto join_1910;
          break;
        }
      }
    }
    join_1911:;
    #line 1468 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6sortedS1913
    = _M0MP19moonbitDB8Database17get__sorted__zset(_M0L4selfS1908, _M0L3keyS1909);
    #line 1469 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3lenS1914 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1913);
    if (_M0L3lenS1914 == 0) {
      moonbit_decref(_M0L6sortedS1913);
      moonbit_decref(_M0L4zsetS1912);
      return 0;
    } else {
      int32_t _M0L6_2atmpS3929 = _M0L3lenS1914 - 1;
      struct _M0TUsfE* _M0L9max__itemS1915;
      moonbit_string_t _M0L6_2atmpS3926;
      struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3927;
      void* _M0L4ZSetS3928;
      #line 1473 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L9max__itemS1915
      = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1913, _M0L6_2atmpS3929);
      moonbit_decref(_M0L6sortedS1913);
      _M0L6_2atmpS3926 = _M0L9max__itemS1915->$0;
      moonbit_incref(_M0L6_2atmpS3926);
      #line 1474 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map6removeGsfE(_M0L4zsetS1912, _M0L6_2atmpS3926);
      moonbit_decref(_M0L6_2atmpS3926);
      _M0L4dataS3927 = _M0L4selfS1908->$0;
      _M0L4ZSetS3928
      = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4ZSet));
      Moonbit_object_header(_M0L4ZSetS3928)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 4);
      ((struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4ZSetS3928)->$0
      = _M0L4zsetS1912;
      moonbit_incref(_M0L4dataS3927);
      #line 1475 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3927, _M0L3keyS1909, _M0L4ZSetS3928);
      moonbit_decref(_M0L4dataS3927);
      moonbit_decref(_M0L4ZSetS3928);
      return _M0L9max__itemS1915;
    }
    join_1910:;
    return 0;
  }
}

float _M0MP19moonbitDB8Database7zincrby(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1887,
  moonbit_string_t _M0L3keyS1888,
  float _M0L9incrementS1907,
  moonbit_string_t _M0L11member__valS1903
) {
  int32_t _M0L6_2atmpS3920;
  struct _M0TPB3MapGsfE* _M0L4zsetS1889;
  struct _M0TPB3MapGsfE* _M0L1zS1893;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3925;
  void* _M0L7_2abindS1894;
  struct _M0TUsfE** _M0L7_2abindS1891;
  struct _M0TUsfE** _M0L6_2atmpS3924;
  struct _M0TPB9ArrayViewGUsfEE _M0L6_2atmpS3923;
  float _M0L1sS1901;
  float _M0L14current__scoreS1899;
  void* _M0L7_2abindS1902;
  float _M0L10new__scoreS1906;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3921;
  void* _M0L4ZSetS3922;
  #line 1379 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1380 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3920
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1887, _M0L3keyS1888);
  _M0L4dataS3925 = _M0L4selfS1887->$0;
  moonbit_incref(_M0L4dataS3925);
  #line 1381 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1894
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3925, _M0L3keyS1888);
  moonbit_decref(_M0L4dataS3925);
  if (_M0L7_2abindS1894 == 0) {
    if (_M0L7_2abindS1894) {
      moonbit_decref(_M0L7_2abindS1894);
    }
    goto join_1890;
  } else {
    void* _M0L7_2aSomeS1895 = _M0L7_2abindS1894;
    void* _M0L4_2axS1896 = _M0L7_2aSomeS1895;
    switch (Moonbit_object_tag(_M0L4_2axS1896)) {
      case 4: {
        struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1897 =
          (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1896;
        struct _M0TPB3MapGsfE* _M0L8_2afieldS3957 = _M0L7_2aZSetS1897->$0;
        int32_t _M0L6_2acntS4388 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1897));
        struct _M0TPB3MapGsfE* _M0L4_2azS1898;
        if (_M0L6_2acntS4388 > 1) {
          int32_t _M0L11_2anew__cntS4389 = _M0L6_2acntS4388 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1897), _M0L11_2anew__cntS4389);
          moonbit_incref(_M0L8_2afieldS3957);
        } else if (_M0L6_2acntS4388 == 1) {
          #line 1381 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aZSetS1897);
        }
        _M0L4_2azS1898 = _M0L8_2afieldS3957;
        _M0L1zS1893 = _M0L4_2azS1898;
        goto join_1892;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1896);
        goto join_1890;
        break;
      }
    }
  }
  goto joinlet_4496;
  join_1892:;
  _M0L4zsetS1889 = _M0L1zS1893;
  joinlet_4496:;
  goto joinlet_4495;
  join_1890:;
  _M0L7_2abindS1891 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3924 = _M0L7_2abindS1891;
  _M0L6_2atmpS3923
  = (struct _M0TPB9ArrayViewGUsfEE){
    .$0 = _M0L6_2atmpS3924, .$1 = 0, .$2 = 0
  };
  #line 1383 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4zsetS1889 = _M0MPB3Map3MapGsfE(_M0L6_2atmpS3923, 10ll);
  moonbit_decref(_M0L6_2atmpS3923.$0);
  joinlet_4495:;
  #line 1385 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1902
  = _M0MPB3Map3getGsfE(_M0L4zsetS1889, _M0L11member__valS1903);
  switch (Moonbit_object_tag(_M0L7_2abindS1902)) {
    case 1: {
      struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS1904 =
        (struct _M0DTPC16option6OptionGfE4Some*)_M0L7_2abindS1902;
      float _M0L4_2asS1905 = _M0L7_2aSomeS1904->$0;
      moonbit_decref(_M0L7_2aSomeS1904);
      _M0L1sS1901 = _M0L4_2asS1905;
      goto join_1900;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2abindS1902);
      _M0L14current__scoreS1899 = 0x0p+0f;
      break;
    }
  }
  goto joinlet_4497;
  join_1900:;
  _M0L14current__scoreS1899 = _M0L1sS1901;
  joinlet_4497:;
  _M0L10new__scoreS1906 = _M0L14current__scoreS1899 + _M0L9incrementS1907;
  #line 1390 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsfE(_M0L4zsetS1889, _M0L11member__valS1903, _M0L10new__scoreS1906);
  _M0L4dataS3921 = _M0L4selfS1887->$0;
  _M0L4ZSetS3922
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4ZSet));
  Moonbit_object_header(_M0L4ZSetS3922)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 4);
  ((struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4ZSetS3922)->$0
  = _M0L4zsetS1889;
  moonbit_incref(_M0L4dataS3921);
  #line 1391 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3921, _M0L3keyS1888, _M0L4ZSetS3922);
  moonbit_decref(_M0L4dataS3921);
  moonbit_decref(_M0L4ZSetS3922);
  return _M0L10new__scoreS1906;
}

int64_t _M0MP19moonbitDB8Database8zrevrank(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1871,
  moonbit_string_t _M0L3keyS1872,
  moonbit_string_t _M0L11member__valS1876
) {
  #line 1353 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1354 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1871, _M0L3keyS1872)
  ) {
    return 4294967296ll;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1875;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3919 =
      _M0L4selfS1871->$0;
    void* _M0L7_2abindS1882;
    int32_t _M0L6_2atmpS3912;
    moonbit_incref(_M0L4dataS3919);
    #line 1357 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1882
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3919, _M0L3keyS1872);
    moonbit_decref(_M0L4dataS3919);
    if (_M0L7_2abindS1882 == 0) {
      if (_M0L7_2abindS1882) {
        moonbit_decref(_M0L7_2abindS1882);
      }
      goto join_1873;
    } else {
      void* _M0L7_2aSomeS1883 = _M0L7_2abindS1882;
      void* _M0L4_2axS1884 = _M0L7_2aSomeS1883;
      switch (Moonbit_object_tag(_M0L4_2axS1884)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1885 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1884;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3960 = _M0L7_2aZSetS1885->$0;
          int32_t _M0L6_2acntS4392 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1885));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1886;
          if (_M0L6_2acntS4392 > 1) {
            int32_t _M0L11_2anew__cntS4393 = _M0L6_2acntS4392 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1885), _M0L11_2anew__cntS4393);
            moonbit_incref(_M0L8_2afieldS3960);
          } else if (_M0L6_2acntS4392 == 1) {
            #line 1357 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1885);
          }
          _M0L7_2azsetS1886 = _M0L8_2afieldS3960;
          _M0L4zsetS1875 = _M0L7_2azsetS1886;
          goto join_1874;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1884);
          goto join_1873;
          break;
        }
      }
    }
    join_1874:;
    #line 1359 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3912
    = _M0MPB3Map8containsGsfE(_M0L4zsetS1875, _M0L11member__valS1876);
    moonbit_decref(_M0L4zsetS1875);
    if (!_M0L6_2atmpS3912) {
      return 4294967296ll;
    } else {
      struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1877;
      int32_t _M0L3lenS1878;
      struct _M0TPB8MutLocalGiE* _M0L4rankS1879;
      int32_t _M0L1iS1880;
      int32_t _M0L3valS3918;
      #line 1362 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6sortedS1877
      = _M0MP19moonbitDB8Database17get__sorted__zset(_M0L4selfS1871, _M0L3keyS1872);
      #line 1363 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L3lenS1878 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1877);
      _M0L4rankS1879
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L4rankS1879)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L4rankS1879->$0 = 0;
      _M0L1iS1880 = 0;
      while (1) {
        if (_M0L1iS1880 < _M0L3lenS1878) {
          struct _M0TUsfE* _M0L6_2atmpS3914;
          moonbit_string_t _M0L8_2afieldS3959;
          int32_t _M0L6_2acntS4390;
          moonbit_string_t _M0L6_2atmpS3913;
          int32_t _result_4501;
          int32_t _M0L6_2atmpS3917;
          #line 1366 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0L6_2atmpS3914
          = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1877, _M0L1iS1880);
          _M0L8_2afieldS3959 = _M0L6_2atmpS3914->$0;
          _M0L6_2acntS4390
          = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3914));
          if (_M0L6_2acntS4390 > 1) {
            int32_t _M0L11_2anew__cntS4391 = _M0L6_2acntS4390 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3914), _M0L11_2anew__cntS4391);
            moonbit_incref(_M0L8_2afieldS3959);
          } else if (_M0L6_2acntS4390 == 1) {
            #line 1366 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2atmpS3914);
          }
          _M0L6_2atmpS3913 = _M0L8_2afieldS3959;
          #line 1366 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _result_4501
          = _M0L6_2atmpS3913 == _M0L11member__valS1876
            || Moonbit_array_length(_M0L6_2atmpS3913)
               == Moonbit_array_length(_M0L11member__valS1876)
               && 0
                  == memcmp(_M0L6_2atmpS3913, _M0L11member__valS1876, Moonbit_array_length(_M0L6_2atmpS3913) * 2);
          moonbit_decref(_M0L6_2atmpS3913);
          if (_result_4501) {
            int32_t _M0L6_2atmpS3916;
            int32_t _M0L6_2atmpS3915;
            moonbit_decref(_M0L6sortedS1877);
            _M0L6_2atmpS3916 = _M0L3lenS1878 - 1;
            _M0L6_2atmpS3915 = _M0L6_2atmpS3916 - _M0L1iS1880;
            _M0L4rankS1879->$0 = _M0L6_2atmpS3915;
            break;
          }
          _M0L6_2atmpS3917 = _M0L1iS1880 + 1;
          _M0L1iS1880 = _M0L6_2atmpS3917;
          continue;
        } else {
          moonbit_decref(_M0L6sortedS1877);
        }
        break;
      }
      _M0L3valS3918 = _M0L4rankS1879->$0;
      moonbit_decref(_M0L4rankS1879);
      return (int64_t)_M0L3valS3918;
    }
    join_1873:;
    return 4294967296ll;
  }
}

int64_t _M0MP19moonbitDB8Database5zrank(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1856,
  moonbit_string_t _M0L3keyS1857,
  moonbit_string_t _M0L11member__valS1861
) {
  #line 1328 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1329 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1856, _M0L3keyS1857)
  ) {
    return 4294967296ll;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1860;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3911 =
      _M0L4selfS1856->$0;
    void* _M0L7_2abindS1866;
    int32_t _M0L6_2atmpS3905;
    moonbit_incref(_M0L4dataS3911);
    #line 1332 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1866
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3911, _M0L3keyS1857);
    moonbit_decref(_M0L4dataS3911);
    if (_M0L7_2abindS1866 == 0) {
      if (_M0L7_2abindS1866) {
        moonbit_decref(_M0L7_2abindS1866);
      }
      goto join_1858;
    } else {
      void* _M0L7_2aSomeS1867 = _M0L7_2abindS1866;
      void* _M0L4_2axS1868 = _M0L7_2aSomeS1867;
      switch (Moonbit_object_tag(_M0L4_2axS1868)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1869 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1868;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3963 = _M0L7_2aZSetS1869->$0;
          int32_t _M0L6_2acntS4396 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1869));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1870;
          if (_M0L6_2acntS4396 > 1) {
            int32_t _M0L11_2anew__cntS4397 = _M0L6_2acntS4396 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1869), _M0L11_2anew__cntS4397);
            moonbit_incref(_M0L8_2afieldS3963);
          } else if (_M0L6_2acntS4396 == 1) {
            #line 1332 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1869);
          }
          _M0L7_2azsetS1870 = _M0L8_2afieldS3963;
          _M0L4zsetS1860 = _M0L7_2azsetS1870;
          goto join_1859;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1868);
          goto join_1858;
          break;
        }
      }
    }
    join_1859:;
    #line 1334 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3905
    = _M0MPB3Map8containsGsfE(_M0L4zsetS1860, _M0L11member__valS1861);
    moonbit_decref(_M0L4zsetS1860);
    if (!_M0L6_2atmpS3905) {
      return 4294967296ll;
    } else {
      struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1862;
      struct _M0TPB8MutLocalGiE* _M0L4rankS1863;
      int32_t _M0L1iS1864;
      int32_t _M0L3valS3910;
      #line 1337 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6sortedS1862
      = _M0MP19moonbitDB8Database17get__sorted__zset(_M0L4selfS1856, _M0L3keyS1857);
      _M0L4rankS1863
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L4rankS1863)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L4rankS1863->$0 = 0;
      _M0L1iS1864 = 0;
      while (1) {
        int32_t _M0L6_2atmpS3906;
        #line 1339 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3906 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1862);
        if (_M0L1iS1864 < _M0L6_2atmpS3906) {
          struct _M0TUsfE* _M0L6_2atmpS3908;
          moonbit_string_t _M0L8_2afieldS3962;
          int32_t _M0L6_2acntS4394;
          moonbit_string_t _M0L6_2atmpS3907;
          int32_t _result_4505;
          int32_t _M0L6_2atmpS3909;
          #line 1340 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0L6_2atmpS3908
          = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1862, _M0L1iS1864);
          _M0L8_2afieldS3962 = _M0L6_2atmpS3908->$0;
          _M0L6_2acntS4394
          = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3908));
          if (_M0L6_2acntS4394 > 1) {
            int32_t _M0L11_2anew__cntS4395 = _M0L6_2acntS4394 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3908), _M0L11_2anew__cntS4395);
            moonbit_incref(_M0L8_2afieldS3962);
          } else if (_M0L6_2acntS4394 == 1) {
            #line 1340 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2atmpS3908);
          }
          _M0L6_2atmpS3907 = _M0L8_2afieldS3962;
          #line 1340 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _result_4505
          = _M0L6_2atmpS3907 == _M0L11member__valS1861
            || Moonbit_array_length(_M0L6_2atmpS3907)
               == Moonbit_array_length(_M0L11member__valS1861)
               && 0
                  == memcmp(_M0L6_2atmpS3907, _M0L11member__valS1861, Moonbit_array_length(_M0L6_2atmpS3907) * 2);
          moonbit_decref(_M0L6_2atmpS3907);
          if (_result_4505) {
            moonbit_decref(_M0L6sortedS1862);
            _M0L4rankS1863->$0 = _M0L1iS1864;
            break;
          }
          _M0L6_2atmpS3909 = _M0L1iS1864 + 1;
          _M0L1iS1864 = _M0L6_2atmpS3909;
          continue;
        } else {
          moonbit_decref(_M0L6sortedS1862);
        }
        break;
      }
      _M0L3valS3910 = _M0L4rankS1863->$0;
      moonbit_decref(_M0L4rankS1863);
      return (int64_t)_M0L3valS3910;
    }
    join_1858:;
    return 4294967296ll;
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database9zrevrange(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1845,
  moonbit_string_t _M0L3keyS1846,
  int32_t _M0L5startS1850,
  int32_t _M0L3endS1852
) {
  struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1844;
  int32_t _M0L3lenS1847;
  moonbit_string_t* _M0L6_2atmpS3904;
  struct _M0TPB5ArrayGsE* _M0L6resultS1848;
  int32_t _M0L10start__idxS1849;
  int32_t _M0L8end__idxS1851;
  int32_t _M0L1iS1853;
  #line 1302 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1303 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6sortedS1844
  = _M0MP19moonbitDB8Database17get__sorted__zset(_M0L4selfS1845, _M0L3keyS1846);
  #line 1304 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L3lenS1847 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1844);
  _M0L6_2atmpS3904 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1848
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1848)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6resultS1848->$0 = _M0L6_2atmpS3904;
  _M0L6resultS1848->$1 = 0;
  if (_M0L5startS1850 < 0) {
    _M0L10start__idxS1849 = _M0L3lenS1847 + _M0L5startS1850;
  } else {
    _M0L10start__idxS1849 = _M0L5startS1850;
  }
  if (_M0L3endS1852 < 0) {
    _M0L8end__idxS1851 = _M0L3lenS1847 + _M0L3endS1852;
  } else {
    _M0L8end__idxS1851 = _M0L3endS1852;
  }
  _M0L1iS1853 = _M0L10start__idxS1849;
  while (1) {
    int32_t _if__result_4507;
    if (_M0L1iS1853 <= _M0L8end__idxS1851) {
      _if__result_4507 = _M0L1iS1853 < _M0L3lenS1847;
    } else {
      _if__result_4507 = 0;
    }
    if (_if__result_4507) {
      int32_t _M0L6_2atmpS3902 = _M0L3lenS1847 - 1;
      int32_t _M0L8rev__idxS1854 = _M0L6_2atmpS3902 - _M0L1iS1853;
      int32_t _M0L6_2atmpS3903;
      if (_M0L8rev__idxS1854 >= 0) {
        struct _M0TUsfE* _M0L6_2atmpS3901;
        moonbit_string_t _M0L8_2afieldS3965;
        int32_t _M0L6_2acntS4398;
        moonbit_string_t _M0L6_2atmpS3900;
        #line 1311 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3901
        = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1844, _M0L8rev__idxS1854);
        _M0L8_2afieldS3965 = _M0L6_2atmpS3901->$0;
        _M0L6_2acntS4398
        = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3901));
        if (_M0L6_2acntS4398 > 1) {
          int32_t _M0L11_2anew__cntS4399 = _M0L6_2acntS4398 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3901), _M0L11_2anew__cntS4399);
          moonbit_incref(_M0L8_2afieldS3965);
        } else if (_M0L6_2acntS4398 == 1) {
          #line 1311 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L6_2atmpS3901);
        }
        _M0L6_2atmpS3900 = _M0L8_2afieldS3965;
        #line 1311 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1848, _M0L6_2atmpS3900);
        moonbit_decref(_M0L6_2atmpS3900);
      }
      _M0L6_2atmpS3903 = _M0L1iS1853 + 1;
      _M0L1iS1853 = _M0L6_2atmpS3903;
      continue;
    } else {
      moonbit_decref(_M0L6sortedS1844);
    }
    break;
  }
  return _M0L6resultS1848;
}

int32_t _M0MP19moonbitDB8Database6zcount(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1835,
  moonbit_string_t _M0L3keyS1836,
  float _M0L10min__scoreS1841,
  float _M0L10max__scoreS1842
) {
  struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1834;
  struct _M0TPB8MutLocalGiE* _M0L5countS1837;
  int32_t _M0L7_2abindS1838;
  int32_t _M0L2__S1839;
  int32_t _result_4510;
  #line 1291 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1292 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6sortedS1834
  = _M0MP19moonbitDB8Database17get__sorted__zset(_M0L4selfS1835, _M0L3keyS1836);
  _M0L5countS1837
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5countS1837)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5countS1837->$0 = 0;
  _M0L7_2abindS1838 = _M0L6sortedS1834->$1;
  _M0L2__S1839 = 0;
  while (1) {
    if (_M0L2__S1839 < _M0L7_2abindS1838) {
      struct _M0TUsfE** _M0L3bufS3899 = _M0L6sortedS1834->$0;
      struct _M0TUsfE* _M0L4itemS1840 =
        (struct _M0TUsfE*)_M0L3bufS3899[_M0L2__S1839];
      float _M0L6_2atmpS3895 = _M0L4itemS1840->$1;
      int32_t _if__result_4509;
      int32_t _M0L6_2atmpS3898;
      if (_M0L6_2atmpS3895 >= _M0L10min__scoreS1841) {
        float _M0L6_2atmpS3894 = _M0L4itemS1840->$1;
        _if__result_4509 = _M0L6_2atmpS3894 <= _M0L10max__scoreS1842;
      } else {
        _if__result_4509 = 0;
      }
      if (_if__result_4509) {
        int32_t _M0L3valS3897 = _M0L5countS1837->$0;
        int32_t _M0L6_2atmpS3896 = _M0L3valS3897 + 1;
        _M0L5countS1837->$0 = _M0L6_2atmpS3896;
      }
      _M0L6_2atmpS3898 = _M0L2__S1839 + 1;
      _M0L2__S1839 = _M0L6_2atmpS3898;
      continue;
    } else {
      moonbit_decref(_M0L6sortedS1834);
    }
    break;
  }
  _result_4510 = _M0L5countS1837->$0;
  moonbit_decref(_M0L5countS1837);
  return _result_4510;
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database13zrangebyscore(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1825,
  moonbit_string_t _M0L3keyS1826,
  float _M0L10min__scoreS1831,
  float _M0L10max__scoreS1832
) {
  struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1824;
  moonbit_string_t* _M0L6_2atmpS3893;
  struct _M0TPB5ArrayGsE* _M0L6resultS1827;
  int32_t _M0L7_2abindS1828;
  int32_t _M0L2__S1829;
  #line 1280 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1281 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6sortedS1824
  = _M0MP19moonbitDB8Database17get__sorted__zset(_M0L4selfS1825, _M0L3keyS1826);
  _M0L6_2atmpS3893 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1827
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1827)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6resultS1827->$0 = _M0L6_2atmpS3893;
  _M0L6resultS1827->$1 = 0;
  _M0L7_2abindS1828 = _M0L6sortedS1824->$1;
  _M0L2__S1829 = 0;
  while (1) {
    if (_M0L2__S1829 < _M0L7_2abindS1828) {
      struct _M0TUsfE** _M0L3bufS3892 = _M0L6sortedS1824->$0;
      struct _M0TUsfE* _M0L4itemS1830 =
        (struct _M0TUsfE*)_M0L3bufS3892[_M0L2__S1829];
      float _M0L6_2atmpS3889 = _M0L4itemS1830->$1;
      int32_t _if__result_4512;
      int32_t _M0L6_2atmpS3891;
      if (_M0L6_2atmpS3889 >= _M0L10min__scoreS1831) {
        float _M0L6_2atmpS3888 = _M0L4itemS1830->$1;
        _if__result_4512 = _M0L6_2atmpS3888 <= _M0L10max__scoreS1832;
      } else {
        _if__result_4512 = 0;
      }
      if (_if__result_4512) {
        moonbit_string_t _M0L6_2atmpS3890 = _M0L4itemS1830->$0;
        moonbit_incref(_M0L6_2atmpS3890);
        #line 1285 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1827, _M0L6_2atmpS3890);
        moonbit_decref(_M0L6_2atmpS3890);
      }
      _M0L6_2atmpS3891 = _M0L2__S1829 + 1;
      _M0L2__S1829 = _M0L6_2atmpS3891;
      continue;
    } else {
      moonbit_decref(_M0L6sortedS1824);
    }
    break;
  }
  return _M0L6resultS1827;
}

struct _M0TPB5ArrayGUsfEE* _M0MP19moonbitDB8Database17get__sorted__zset(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1803,
  moonbit_string_t _M0L3keyS1804
) {
  #line 1263 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1264 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1803, _M0L3keyS1804)
  ) {
    struct _M0TUsfE** _M0L6_2atmpS3883 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _block_4513 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_4513)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
    _block_4513->$0 = _M0L6_2atmpS3883;
    _block_4513->$1 = 0;
    return _block_4513;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1807;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3887 =
      _M0L4selfS1803->$0;
    void* _M0L7_2abindS1819;
    struct _M0TUsfE** _M0L6_2atmpS3886;
    struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1808;
    struct _M0TPB4IterGUsfEE* _M0L5_2aitS1809;
    struct _M0TPB5ArrayGUsfEE* _result_4518;
    struct _M0TUsfE** _M0L6_2atmpS3884;
    struct _M0TPB5ArrayGUsfEE* _block_4519;
    moonbit_incref(_M0L4dataS3887);
    #line 1267 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1819
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3887, _M0L3keyS1804);
    moonbit_decref(_M0L4dataS3887);
    if (_M0L7_2abindS1819 == 0) {
      if (_M0L7_2abindS1819) {
        moonbit_decref(_M0L7_2abindS1819);
      }
      goto join_1805;
    } else {
      void* _M0L7_2aSomeS1820 = _M0L7_2abindS1819;
      void* _M0L4_2axS1821 = _M0L7_2aSomeS1820;
      switch (Moonbit_object_tag(_M0L4_2axS1821)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1822 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1821;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3972 = _M0L7_2aZSetS1822->$0;
          int32_t _M0L6_2acntS4402 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1822));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1823;
          if (_M0L6_2acntS4402 > 1) {
            int32_t _M0L11_2anew__cntS4403 = _M0L6_2acntS4402 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1822), _M0L11_2anew__cntS4403);
            moonbit_incref(_M0L8_2afieldS3972);
          } else if (_M0L6_2acntS4402 == 1) {
            #line 1267 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1822);
          }
          _M0L7_2azsetS1823 = _M0L8_2afieldS3972;
          _M0L4zsetS1807 = _M0L7_2azsetS1823;
          goto join_1806;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1821);
          goto join_1805;
          break;
        }
      }
    }
    join_1806:;
    _M0L6_2atmpS3886 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5itemsS1808
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5itemsS1808)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
    _M0L5itemsS1808->$0 = _M0L6_2atmpS3886;
    _M0L5itemsS1808->$1 = 0;
    #line 1269 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L5_2aitS1809 = _M0MPB3Map5iter2GsfE(_M0L4zsetS1807);
    moonbit_decref(_M0L4zsetS1807);
    while (1) {
      moonbit_string_t _M0L1mS1811;
      float _M0L1sS1812;
      struct _M0TUsfE* _M0L7_2abindS1814;
      struct _M0TUsfE* _M0L8_2atupleS3885;
      #line 1270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L7_2abindS1814 = _M0MPB5Iter24nextGsfE(_M0L5_2aitS1809);
      if (_M0L7_2abindS1814 == 0) {
        if (_M0L7_2abindS1814) {
          moonbit_decref(_M0L7_2abindS1814);
        }
        moonbit_decref(_M0L5_2aitS1809);
      } else {
        struct _M0TUsfE* _M0L7_2aSomeS1815 = _M0L7_2abindS1814;
        struct _M0TUsfE* _M0L4_2axS1816 = _M0L7_2aSomeS1815;
        moonbit_string_t _M0L4_2amS1817 = _M0L4_2axS1816->$0;
        float _M0L4_2asS1818 = _M0L4_2axS1816->$1;
        int32_t _M0L6_2acntS4400 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1816));
        if (_M0L6_2acntS4400 > 1) {
          int32_t _M0L11_2anew__cntS4401 = _M0L6_2acntS4400 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1816), _M0L11_2anew__cntS4401);
          moonbit_incref(_M0L4_2amS1817);
        } else if (_M0L6_2acntS4400 == 1) {
          #line 1270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L4_2axS1816);
        }
        _M0L1mS1811 = _M0L4_2amS1817;
        _M0L1sS1812 = _M0L4_2asS1818;
        goto join_1810;
      }
      goto joinlet_4517;
      join_1810:;
      _M0L8_2atupleS3885
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS3885)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 13, 0);
      _M0L8_2atupleS3885->$0 = _M0L1mS1811;
      _M0L8_2atupleS3885->$1 = _M0L1sS1812;
      #line 1271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L5itemsS1808, _M0L8_2atupleS3885);
      moonbit_decref(_M0L8_2atupleS3885);
      continue;
      joinlet_4517:;
      break;
    }
    #line 1273 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4518 = _M0FP19moonbitDB15sort__by__score(_M0L5itemsS1808);
    moonbit_decref(_M0L5itemsS1808);
    return _result_4518;
    join_1805:;
    _M0L6_2atmpS3884 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _block_4519
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_4519)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
    _block_4519->$0 = _M0L6_2atmpS3884;
    _block_4519->$1 = 0;
    return _block_4519;
  }
}

void* _M0MP19moonbitDB8Database6zscore(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1792,
  moonbit_string_t _M0L3keyS1793,
  moonbit_string_t _M0L11member__valS1797
) {
  #line 1234 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1235 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1792, _M0L3keyS1793)
  ) {
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1796;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3882 =
      _M0L4selfS1792->$0;
    void* _M0L7_2abindS1798;
    void* _result_4522;
    moonbit_incref(_M0L4dataS3882);
    #line 1238 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1798
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3882, _M0L3keyS1793);
    moonbit_decref(_M0L4dataS3882);
    if (_M0L7_2abindS1798 == 0) {
      if (_M0L7_2abindS1798) {
        moonbit_decref(_M0L7_2abindS1798);
      }
      goto join_1794;
    } else {
      void* _M0L7_2aSomeS1799 = _M0L7_2abindS1798;
      void* _M0L4_2axS1800 = _M0L7_2aSomeS1799;
      switch (Moonbit_object_tag(_M0L4_2axS1800)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1801 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1800;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3974 = _M0L7_2aZSetS1801->$0;
          int32_t _M0L6_2acntS4404 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1801));
          struct _M0TPB3MapGsfE* _M0L4_2azS1802;
          if (_M0L6_2acntS4404 > 1) {
            int32_t _M0L11_2anew__cntS4405 = _M0L6_2acntS4404 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1801), _M0L11_2anew__cntS4405);
            moonbit_incref(_M0L8_2afieldS3974);
          } else if (_M0L6_2acntS4404 == 1) {
            #line 1238 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1801);
          }
          _M0L4_2azS1802 = _M0L8_2afieldS3974;
          _M0L1zS1796 = _M0L4_2azS1802;
          goto join_1795;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1800);
          goto join_1794;
          break;
        }
      }
    }
    join_1795:;
    #line 1239 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4522 = _M0MPB3Map3getGsfE(_M0L1zS1796, _M0L11member__valS1797);
    moonbit_decref(_M0L1zS1796);
    return _result_4522;
    join_1794:;
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
}

int32_t _M0MP19moonbitDB8Database5zcard(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1783,
  moonbit_string_t _M0L3keyS1784
) {
  #line 1223 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1224 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1783, _M0L3keyS1784)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1786;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3881 =
      _M0L4selfS1783->$0;
    void* _M0L7_2abindS1787;
    int32_t _result_4524;
    moonbit_incref(_M0L4dataS3881);
    #line 1227 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1787
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3881, _M0L3keyS1784);
    moonbit_decref(_M0L4dataS3881);
    if (_M0L7_2abindS1787 == 0) {
      if (_M0L7_2abindS1787) {
        moonbit_decref(_M0L7_2abindS1787);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1788 = _M0L7_2abindS1787;
      void* _M0L4_2axS1789 = _M0L7_2aSomeS1788;
      switch (Moonbit_object_tag(_M0L4_2axS1789)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1790 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1789;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3976 = _M0L7_2aZSetS1790->$0;
          int32_t _M0L6_2acntS4406 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1790));
          struct _M0TPB3MapGsfE* _M0L4_2azS1791;
          if (_M0L6_2acntS4406 > 1) {
            int32_t _M0L11_2anew__cntS4407 = _M0L6_2acntS4406 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1790), _M0L11_2anew__cntS4407);
            moonbit_incref(_M0L8_2afieldS3976);
          } else if (_M0L6_2acntS4406 == 1) {
            #line 1227 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1790);
          }
          _M0L4_2azS1791 = _M0L8_2afieldS3976;
          _M0L1zS1786 = _M0L4_2azS1791;
          goto join_1785;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1789);
          return 0;
          break;
        }
      }
    }
    join_1785:;
    #line 1228 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4524 = _M0MPB3Map6lengthGsfE(_M0L1zS1786);
    moonbit_decref(_M0L1zS1786);
    return _result_4524;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP19moonbitDB15sort__by__score(
  struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1782
) {
  #line 1177 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1178 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  return _M0FP19moonbitDB11merge__sort(_M0L5itemsS1782);
}

struct _M0TPB5ArrayGUsfEE* _M0FP19moonbitDB11merge__sort(
  struct _M0TPB5ArrayGUsfEE* _M0L3arrS1774
) {
  int32_t _M0L3lenS1773;
  #line 1181 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1182 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L3lenS1773 = _M0MPC15array5Array6lengthGUsfEE(_M0L3arrS1774);
  if (_M0L3lenS1773 <= 1) {
    moonbit_incref(_M0L3arrS1774);
    return _M0L3arrS1774;
  } else {
    int32_t _M0L3midS1775 = _M0L3lenS1773 / 2;
    struct _M0TUsfE** _M0L6_2atmpS3880 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _M0L4leftS1776 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    struct _M0TUsfE** _M0L6_2atmpS3879;
    struct _M0TPB5ArrayGUsfEE* _M0L5rightS1777;
    int32_t _M0L1iS1778;
    int32_t _M0L1iS1780;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS3877;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS3878;
    struct _M0TPB5ArrayGUsfEE* _result_4527;
    Moonbit_object_header(_M0L4leftS1776)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
    _M0L4leftS1776->$0 = _M0L6_2atmpS3880;
    _M0L4leftS1776->$1 = 0;
    _M0L6_2atmpS3879 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5rightS1777
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5rightS1777)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
    _M0L5rightS1777->$0 = _M0L6_2atmpS3879;
    _M0L5rightS1777->$1 = 0;
    _M0L1iS1778 = 0;
    while (1) {
      if (_M0L1iS1778 < _M0L3midS1775) {
        struct _M0TUsfE* _M0L6_2atmpS3873;
        int32_t _M0L6_2atmpS3874;
        #line 1190 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3873
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1774, _M0L1iS1778);
        #line 1190 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L4leftS1776, _M0L6_2atmpS3873);
        moonbit_decref(_M0L6_2atmpS3873);
        _M0L6_2atmpS3874 = _M0L1iS1778 + 1;
        _M0L1iS1778 = _M0L6_2atmpS3874;
        continue;
      }
      break;
    }
    _M0L1iS1780 = _M0L3midS1775;
    while (1) {
      if (_M0L1iS1780 < _M0L3lenS1773) {
        struct _M0TUsfE* _M0L6_2atmpS3875;
        int32_t _M0L6_2atmpS3876;
        #line 1193 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3875
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1774, _M0L1iS1780);
        #line 1193 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L5rightS1777, _M0L6_2atmpS3875);
        moonbit_decref(_M0L6_2atmpS3875);
        _M0L6_2atmpS3876 = _M0L1iS1780 + 1;
        _M0L1iS1780 = _M0L6_2atmpS3876;
        continue;
      }
      break;
    }
    #line 1195 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3877 = _M0FP19moonbitDB11merge__sort(_M0L4leftS1776);
    moonbit_decref(_M0L4leftS1776);
    #line 1195 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3878 = _M0FP19moonbitDB11merge__sort(_M0L5rightS1777);
    moonbit_decref(_M0L5rightS1777);
    #line 1195 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4527 = _M0FP19moonbitDB5merge(_M0L6_2atmpS3877, _M0L6_2atmpS3878);
    moonbit_decref(_M0L6_2atmpS3877);
    moonbit_decref(_M0L6_2atmpS3878);
    return _result_4527;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP19moonbitDB5merge(
  struct _M0TPB5ArrayGUsfEE* _M0L4leftS1768,
  struct _M0TPB5ArrayGUsfEE* _M0L5rightS1769
) {
  struct _M0TUsfE** _M0L6_2atmpS3872;
  struct _M0TPB5ArrayGUsfEE* _M0L6resultS1765;
  struct _M0TPB8MutLocalGiE* _M0L1iS1766;
  struct _M0TPB8MutLocalGiE* _M0L1jS1767;
  #line 1199 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3872 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6resultS1765
  = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
  Moonbit_object_header(_M0L6resultS1765)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
  _M0L6resultS1765->$0 = _M0L6_2atmpS3872;
  _M0L6resultS1765->$1 = 0;
  _M0L1iS1766
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1766)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS1766->$0 = 0;
  _M0L1jS1767
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1jS1767)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1jS1767->$0 = 0;
  while (1) {
    int32_t _M0L3valS3844 = _M0L1iS1766->$0;
    int32_t _M0L6_2atmpS3845;
    int32_t _if__result_4529;
    #line 1203 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3845 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1768);
    if (_M0L3valS3844 < _M0L6_2atmpS3845) {
      int32_t _M0L3valS3842 = _M0L1jS1767->$0;
      int32_t _M0L6_2atmpS3843;
      #line 1203 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3843 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1769);
      _if__result_4529 = _M0L3valS3842 < _M0L6_2atmpS3843;
    } else {
      _if__result_4529 = 0;
    }
    if (_if__result_4529) {
      int32_t _M0L3valS3851 = _M0L1iS1766->$0;
      struct _M0TUsfE* _M0L6_2atmpS3850;
      float _M0L6_2atmpS3846;
      int32_t _M0L3valS3849;
      struct _M0TUsfE* _M0L6_2atmpS3848;
      float _M0L6_2atmpS3847;
      #line 1204 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3850
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1768, _M0L3valS3851);
      _M0L6_2atmpS3846 = _M0L6_2atmpS3850->$1;
      moonbit_decref(_M0L6_2atmpS3850);
      _M0L3valS3849 = _M0L1jS1767->$0;
      #line 1204 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3848
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1769, _M0L3valS3849);
      _M0L6_2atmpS3847 = _M0L6_2atmpS3848->$1;
      moonbit_decref(_M0L6_2atmpS3848);
      if (_M0L6_2atmpS3846 <= _M0L6_2atmpS3847) {
        int32_t _M0L3valS3853 = _M0L1iS1766->$0;
        struct _M0TUsfE* _M0L6_2atmpS3852;
        int32_t _M0L3valS3855;
        int32_t _M0L6_2atmpS3854;
        #line 1205 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3852
        = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1768, _M0L3valS3853);
        #line 1205 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1765, _M0L6_2atmpS3852);
        moonbit_decref(_M0L6_2atmpS3852);
        _M0L3valS3855 = _M0L1iS1766->$0;
        _M0L6_2atmpS3854 = _M0L3valS3855 + 1;
        _M0L1iS1766->$0 = _M0L6_2atmpS3854;
      } else {
        int32_t _M0L3valS3857 = _M0L1jS1767->$0;
        struct _M0TUsfE* _M0L6_2atmpS3856;
        int32_t _M0L3valS3859;
        int32_t _M0L6_2atmpS3858;
        #line 1208 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3856
        = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1769, _M0L3valS3857);
        #line 1208 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1765, _M0L6_2atmpS3856);
        moonbit_decref(_M0L6_2atmpS3856);
        _M0L3valS3859 = _M0L1jS1767->$0;
        _M0L6_2atmpS3858 = _M0L3valS3859 + 1;
        _M0L1jS1767->$0 = _M0L6_2atmpS3858;
      }
      continue;
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS3860 = _M0L1iS1766->$0;
    int32_t _M0L6_2atmpS3861;
    #line 1212 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3861 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1768);
    if (_M0L3valS3860 < _M0L6_2atmpS3861) {
      int32_t _M0L3valS3863 = _M0L1iS1766->$0;
      struct _M0TUsfE* _M0L6_2atmpS3862;
      int32_t _M0L3valS3865;
      int32_t _M0L6_2atmpS3864;
      #line 1213 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3862
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1768, _M0L3valS3863);
      #line 1213 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1765, _M0L6_2atmpS3862);
      moonbit_decref(_M0L6_2atmpS3862);
      _M0L3valS3865 = _M0L1iS1766->$0;
      _M0L6_2atmpS3864 = _M0L3valS3865 + 1;
      _M0L1iS1766->$0 = _M0L6_2atmpS3864;
      continue;
    } else {
      moonbit_decref(_M0L1iS1766);
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS3866 = _M0L1jS1767->$0;
    int32_t _M0L6_2atmpS3867;
    #line 1216 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3867 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1769);
    if (_M0L3valS3866 < _M0L6_2atmpS3867) {
      int32_t _M0L3valS3869 = _M0L1jS1767->$0;
      struct _M0TUsfE* _M0L6_2atmpS3868;
      int32_t _M0L3valS3871;
      int32_t _M0L6_2atmpS3870;
      #line 1217 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3868
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1769, _M0L3valS3869);
      #line 1217 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1765, _M0L6_2atmpS3868);
      moonbit_decref(_M0L6_2atmpS3868);
      _M0L3valS3871 = _M0L1jS1767->$0;
      _M0L6_2atmpS3870 = _M0L3valS3871 + 1;
      _M0L1jS1767->$0 = _M0L6_2atmpS3870;
      continue;
    } else {
      moonbit_decref(_M0L1jS1767);
    }
    break;
  }
  return _M0L6resultS1765;
}

int32_t _M0MP19moonbitDB8Database4zadd(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1750,
  moonbit_string_t _M0L3keyS1751,
  float _M0L5scoreS1764,
  moonbit_string_t _M0L11member__valS1763
) {
  int32_t _M0L6_2atmpS3836;
  struct _M0TPB3MapGsfE* _M0L4zsetS1752;
  struct _M0TPB3MapGsfE* _M0L1zS1756;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3841;
  void* _M0L7_2abindS1757;
  struct _M0TUsfE** _M0L7_2abindS1754;
  struct _M0TUsfE** _M0L6_2atmpS3840;
  struct _M0TPB9ArrayViewGUsfEE _M0L6_2atmpS3839;
  int32_t _M0L7existedS1762;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3837;
  void* _M0L4ZSetS3838;
  #line 1140 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1141 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3836
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1750, _M0L3keyS1751);
  _M0L4dataS3841 = _M0L4selfS1750->$0;
  moonbit_incref(_M0L4dataS3841);
  #line 1142 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1757
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3841, _M0L3keyS1751);
  moonbit_decref(_M0L4dataS3841);
  if (_M0L7_2abindS1757 == 0) {
    if (_M0L7_2abindS1757) {
      moonbit_decref(_M0L7_2abindS1757);
    }
    goto join_1753;
  } else {
    void* _M0L7_2aSomeS1758 = _M0L7_2abindS1757;
    void* _M0L4_2axS1759 = _M0L7_2aSomeS1758;
    switch (Moonbit_object_tag(_M0L4_2axS1759)) {
      case 4: {
        struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1760 =
          (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1759;
        struct _M0TPB3MapGsfE* _M0L8_2afieldS3979 = _M0L7_2aZSetS1760->$0;
        int32_t _M0L6_2acntS4408 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1760));
        struct _M0TPB3MapGsfE* _M0L4_2azS1761;
        if (_M0L6_2acntS4408 > 1) {
          int32_t _M0L11_2anew__cntS4409 = _M0L6_2acntS4408 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1760), _M0L11_2anew__cntS4409);
          moonbit_incref(_M0L8_2afieldS3979);
        } else if (_M0L6_2acntS4408 == 1) {
          #line 1142 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aZSetS1760);
        }
        _M0L4_2azS1761 = _M0L8_2afieldS3979;
        _M0L1zS1756 = _M0L4_2azS1761;
        goto join_1755;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1759);
        goto join_1753;
        break;
      }
    }
  }
  goto joinlet_4533;
  join_1755:;
  _M0L4zsetS1752 = _M0L1zS1756;
  joinlet_4533:;
  goto joinlet_4532;
  join_1753:;
  _M0L7_2abindS1754 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3840 = _M0L7_2abindS1754;
  _M0L6_2atmpS3839
  = (struct _M0TPB9ArrayViewGUsfEE){
    .$0 = _M0L6_2atmpS3840, .$1 = 0, .$2 = 0
  };
  #line 1144 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4zsetS1752 = _M0MPB3Map3MapGsfE(_M0L6_2atmpS3839, 10ll);
  moonbit_decref(_M0L6_2atmpS3839.$0);
  joinlet_4532:;
  #line 1146 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7existedS1762
  = _M0MPB3Map8containsGsfE(_M0L4zsetS1752, _M0L11member__valS1763);
  #line 1147 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsfE(_M0L4zsetS1752, _M0L11member__valS1763, _M0L5scoreS1764);
  _M0L4dataS3837 = _M0L4selfS1750->$0;
  _M0L4ZSetS3838
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4ZSet));
  Moonbit_object_header(_M0L4ZSetS3838)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 4);
  ((struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4ZSetS3838)->$0
  = _M0L4zsetS1752;
  moonbit_incref(_M0L4dataS3837);
  #line 1148 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3837, _M0L3keyS1751, _M0L4ZSetS3838);
  moonbit_decref(_M0L4dataS3837);
  moonbit_decref(_M0L4ZSetS3838);
  return !_M0L7existedS1762;
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database5sdiff(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1714,
  struct _M0TPB5ArrayGsE* _M0L4keysS1712
) {
  int32_t _M0L6_2atmpS3822;
  moonbit_string_t _M0L6_2atmpS3835;
  struct _M0TPB3MapGsbE* _M0L10first__setS1713;
  struct _M0TUsbE** _M0L7_2abindS1716;
  struct _M0TUsbE** _M0L6_2atmpS3834;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3831;
  int32_t _M0L6_2atmpS3833;
  int64_t _M0L6_2atmpS3832;
  struct _M0TPB3MapGsbE* _M0L6resultS1715;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1717;
  int32_t _M0L1iS1725;
  moonbit_string_t* _M0L6_2atmpS3830;
  struct _M0TPB5ArrayGsE* _M0L3arrS1741;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1742;
  #line 1051 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1052 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3822 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1712);
  if (_M0L6_2atmpS3822 == 0) {
    moonbit_string_t* _M0L6_2atmpS3823 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4534 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4534)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_4534->$0 = _M0L6_2atmpS3823;
    _block_4534->$1 = 0;
    return _block_4534;
  }
  #line 1055 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3835 = _M0MPC15array5Array2atGsE(_M0L4keysS1712, 0);
  #line 1055 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L10first__setS1713
  = _M0MP19moonbitDB8Database17get__set__members(_M0L4selfS1714, _M0L6_2atmpS3835);
  moonbit_decref(_M0L6_2atmpS3835);
  _M0L7_2abindS1716 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3834 = _M0L7_2abindS1716;
  _M0L6_2atmpS3831
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3834, .$1 = 0, .$2 = 0
  };
  #line 1056 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3833 = _M0MPB3Map6lengthGsbE(_M0L10first__setS1713);
  _M0L6_2atmpS3832 = (int64_t)_M0L6_2atmpS3833;
  #line 1056 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6resultS1715 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3831, _M0L6_2atmpS3832);
  moonbit_decref(_M0L6_2atmpS3831.$0);
  #line 1056 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1717 = _M0MPB3Map5iter2GsbE(_M0L10first__setS1713);
  moonbit_decref(_M0L10first__setS1713);
  while (1) {
    moonbit_string_t _M0L1mS1719;
    struct _M0TUsbE* _M0L7_2abindS1721;
    #line 1057 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1721 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1717);
    if (_M0L7_2abindS1721 == 0) {
      if (_M0L7_2abindS1721) {
        moonbit_decref(_M0L7_2abindS1721);
      }
      moonbit_decref(_M0L5_2aitS1717);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1722 = _M0L7_2abindS1721;
      struct _M0TUsbE* _M0L4_2axS1723 = _M0L7_2aSomeS1722;
      moonbit_string_t _M0L8_2afieldS3985 = _M0L4_2axS1723->$0;
      int32_t _M0L6_2acntS4410 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1723));
      moonbit_string_t _M0L4_2amS1724;
      if (_M0L6_2acntS4410 > 1) {
        int32_t _M0L11_2anew__cntS4411 = _M0L6_2acntS4410 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1723), _M0L11_2anew__cntS4411);
        moonbit_incref(_M0L8_2afieldS3985);
      } else if (_M0L6_2acntS4410 == 1) {
        #line 1057 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1723);
      }
      _M0L4_2amS1724 = _M0L8_2afieldS3985;
      _M0L1mS1719 = _M0L4_2amS1724;
      goto join_1718;
    }
    goto joinlet_4536;
    join_1718:;
    #line 1058 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsbE(_M0L6resultS1715, _M0L1mS1719, 1);
    moonbit_decref(_M0L1mS1719);
    continue;
    joinlet_4536:;
    break;
  }
  _M0L1iS1725 = 1;
  while (1) {
    int32_t _M0L6_2atmpS3824;
    #line 1060 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3824 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1712);
    if (_M0L1iS1725 < _M0L6_2atmpS3824) {
      moonbit_string_t _M0L6_2atmpS3828;
      struct _M0TPB3MapGsbE* _M0L12current__setS1726;
      moonbit_string_t* _M0L6_2atmpS3827;
      struct _M0TPB5ArrayGsE* _M0L10to__removeS1727;
      struct _M0TPB4IterGUsbEE* _M0L5_2aitS1728;
      int32_t _M0L7_2abindS1736;
      int32_t _M0L2__S1737;
      int32_t _M0L6_2atmpS3829;
      #line 1061 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3828
      = _M0MPC15array5Array2atGsE(_M0L4keysS1712, _M0L1iS1725);
      #line 1061 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L12current__setS1726
      = _M0MP19moonbitDB8Database17get__set__members(_M0L4selfS1714, _M0L6_2atmpS3828);
      moonbit_decref(_M0L6_2atmpS3828);
      _M0L6_2atmpS3827 = (moonbit_string_t*)moonbit_empty_ref_array;
      _M0L10to__removeS1727
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_M0L10to__removeS1727)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
      _M0L10to__removeS1727->$0 = _M0L6_2atmpS3827;
      _M0L10to__removeS1727->$1 = 0;
      #line 1062 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L5_2aitS1728 = _M0MPB3Map5iter2GsbE(_M0L6resultS1715);
      while (1) {
        moonbit_string_t _M0L1mS1730;
        struct _M0TUsbE* _M0L7_2abindS1732;
        #line 1063 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L7_2abindS1732 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1728);
        if (_M0L7_2abindS1732 == 0) {
          if (_M0L7_2abindS1732) {
            moonbit_decref(_M0L7_2abindS1732);
          }
          moonbit_decref(_M0L5_2aitS1728);
          moonbit_decref(_M0L12current__setS1726);
        } else {
          struct _M0TUsbE* _M0L7_2aSomeS1733 = _M0L7_2abindS1732;
          struct _M0TUsbE* _M0L4_2axS1734 = _M0L7_2aSomeS1733;
          moonbit_string_t _M0L8_2afieldS3984 = _M0L4_2axS1734->$0;
          int32_t _M0L6_2acntS4412 =
            Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1734));
          moonbit_string_t _M0L4_2amS1735;
          if (_M0L6_2acntS4412 > 1) {
            int32_t _M0L11_2anew__cntS4413 = _M0L6_2acntS4412 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1734), _M0L11_2anew__cntS4413);
            moonbit_incref(_M0L8_2afieldS3984);
          } else if (_M0L6_2acntS4412 == 1) {
            #line 1063 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L4_2axS1734);
          }
          _M0L4_2amS1735 = _M0L8_2afieldS3984;
          _M0L1mS1730 = _M0L4_2amS1735;
          goto join_1729;
        }
        goto joinlet_4539;
        join_1729:;
        #line 1064 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        if (_M0MPB3Map8containsGsbE(_M0L12current__setS1726, _M0L1mS1730)) {
          #line 1065 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0MPC15array5Array4pushGsE(_M0L10to__removeS1727, _M0L1mS1730);
          moonbit_decref(_M0L1mS1730);
        } else {
          moonbit_decref(_M0L1mS1730);
        }
        continue;
        joinlet_4539:;
        break;
      }
      _M0L7_2abindS1736 = _M0L10to__removeS1727->$1;
      _M0L2__S1737 = 0;
      while (1) {
        if (_M0L2__S1737 < _M0L7_2abindS1736) {
          moonbit_string_t* _M0L3bufS3826 = _M0L10to__removeS1727->$0;
          moonbit_string_t _M0L1mS1738 =
            (moonbit_string_t)_M0L3bufS3826[_M0L2__S1737];
          int32_t _M0L6_2atmpS3825;
          moonbit_incref(_M0L1mS1738);
          #line 1069 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0MPB3Map6removeGsbE(_M0L6resultS1715, _M0L1mS1738);
          moonbit_decref(_M0L1mS1738);
          _M0L6_2atmpS3825 = _M0L2__S1737 + 1;
          _M0L2__S1737 = _M0L6_2atmpS3825;
          continue;
        } else {
          moonbit_decref(_M0L10to__removeS1727);
        }
        break;
      }
      _M0L6_2atmpS3829 = _M0L1iS1725 + 1;
      _M0L1iS1725 = _M0L6_2atmpS3829;
      continue;
    }
    break;
  }
  _M0L6_2atmpS3830 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3arrS1741
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3arrS1741)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L3arrS1741->$0 = _M0L6_2atmpS3830;
  _M0L3arrS1741->$1 = 0;
  #line 1072 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1742 = _M0MPB3Map5iter2GsbE(_M0L6resultS1715);
  moonbit_decref(_M0L6resultS1715);
  while (1) {
    moonbit_string_t _M0L1mS1744;
    struct _M0TUsbE* _M0L7_2abindS1746;
    #line 1073 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1746 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1742);
    if (_M0L7_2abindS1746 == 0) {
      if (_M0L7_2abindS1746) {
        moonbit_decref(_M0L7_2abindS1746);
      }
      moonbit_decref(_M0L5_2aitS1742);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1747 = _M0L7_2abindS1746;
      struct _M0TUsbE* _M0L4_2axS1748 = _M0L7_2aSomeS1747;
      moonbit_string_t _M0L8_2afieldS3981 = _M0L4_2axS1748->$0;
      int32_t _M0L6_2acntS4414 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1748));
      moonbit_string_t _M0L4_2amS1749;
      if (_M0L6_2acntS4414 > 1) {
        int32_t _M0L11_2anew__cntS4415 = _M0L6_2acntS4414 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1748), _M0L11_2anew__cntS4415);
        moonbit_incref(_M0L8_2afieldS3981);
      } else if (_M0L6_2acntS4414 == 1) {
        #line 1073 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1748);
      }
      _M0L4_2amS1749 = _M0L8_2afieldS3981;
      _M0L1mS1744 = _M0L4_2amS1749;
      goto join_1743;
    }
    goto joinlet_4542;
    join_1743:;
    #line 1074 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3arrS1741, _M0L1mS1744);
    moonbit_decref(_M0L1mS1744);
    continue;
    joinlet_4542:;
    break;
  }
  return _M0L3arrS1741;
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database6sunion(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1693,
  struct _M0TPB5ArrayGsE* _M0L4keysS1689
) {
  struct _M0TUsbE** _M0L7_2abindS1687;
  struct _M0TUsbE** _M0L6_2atmpS3821;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3820;
  struct _M0TPB3MapGsbE* _M0L6resultS1686;
  int32_t _M0L7_2abindS1688;
  int32_t _M0L2__S1690;
  moonbit_string_t* _M0L6_2atmpS3819;
  struct _M0TPB5ArrayGsE* _M0L3arrS1703;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1704;
  #line 1026 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1687 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3821 = _M0L7_2abindS1687;
  _M0L6_2atmpS3820
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3821, .$1 = 0, .$2 = 0
  };
  #line 1027 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6resultS1686 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3820, 10ll);
  moonbit_decref(_M0L6_2atmpS3820.$0);
  _M0L7_2abindS1688 = _M0L4keysS1689->$1;
  _M0L2__S1690 = 0;
  while (1) {
    if (_M0L2__S1690 < _M0L7_2abindS1688) {
      moonbit_string_t* _M0L3bufS3818 = _M0L4keysS1689->$0;
      moonbit_string_t _M0L3keyS1691 =
        (moonbit_string_t)_M0L3bufS3818[_M0L2__S1690];
      struct _M0TPB3MapGsbE* _M0L12current__setS1692;
      struct _M0TPB4IterGUsbEE* _M0L5_2aitS1694;
      int32_t _M0L6_2atmpS3817;
      moonbit_incref(_M0L3keyS1691);
      #line 1029 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L12current__setS1692
      = _M0MP19moonbitDB8Database17get__set__members(_M0L4selfS1693, _M0L3keyS1691);
      moonbit_decref(_M0L3keyS1691);
      #line 1029 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L5_2aitS1694 = _M0MPB3Map5iter2GsbE(_M0L12current__setS1692);
      moonbit_decref(_M0L12current__setS1692);
      while (1) {
        moonbit_string_t _M0L1mS1696;
        struct _M0TUsbE* _M0L7_2abindS1698;
        #line 1030 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L7_2abindS1698 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1694);
        if (_M0L7_2abindS1698 == 0) {
          if (_M0L7_2abindS1698) {
            moonbit_decref(_M0L7_2abindS1698);
          }
          moonbit_decref(_M0L5_2aitS1694);
        } else {
          struct _M0TUsbE* _M0L7_2aSomeS1699 = _M0L7_2abindS1698;
          struct _M0TUsbE* _M0L4_2axS1700 = _M0L7_2aSomeS1699;
          moonbit_string_t _M0L8_2afieldS3987 = _M0L4_2axS1700->$0;
          int32_t _M0L6_2acntS4416 =
            Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1700));
          moonbit_string_t _M0L4_2amS1701;
          if (_M0L6_2acntS4416 > 1) {
            int32_t _M0L11_2anew__cntS4417 = _M0L6_2acntS4416 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1700), _M0L11_2anew__cntS4417);
            moonbit_incref(_M0L8_2afieldS3987);
          } else if (_M0L6_2acntS4416 == 1) {
            #line 1030 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L4_2axS1700);
          }
          _M0L4_2amS1701 = _M0L8_2afieldS3987;
          _M0L1mS1696 = _M0L4_2amS1701;
          goto join_1695;
        }
        goto joinlet_4545;
        join_1695:;
        #line 1031 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPB3Map3setGsbE(_M0L6resultS1686, _M0L1mS1696, 1);
        moonbit_decref(_M0L1mS1696);
        continue;
        joinlet_4545:;
        break;
      }
      _M0L6_2atmpS3817 = _M0L2__S1690 + 1;
      _M0L2__S1690 = _M0L6_2atmpS3817;
      continue;
    }
    break;
  }
  _M0L6_2atmpS3819 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3arrS1703
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3arrS1703)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L3arrS1703->$0 = _M0L6_2atmpS3819;
  _M0L3arrS1703->$1 = 0;
  #line 1034 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1704 = _M0MPB3Map5iter2GsbE(_M0L6resultS1686);
  moonbit_decref(_M0L6resultS1686);
  while (1) {
    moonbit_string_t _M0L1mS1706;
    struct _M0TUsbE* _M0L7_2abindS1708;
    #line 1035 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1708 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1704);
    if (_M0L7_2abindS1708 == 0) {
      if (_M0L7_2abindS1708) {
        moonbit_decref(_M0L7_2abindS1708);
      }
      moonbit_decref(_M0L5_2aitS1704);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1709 = _M0L7_2abindS1708;
      struct _M0TUsbE* _M0L4_2axS1710 = _M0L7_2aSomeS1709;
      moonbit_string_t _M0L8_2afieldS3986 = _M0L4_2axS1710->$0;
      int32_t _M0L6_2acntS4418 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1710));
      moonbit_string_t _M0L4_2amS1711;
      if (_M0L6_2acntS4418 > 1) {
        int32_t _M0L11_2anew__cntS4419 = _M0L6_2acntS4418 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1710), _M0L11_2anew__cntS4419);
        moonbit_incref(_M0L8_2afieldS3986);
      } else if (_M0L6_2acntS4418 == 1) {
        #line 1035 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1710);
      }
      _M0L4_2amS1711 = _M0L8_2afieldS3986;
      _M0L1mS1706 = _M0L4_2amS1711;
      goto join_1705;
    }
    goto joinlet_4547;
    join_1705:;
    #line 1036 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3arrS1703, _M0L1mS1706);
    moonbit_decref(_M0L1mS1706);
    continue;
    joinlet_4547:;
    break;
  }
  return _M0L3arrS1703;
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database6sinter(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1650,
  struct _M0TPB5ArrayGsE* _M0L4keysS1648
) {
  int32_t _M0L6_2atmpS3800;
  moonbit_string_t _M0L6_2atmpS3816;
  struct _M0TPB3MapGsbE* _M0L10first__setS1649;
  int32_t _M0L6_2atmpS3802;
  struct _M0TUsbE** _M0L7_2abindS1652;
  struct _M0TUsbE** _M0L6_2atmpS3815;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3812;
  int32_t _M0L6_2atmpS3814;
  int64_t _M0L6_2atmpS3813;
  struct _M0TPB3MapGsbE* _M0L6resultS1651;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1653;
  int32_t _M0L1iS1661;
  moonbit_string_t* _M0L6_2atmpS3811;
  struct _M0TPB5ArrayGsE* _M0L3arrS1677;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1678;
  #line 985 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 986 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3800 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1648);
  if (_M0L6_2atmpS3800 == 0) {
    moonbit_string_t* _M0L6_2atmpS3801 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4548 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4548)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_4548->$0 = _M0L6_2atmpS3801;
    _block_4548->$1 = 0;
    return _block_4548;
  }
  #line 989 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3816 = _M0MPC15array5Array2atGsE(_M0L4keysS1648, 0);
  #line 989 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L10first__setS1649
  = _M0MP19moonbitDB8Database17get__set__members(_M0L4selfS1650, _M0L6_2atmpS3816);
  moonbit_decref(_M0L6_2atmpS3816);
  #line 990 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3802 = _M0MPB3Map6lengthGsbE(_M0L10first__setS1649);
  if (_M0L6_2atmpS3802 == 0) {
    moonbit_string_t* _M0L6_2atmpS3803;
    struct _M0TPB5ArrayGsE* _block_4549;
    moonbit_decref(_M0L10first__setS1649);
    _M0L6_2atmpS3803 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_4549
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4549)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_4549->$0 = _M0L6_2atmpS3803;
    _block_4549->$1 = 0;
    return _block_4549;
  }
  _M0L7_2abindS1652 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3815 = _M0L7_2abindS1652;
  _M0L6_2atmpS3812
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3815, .$1 = 0, .$2 = 0
  };
  #line 993 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3814 = _M0MPB3Map6lengthGsbE(_M0L10first__setS1649);
  _M0L6_2atmpS3813 = (int64_t)_M0L6_2atmpS3814;
  #line 993 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6resultS1651 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3812, _M0L6_2atmpS3813);
  moonbit_decref(_M0L6_2atmpS3812.$0);
  #line 993 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1653 = _M0MPB3Map5iter2GsbE(_M0L10first__setS1649);
  moonbit_decref(_M0L10first__setS1649);
  while (1) {
    moonbit_string_t _M0L1mS1655;
    struct _M0TUsbE* _M0L7_2abindS1657;
    #line 994 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1657 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1653);
    if (_M0L7_2abindS1657 == 0) {
      if (_M0L7_2abindS1657) {
        moonbit_decref(_M0L7_2abindS1657);
      }
      moonbit_decref(_M0L5_2aitS1653);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1658 = _M0L7_2abindS1657;
      struct _M0TUsbE* _M0L4_2axS1659 = _M0L7_2aSomeS1658;
      moonbit_string_t _M0L8_2afieldS3994 = _M0L4_2axS1659->$0;
      int32_t _M0L6_2acntS4420 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1659));
      moonbit_string_t _M0L4_2amS1660;
      if (_M0L6_2acntS4420 > 1) {
        int32_t _M0L11_2anew__cntS4421 = _M0L6_2acntS4420 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1659), _M0L11_2anew__cntS4421);
        moonbit_incref(_M0L8_2afieldS3994);
      } else if (_M0L6_2acntS4420 == 1) {
        #line 994 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1659);
      }
      _M0L4_2amS1660 = _M0L8_2afieldS3994;
      _M0L1mS1655 = _M0L4_2amS1660;
      goto join_1654;
    }
    goto joinlet_4551;
    join_1654:;
    #line 995 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsbE(_M0L6resultS1651, _M0L1mS1655, 1);
    moonbit_decref(_M0L1mS1655);
    continue;
    joinlet_4551:;
    break;
  }
  _M0L1iS1661 = 1;
  while (1) {
    int32_t _M0L6_2atmpS3804;
    #line 997 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3804 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1648);
    if (_M0L1iS1661 < _M0L6_2atmpS3804) {
      moonbit_string_t _M0L6_2atmpS3809;
      struct _M0TPB3MapGsbE* _M0L12current__setS1662;
      moonbit_string_t* _M0L6_2atmpS3808;
      struct _M0TPB5ArrayGsE* _M0L10to__removeS1663;
      struct _M0TPB4IterGUsbEE* _M0L5_2aitS1664;
      int32_t _M0L7_2abindS1672;
      int32_t _M0L2__S1673;
      int32_t _M0L6_2atmpS3810;
      #line 998 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3809
      = _M0MPC15array5Array2atGsE(_M0L4keysS1648, _M0L1iS1661);
      #line 998 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L12current__setS1662
      = _M0MP19moonbitDB8Database17get__set__members(_M0L4selfS1650, _M0L6_2atmpS3809);
      moonbit_decref(_M0L6_2atmpS3809);
      _M0L6_2atmpS3808 = (moonbit_string_t*)moonbit_empty_ref_array;
      _M0L10to__removeS1663
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_M0L10to__removeS1663)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
      _M0L10to__removeS1663->$0 = _M0L6_2atmpS3808;
      _M0L10to__removeS1663->$1 = 0;
      #line 999 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L5_2aitS1664 = _M0MPB3Map5iter2GsbE(_M0L6resultS1651);
      while (1) {
        moonbit_string_t _M0L1mS1666;
        struct _M0TUsbE* _M0L7_2abindS1668;
        int32_t _M0L6_2atmpS3805;
        #line 1000 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L7_2abindS1668 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1664);
        if (_M0L7_2abindS1668 == 0) {
          if (_M0L7_2abindS1668) {
            moonbit_decref(_M0L7_2abindS1668);
          }
          moonbit_decref(_M0L5_2aitS1664);
          moonbit_decref(_M0L12current__setS1662);
        } else {
          struct _M0TUsbE* _M0L7_2aSomeS1669 = _M0L7_2abindS1668;
          struct _M0TUsbE* _M0L4_2axS1670 = _M0L7_2aSomeS1669;
          moonbit_string_t _M0L8_2afieldS3993 = _M0L4_2axS1670->$0;
          int32_t _M0L6_2acntS4422 =
            Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1670));
          moonbit_string_t _M0L4_2amS1671;
          if (_M0L6_2acntS4422 > 1) {
            int32_t _M0L11_2anew__cntS4423 = _M0L6_2acntS4422 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1670), _M0L11_2anew__cntS4423);
            moonbit_incref(_M0L8_2afieldS3993);
          } else if (_M0L6_2acntS4422 == 1) {
            #line 1000 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L4_2axS1670);
          }
          _M0L4_2amS1671 = _M0L8_2afieldS3993;
          _M0L1mS1666 = _M0L4_2amS1671;
          goto join_1665;
        }
        goto joinlet_4554;
        join_1665:;
        #line 1001 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3805
        = _M0MPB3Map8containsGsbE(_M0L12current__setS1662, _M0L1mS1666);
        if (!_M0L6_2atmpS3805) {
          #line 1002 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0MPC15array5Array4pushGsE(_M0L10to__removeS1663, _M0L1mS1666);
          moonbit_decref(_M0L1mS1666);
        } else {
          moonbit_decref(_M0L1mS1666);
        }
        continue;
        joinlet_4554:;
        break;
      }
      _M0L7_2abindS1672 = _M0L10to__removeS1663->$1;
      _M0L2__S1673 = 0;
      while (1) {
        if (_M0L2__S1673 < _M0L7_2abindS1672) {
          moonbit_string_t* _M0L3bufS3807 = _M0L10to__removeS1663->$0;
          moonbit_string_t _M0L1mS1674 =
            (moonbit_string_t)_M0L3bufS3807[_M0L2__S1673];
          int32_t _M0L6_2atmpS3806;
          moonbit_incref(_M0L1mS1674);
          #line 1006 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0MPB3Map6removeGsbE(_M0L6resultS1651, _M0L1mS1674);
          moonbit_decref(_M0L1mS1674);
          _M0L6_2atmpS3806 = _M0L2__S1673 + 1;
          _M0L2__S1673 = _M0L6_2atmpS3806;
          continue;
        } else {
          moonbit_decref(_M0L10to__removeS1663);
        }
        break;
      }
      _M0L6_2atmpS3810 = _M0L1iS1661 + 1;
      _M0L1iS1661 = _M0L6_2atmpS3810;
      continue;
    }
    break;
  }
  _M0L6_2atmpS3811 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3arrS1677
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3arrS1677)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L3arrS1677->$0 = _M0L6_2atmpS3811;
  _M0L3arrS1677->$1 = 0;
  #line 1009 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1678 = _M0MPB3Map5iter2GsbE(_M0L6resultS1651);
  moonbit_decref(_M0L6resultS1651);
  while (1) {
    moonbit_string_t _M0L1mS1680;
    struct _M0TUsbE* _M0L7_2abindS1682;
    #line 1010 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1682 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1678);
    if (_M0L7_2abindS1682 == 0) {
      if (_M0L7_2abindS1682) {
        moonbit_decref(_M0L7_2abindS1682);
      }
      moonbit_decref(_M0L5_2aitS1678);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1683 = _M0L7_2abindS1682;
      struct _M0TUsbE* _M0L4_2axS1684 = _M0L7_2aSomeS1683;
      moonbit_string_t _M0L8_2afieldS3990 = _M0L4_2axS1684->$0;
      int32_t _M0L6_2acntS4424 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1684));
      moonbit_string_t _M0L4_2amS1685;
      if (_M0L6_2acntS4424 > 1) {
        int32_t _M0L11_2anew__cntS4425 = _M0L6_2acntS4424 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1684), _M0L11_2anew__cntS4425);
        moonbit_incref(_M0L8_2afieldS3990);
      } else if (_M0L6_2acntS4424 == 1) {
        #line 1010 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1684);
      }
      _M0L4_2amS1685 = _M0L8_2afieldS3990;
      _M0L1mS1680 = _M0L4_2amS1685;
      goto join_1679;
    }
    goto joinlet_4557;
    join_1679:;
    #line 1011 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3arrS1677, _M0L1mS1680);
    moonbit_decref(_M0L1mS1680);
    continue;
    joinlet_4557:;
    break;
  }
  return _M0L3arrS1677;
}

struct _M0TPB3MapGsbE* _M0MP19moonbitDB8Database17get__set__members(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1636,
  moonbit_string_t _M0L3keyS1637
) {
  #line 974 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 975 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1636, _M0L3keyS1637)
  ) {
    struct _M0TUsbE** _M0L7_2abindS1638 =
      (struct _M0TUsbE**)moonbit_empty_ref_array;
    struct _M0TUsbE** _M0L6_2atmpS3796 = _M0L7_2abindS1638;
    struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3795 =
      (struct _M0TPB9ArrayViewGUsbEE){.$0 = _M0L6_2atmpS3796,
                                        .$1 = 0,
                                        .$2 = 0};
    struct _M0TPB3MapGsbE* _result_4558;
    #line 976 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4558 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3795, 0ll);
    moonbit_decref(_M0L6_2atmpS3795.$0);
    return _result_4558;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1642;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3799 =
      _M0L4selfS1636->$0;
    void* _M0L7_2abindS1643;
    struct _M0TUsbE** _M0L7_2abindS1640;
    struct _M0TUsbE** _M0L6_2atmpS3798;
    struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3797;
    struct _M0TPB3MapGsbE* _result_4561;
    moonbit_incref(_M0L4dataS3799);
    #line 978 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1643
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3799, _M0L3keyS1637);
    moonbit_decref(_M0L4dataS3799);
    if (_M0L7_2abindS1643 == 0) {
      if (_M0L7_2abindS1643) {
        moonbit_decref(_M0L7_2abindS1643);
      }
      goto join_1639;
    } else {
      void* _M0L7_2aSomeS1644 = _M0L7_2abindS1643;
      void* _M0L4_2axS1645 = _M0L7_2aSomeS1644;
      switch (Moonbit_object_tag(_M0L4_2axS1645)) {
        case 3: {
          struct _M0DTP19moonbitDB10RedisValue3Set* _M0L6_2aSetS1646 =
            (struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L4_2axS1645;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3995 = _M0L6_2aSetS1646->$0;
          int32_t _M0L6_2acntS4426 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1646));
          struct _M0TPB3MapGsbE* _M0L4_2asS1647;
          if (_M0L6_2acntS4426 > 1) {
            int32_t _M0L11_2anew__cntS4427 = _M0L6_2acntS4426 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1646), _M0L11_2anew__cntS4427);
            moonbit_incref(_M0L8_2afieldS3995);
          } else if (_M0L6_2acntS4426 == 1) {
            #line 978 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2aSetS1646);
          }
          _M0L4_2asS1647 = _M0L8_2afieldS3995;
          _M0L1sS1642 = _M0L4_2asS1647;
          goto join_1641;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1645);
          goto join_1639;
          break;
        }
      }
    }
    join_1641:;
    return _M0L1sS1642;
    join_1639:;
    _M0L7_2abindS1640 = (struct _M0TUsbE**)moonbit_empty_ref_array;
    _M0L6_2atmpS3798 = _M0L7_2abindS1640;
    _M0L6_2atmpS3797
    = (struct _M0TPB9ArrayViewGUsbEE){
      .$0 = _M0L6_2atmpS3798, .$1 = 0, .$2 = 0
    };
    #line 980 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4561 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3797, 0ll);
    moonbit_decref(_M0L6_2atmpS3797.$0);
    return _result_4561;
  }
}

int32_t _M0MP19moonbitDB8Database9sismember(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1626,
  moonbit_string_t _M0L3keyS1627,
  moonbit_string_t _M0L5valueS1630
) {
  #line 963 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 964 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1626, _M0L3keyS1627)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1629;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3794 =
      _M0L4selfS1626->$0;
    void* _M0L7_2abindS1631;
    int32_t _result_4563;
    moonbit_incref(_M0L4dataS3794);
    #line 967 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1631
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3794, _M0L3keyS1627);
    moonbit_decref(_M0L4dataS3794);
    if (_M0L7_2abindS1631 == 0) {
      if (_M0L7_2abindS1631) {
        moonbit_decref(_M0L7_2abindS1631);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1632 = _M0L7_2abindS1631;
      void* _M0L4_2axS1633 = _M0L7_2aSomeS1632;
      switch (Moonbit_object_tag(_M0L4_2axS1633)) {
        case 3: {
          struct _M0DTP19moonbitDB10RedisValue3Set* _M0L6_2aSetS1634 =
            (struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L4_2axS1633;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3997 = _M0L6_2aSetS1634->$0;
          int32_t _M0L6_2acntS4428 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1634));
          struct _M0TPB3MapGsbE* _M0L4_2asS1635;
          if (_M0L6_2acntS4428 > 1) {
            int32_t _M0L11_2anew__cntS4429 = _M0L6_2acntS4428 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1634), _M0L11_2anew__cntS4429);
            moonbit_incref(_M0L8_2afieldS3997);
          } else if (_M0L6_2acntS4428 == 1) {
            #line 967 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2aSetS1634);
          }
          _M0L4_2asS1635 = _M0L8_2afieldS3997;
          _M0L1sS1629 = _M0L4_2asS1635;
          goto join_1628;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1633);
          return 0;
          break;
        }
      }
    }
    join_1628:;
    #line 968 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4563 = _M0MPB3Map8containsGsbE(_M0L1sS1629, _M0L5valueS1630);
    moonbit_decref(_M0L1sS1629);
    return _result_4563;
  }
}

int32_t _M0MP19moonbitDB8Database5scard(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1617,
  moonbit_string_t _M0L3keyS1618
) {
  #line 952 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 953 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1617, _M0L3keyS1618)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1620;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3793 =
      _M0L4selfS1617->$0;
    void* _M0L7_2abindS1621;
    int32_t _result_4565;
    moonbit_incref(_M0L4dataS3793);
    #line 956 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1621
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3793, _M0L3keyS1618);
    moonbit_decref(_M0L4dataS3793);
    if (_M0L7_2abindS1621 == 0) {
      if (_M0L7_2abindS1621) {
        moonbit_decref(_M0L7_2abindS1621);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1622 = _M0L7_2abindS1621;
      void* _M0L4_2axS1623 = _M0L7_2aSomeS1622;
      switch (Moonbit_object_tag(_M0L4_2axS1623)) {
        case 3: {
          struct _M0DTP19moonbitDB10RedisValue3Set* _M0L6_2aSetS1624 =
            (struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L4_2axS1623;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3999 = _M0L6_2aSetS1624->$0;
          int32_t _M0L6_2acntS4430 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1624));
          struct _M0TPB3MapGsbE* _M0L4_2asS1625;
          if (_M0L6_2acntS4430 > 1) {
            int32_t _M0L11_2anew__cntS4431 = _M0L6_2acntS4430 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1624), _M0L11_2anew__cntS4431);
            moonbit_incref(_M0L8_2afieldS3999);
          } else if (_M0L6_2acntS4430 == 1) {
            #line 956 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2aSetS1624);
          }
          _M0L4_2asS1625 = _M0L8_2afieldS3999;
          _M0L1sS1620 = _M0L4_2asS1625;
          goto join_1619;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1623);
          return 0;
          break;
        }
      }
    }
    join_1619:;
    #line 957 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4565 = _M0MPB3Map6lengthGsbE(_M0L1sS1620);
    moonbit_decref(_M0L1sS1620);
    return _result_4565;
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database8smembers(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1598,
  moonbit_string_t _M0L3keyS1599
) {
  #line 917 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 918 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1598, _M0L3keyS1599)
  ) {
    moonbit_string_t* _M0L6_2atmpS3789 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4566 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4566)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_4566->$0 = _M0L6_2atmpS3789;
    _block_4566->$1 = 0;
    return _block_4566;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1602;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3792 =
      _M0L4selfS1598->$0;
    void* _M0L7_2abindS1612;
    moonbit_string_t* _M0L6_2atmpS3791;
    struct _M0TPB5ArrayGsE* _M0L6resultS1603;
    struct _M0TPB4IterGUsbEE* _M0L5_2aitS1604;
    moonbit_string_t* _M0L6_2atmpS3790;
    struct _M0TPB5ArrayGsE* _block_4571;
    moonbit_incref(_M0L4dataS3792);
    #line 921 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1612
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3792, _M0L3keyS1599);
    moonbit_decref(_M0L4dataS3792);
    if (_M0L7_2abindS1612 == 0) {
      if (_M0L7_2abindS1612) {
        moonbit_decref(_M0L7_2abindS1612);
      }
      goto join_1600;
    } else {
      void* _M0L7_2aSomeS1613 = _M0L7_2abindS1612;
      void* _M0L4_2axS1614 = _M0L7_2aSomeS1613;
      switch (Moonbit_object_tag(_M0L4_2axS1614)) {
        case 3: {
          struct _M0DTP19moonbitDB10RedisValue3Set* _M0L6_2aSetS1615 =
            (struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L4_2axS1614;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS4002 = _M0L6_2aSetS1615->$0;
          int32_t _M0L6_2acntS4434 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1615));
          struct _M0TPB3MapGsbE* _M0L4_2asS1616;
          if (_M0L6_2acntS4434 > 1) {
            int32_t _M0L11_2anew__cntS4435 = _M0L6_2acntS4434 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1615), _M0L11_2anew__cntS4435);
            moonbit_incref(_M0L8_2afieldS4002);
          } else if (_M0L6_2acntS4434 == 1) {
            #line 921 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2aSetS1615);
          }
          _M0L4_2asS1616 = _M0L8_2afieldS4002;
          _M0L1sS1602 = _M0L4_2asS1616;
          goto join_1601;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1614);
          goto join_1600;
          break;
        }
      }
    }
    join_1601:;
    _M0L6_2atmpS3791 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1603
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1603)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _M0L6resultS1603->$0 = _M0L6_2atmpS3791;
    _M0L6resultS1603->$1 = 0;
    #line 923 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L5_2aitS1604 = _M0MPB3Map5iter2GsbE(_M0L1sS1602);
    moonbit_decref(_M0L1sS1602);
    while (1) {
      moonbit_string_t _M0L1mS1606;
      struct _M0TUsbE* _M0L7_2abindS1608;
      #line 924 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L7_2abindS1608 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1604);
      if (_M0L7_2abindS1608 == 0) {
        if (_M0L7_2abindS1608) {
          moonbit_decref(_M0L7_2abindS1608);
        }
        moonbit_decref(_M0L5_2aitS1604);
      } else {
        struct _M0TUsbE* _M0L7_2aSomeS1609 = _M0L7_2abindS1608;
        struct _M0TUsbE* _M0L4_2axS1610 = _M0L7_2aSomeS1609;
        moonbit_string_t _M0L8_2afieldS4001 = _M0L4_2axS1610->$0;
        int32_t _M0L6_2acntS4432 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1610));
        moonbit_string_t _M0L4_2amS1611;
        if (_M0L6_2acntS4432 > 1) {
          int32_t _M0L11_2anew__cntS4433 = _M0L6_2acntS4432 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1610), _M0L11_2anew__cntS4433);
          moonbit_incref(_M0L8_2afieldS4001);
        } else if (_M0L6_2acntS4432 == 1) {
          #line 924 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L4_2axS1610);
        }
        _M0L4_2amS1611 = _M0L8_2afieldS4001;
        _M0L1mS1606 = _M0L4_2amS1611;
        goto join_1605;
      }
      goto joinlet_4570;
      join_1605:;
      #line 925 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1603, _M0L1mS1606);
      moonbit_decref(_M0L1mS1606);
      continue;
      joinlet_4570:;
      break;
    }
    return _M0L6resultS1603;
    join_1600:;
    _M0L6_2atmpS3790 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_4571
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4571)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_4571->$0 = _M0L6_2atmpS3790;
    _block_4571->$1 = 0;
    return _block_4571;
  }
}

int32_t _M0MP19moonbitDB8Database4sadd(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1585,
  moonbit_string_t _M0L3keyS1586,
  moonbit_string_t _M0L5valueS1597
) {
  int32_t _M0L6_2atmpS3783;
  struct _M0TPB3MapGsbE* _M0L3setS1587;
  struct _M0TPB3MapGsbE* _M0L1sS1591;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3788;
  void* _M0L7_2abindS1592;
  struct _M0TUsbE** _M0L7_2abindS1589;
  struct _M0TUsbE** _M0L6_2atmpS3787;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3786;
  #line 902 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 903 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3783
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1585, _M0L3keyS1586);
  _M0L4dataS3788 = _M0L4selfS1585->$0;
  moonbit_incref(_M0L4dataS3788);
  #line 904 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1592
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3788, _M0L3keyS1586);
  moonbit_decref(_M0L4dataS3788);
  if (_M0L7_2abindS1592 == 0) {
    if (_M0L7_2abindS1592) {
      moonbit_decref(_M0L7_2abindS1592);
    }
    goto join_1588;
  } else {
    void* _M0L7_2aSomeS1593 = _M0L7_2abindS1592;
    void* _M0L4_2axS1594 = _M0L7_2aSomeS1593;
    switch (Moonbit_object_tag(_M0L4_2axS1594)) {
      case 3: {
        struct _M0DTP19moonbitDB10RedisValue3Set* _M0L6_2aSetS1595 =
          (struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L4_2axS1594;
        struct _M0TPB3MapGsbE* _M0L8_2afieldS4005 = _M0L6_2aSetS1595->$0;
        int32_t _M0L6_2acntS4436 =
          Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1595));
        struct _M0TPB3MapGsbE* _M0L4_2asS1596;
        if (_M0L6_2acntS4436 > 1) {
          int32_t _M0L11_2anew__cntS4437 = _M0L6_2acntS4436 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1595), _M0L11_2anew__cntS4437);
          moonbit_incref(_M0L8_2afieldS4005);
        } else if (_M0L6_2acntS4436 == 1) {
          #line 904 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L6_2aSetS1595);
        }
        _M0L4_2asS1596 = _M0L8_2afieldS4005;
        _M0L1sS1591 = _M0L4_2asS1596;
        goto join_1590;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1594);
        goto join_1588;
        break;
      }
    }
  }
  goto joinlet_4573;
  join_1590:;
  _M0L3setS1587 = _M0L1sS1591;
  joinlet_4573:;
  goto joinlet_4572;
  join_1588:;
  _M0L7_2abindS1589 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3787 = _M0L7_2abindS1589;
  _M0L6_2atmpS3786
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3787, .$1 = 0, .$2 = 0
  };
  #line 906 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L3setS1587 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3786, 10ll);
  moonbit_decref(_M0L6_2atmpS3786.$0);
  joinlet_4572:;
  #line 908 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (_M0MPB3Map8containsGsbE(_M0L3setS1587, _M0L5valueS1597)) {
    moonbit_decref(_M0L3setS1587);
    return 0;
  } else {
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3784;
    void* _M0L3SetS3785;
    #line 911 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsbE(_M0L3setS1587, _M0L5valueS1597, 1);
    _M0L4dataS3784 = _M0L4selfS1585->$0;
    _M0L3SetS3785
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue3Set));
    Moonbit_object_header(_M0L3SetS3785)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 16, 3);
    ((struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L3SetS3785)->$0
    = _M0L3setS1587;
    moonbit_incref(_M0L4dataS3784);
    #line 912 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3784, _M0L3keyS1586, _M0L3SetS3785);
    moonbit_decref(_M0L4dataS3784);
    moonbit_decref(_M0L3SetS3785);
    return 1;
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database6lrange(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1566,
  moonbit_string_t _M0L3keyS1567,
  int32_t _M0L5startS1574,
  int32_t _M0L3endS1576
) {
  #line 720 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 721 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1566, _M0L3keyS1567)
  ) {
    moonbit_string_t* _M0L6_2atmpS3777 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4574 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4574)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_4574->$0 = _M0L6_2atmpS3777;
    _block_4574->$1 = 0;
    return _block_4574;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L5dequeS1570;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3782 =
      _M0L4selfS1566->$0;
    void* _M0L7_2abindS1580;
    struct _M0TPB5ArrayGsE* _M0L3arrS1571;
    int32_t _M0L3lenS1572;
    int32_t _M0L10start__idxS1573;
    int32_t _M0L8end__idxS1575;
    moonbit_string_t* _M0L6_2atmpS3781;
    struct _M0TPB5ArrayGsE* _M0L6resultS1577;
    int32_t _M0L1iS1578;
    moonbit_string_t* _M0L6_2atmpS3778;
    struct _M0TPB5ArrayGsE* _block_4579;
    moonbit_incref(_M0L4dataS3782);
    #line 724 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1580
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3782, _M0L3keyS1567);
    moonbit_decref(_M0L4dataS3782);
    if (_M0L7_2abindS1580 == 0) {
      if (_M0L7_2abindS1580) {
        moonbit_decref(_M0L7_2abindS1580);
      }
      goto join_1568;
    } else {
      void* _M0L7_2aSomeS1581 = _M0L7_2abindS1580;
      void* _M0L4_2axS1582 = _M0L7_2aSomeS1581;
      switch (Moonbit_object_tag(_M0L4_2axS1582)) {
        case 2: {
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1583 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1582;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS4007 =
            _M0L7_2aListS1583->$0;
          int32_t _M0L6_2acntS4438 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1583));
          struct _M0TP19moonbitDB5Deque* _M0L8_2adequeS1584;
          if (_M0L6_2acntS4438 > 1) {
            int32_t _M0L11_2anew__cntS4439 = _M0L6_2acntS4438 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1583), _M0L11_2anew__cntS4439);
            moonbit_incref(_M0L8_2afieldS4007);
          } else if (_M0L6_2acntS4438 == 1) {
            #line 724 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1583);
          }
          _M0L8_2adequeS1584 = _M0L8_2afieldS4007;
          _M0L5dequeS1570 = _M0L8_2adequeS1584;
          goto join_1569;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1582);
          goto join_1568;
          break;
        }
      }
    }
    join_1569:;
    #line 726 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3arrS1571 = _M0MP19moonbitDB5Deque9to__array(_M0L5dequeS1570);
    moonbit_decref(_M0L5dequeS1570);
    #line 727 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3lenS1572 = _M0MPC15array5Array6lengthGsE(_M0L3arrS1571);
    if (_M0L5startS1574 < 0) {
      _M0L10start__idxS1573 = _M0L3lenS1572 + _M0L5startS1574;
    } else {
      _M0L10start__idxS1573 = _M0L5startS1574;
    }
    if (_M0L3endS1576 < 0) {
      _M0L8end__idxS1575 = _M0L3lenS1572 + _M0L3endS1576;
    } else {
      _M0L8end__idxS1575 = _M0L3endS1576;
    }
    _M0L6_2atmpS3781 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1577
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1577)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _M0L6resultS1577->$0 = _M0L6_2atmpS3781;
    _M0L6resultS1577->$1 = 0;
    _M0L1iS1578 = _M0L10start__idxS1573;
    while (1) {
      int32_t _if__result_4578;
      if (_M0L1iS1578 <= _M0L8end__idxS1575) {
        if (_M0L1iS1578 >= 0) {
          _if__result_4578 = _M0L1iS1578 < _M0L3lenS1572;
        } else {
          _if__result_4578 = 0;
        }
      } else {
        _if__result_4578 = 0;
      }
      if (_if__result_4578) {
        moonbit_string_t _M0L6_2atmpS3779;
        int32_t _M0L6_2atmpS3780;
        #line 732 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3779
        = _M0MPC15array5Array2atGsE(_M0L3arrS1571, _M0L1iS1578);
        #line 732 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1577, _M0L6_2atmpS3779);
        moonbit_decref(_M0L6_2atmpS3779);
        _M0L6_2atmpS3780 = _M0L1iS1578 + 1;
        _M0L1iS1578 = _M0L6_2atmpS3780;
        continue;
      } else {
        moonbit_decref(_M0L3arrS1571);
      }
      break;
    }
    return _M0L6resultS1577;
    join_1568:;
    _M0L6_2atmpS3778 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_4579
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4579)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_4579->$0 = _M0L6_2atmpS3778;
    _block_4579->$1 = 0;
    return _block_4579;
  }
}

int32_t _M0MP19moonbitDB8Database4llen(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1557,
  moonbit_string_t _M0L3keyS1558
) {
  #line 709 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 710 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1557, _M0L3keyS1558)
  ) {
    return 0;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L1dS1560;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3776 =
      _M0L4selfS1557->$0;
    void* _M0L7_2abindS1561;
    int32_t _result_4581;
    moonbit_incref(_M0L4dataS3776);
    #line 713 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1561
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3776, _M0L3keyS1558);
    moonbit_decref(_M0L4dataS3776);
    if (_M0L7_2abindS1561 == 0) {
      if (_M0L7_2abindS1561) {
        moonbit_decref(_M0L7_2abindS1561);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1562 = _M0L7_2abindS1561;
      void* _M0L4_2axS1563 = _M0L7_2aSomeS1562;
      switch (Moonbit_object_tag(_M0L4_2axS1563)) {
        case 2: {
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1564 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1563;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS4009 =
            _M0L7_2aListS1564->$0;
          int32_t _M0L6_2acntS4440 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1564));
          struct _M0TP19moonbitDB5Deque* _M0L4_2adS1565;
          if (_M0L6_2acntS4440 > 1) {
            int32_t _M0L11_2anew__cntS4441 = _M0L6_2acntS4440 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1564), _M0L11_2anew__cntS4441);
            moonbit_incref(_M0L8_2afieldS4009);
          } else if (_M0L6_2acntS4440 == 1) {
            #line 713 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1564);
          }
          _M0L4_2adS1565 = _M0L8_2afieldS4009;
          _M0L1dS1560 = _M0L4_2adS1565;
          goto join_1559;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1563);
          return 0;
          break;
        }
      }
    }
    join_1559:;
    #line 714 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4581 = _M0MP19moonbitDB5Deque6length(_M0L1dS1560);
    moonbit_decref(_M0L1dS1560);
    return _result_4581;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database4rpop(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1546,
  moonbit_string_t _M0L3keyS1547
) {
  #line 694 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 695 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1546, _M0L3keyS1547)
  ) {
    return 0;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L5dequeS1550;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3775 =
      _M0L4selfS1546->$0;
    void* _M0L7_2abindS1552;
    moonbit_string_t _M0L3valS1551;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3773;
    void* _M0L4ListS3774;
    moonbit_incref(_M0L4dataS3775);
    #line 698 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1552
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3775, _M0L3keyS1547);
    moonbit_decref(_M0L4dataS3775);
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
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1555 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1554;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS4012 =
            _M0L7_2aListS1555->$0;
          int32_t _M0L6_2acntS4442 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1555));
          struct _M0TP19moonbitDB5Deque* _M0L8_2adequeS1556;
          if (_M0L6_2acntS4442 > 1) {
            int32_t _M0L11_2anew__cntS4443 = _M0L6_2acntS4442 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1555), _M0L11_2anew__cntS4443);
            moonbit_incref(_M0L8_2afieldS4012);
          } else if (_M0L6_2acntS4442 == 1) {
            #line 698 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1555);
          }
          _M0L8_2adequeS1556 = _M0L8_2afieldS4012;
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
    #line 700 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3valS1551 = _M0MP19moonbitDB5Deque9pop__back(_M0L5dequeS1550);
    _M0L4dataS3773 = _M0L4selfS1546->$0;
    _M0L4ListS3774
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4List));
    Moonbit_object_header(_M0L4ListS3774)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 19, 2);
    ((struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4ListS3774)->$0
    = _M0L5dequeS1550;
    moonbit_incref(_M0L4dataS3773);
    #line 701 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3773, _M0L3keyS1547, _M0L4ListS3774);
    moonbit_decref(_M0L4dataS3773);
    moonbit_decref(_M0L4ListS3774);
    return _M0L3valS1551;
    join_1548:;
    return 0;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database4lpop(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1535,
  moonbit_string_t _M0L3keyS1536
) {
  #line 679 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 680 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1535, _M0L3keyS1536)
  ) {
    return 0;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L5dequeS1539;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3772 =
      _M0L4selfS1535->$0;
    void* _M0L7_2abindS1541;
    moonbit_string_t _M0L3valS1540;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3770;
    void* _M0L4ListS3771;
    moonbit_incref(_M0L4dataS3772);
    #line 683 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1541
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3772, _M0L3keyS1536);
    moonbit_decref(_M0L4dataS3772);
    if (_M0L7_2abindS1541 == 0) {
      if (_M0L7_2abindS1541) {
        moonbit_decref(_M0L7_2abindS1541);
      }
      goto join_1537;
    } else {
      void* _M0L7_2aSomeS1542 = _M0L7_2abindS1541;
      void* _M0L4_2axS1543 = _M0L7_2aSomeS1542;
      switch (Moonbit_object_tag(_M0L4_2axS1543)) {
        case 2: {
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1544 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1543;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS4015 =
            _M0L7_2aListS1544->$0;
          int32_t _M0L6_2acntS4444 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1544));
          struct _M0TP19moonbitDB5Deque* _M0L8_2adequeS1545;
          if (_M0L6_2acntS4444 > 1) {
            int32_t _M0L11_2anew__cntS4445 = _M0L6_2acntS4444 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1544), _M0L11_2anew__cntS4445);
            moonbit_incref(_M0L8_2afieldS4015);
          } else if (_M0L6_2acntS4444 == 1) {
            #line 683 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1544);
          }
          _M0L8_2adequeS1545 = _M0L8_2afieldS4015;
          _M0L5dequeS1539 = _M0L8_2adequeS1545;
          goto join_1538;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1543);
          goto join_1537;
          break;
        }
      }
    }
    join_1538:;
    #line 685 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3valS1540 = _M0MP19moonbitDB5Deque10pop__front(_M0L5dequeS1539);
    _M0L4dataS3770 = _M0L4selfS1535->$0;
    _M0L4ListS3771
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4List));
    Moonbit_object_header(_M0L4ListS3771)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 19, 2);
    ((struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4ListS3771)->$0
    = _M0L5dequeS1539;
    moonbit_incref(_M0L4dataS3770);
    #line 686 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3770, _M0L3keyS1536, _M0L4ListS3771);
    moonbit_decref(_M0L4dataS3770);
    moonbit_decref(_M0L4ListS3771);
    return _M0L3valS1540;
    join_1537:;
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database5rpush(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1523,
  moonbit_string_t _M0L3keyS1524,
  moonbit_string_t _M0L5valueS1534
) {
  int32_t _M0L6_2atmpS3766;
  struct _M0TP19moonbitDB5Deque* _M0L5dequeS1525;
  struct _M0TP19moonbitDB5Deque* _M0L1dS1528;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3769;
  void* _M0L7_2abindS1529;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3767;
  void* _M0L4ListS3768;
  int32_t _result_4588;
  #line 668 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 669 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3766
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1523, _M0L3keyS1524);
  _M0L4dataS3769 = _M0L4selfS1523->$0;
  moonbit_incref(_M0L4dataS3769);
  #line 670 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1529
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3769, _M0L3keyS1524);
  moonbit_decref(_M0L4dataS3769);
  if (_M0L7_2abindS1529 == 0) {
    if (_M0L7_2abindS1529) {
      moonbit_decref(_M0L7_2abindS1529);
    }
    goto join_1526;
  } else {
    void* _M0L7_2aSomeS1530 = _M0L7_2abindS1529;
    void* _M0L4_2axS1531 = _M0L7_2aSomeS1530;
    switch (Moonbit_object_tag(_M0L4_2axS1531)) {
      case 2: {
        struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1532 =
          (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1531;
        struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS4018 =
          _M0L7_2aListS1532->$0;
        int32_t _M0L6_2acntS4446 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1532));
        struct _M0TP19moonbitDB5Deque* _M0L4_2adS1533;
        if (_M0L6_2acntS4446 > 1) {
          int32_t _M0L11_2anew__cntS4447 = _M0L6_2acntS4446 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1532), _M0L11_2anew__cntS4447);
          moonbit_incref(_M0L8_2afieldS4018);
        } else if (_M0L6_2acntS4446 == 1) {
          #line 670 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aListS1532);
        }
        _M0L4_2adS1533 = _M0L8_2afieldS4018;
        _M0L1dS1528 = _M0L4_2adS1533;
        goto join_1527;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1531);
        goto join_1526;
        break;
      }
    }
  }
  goto joinlet_4587;
  join_1527:;
  _M0L5dequeS1525 = _M0L1dS1528;
  joinlet_4587:;
  goto joinlet_4586;
  join_1526:;
  #line 672 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5dequeS1525 = _M0MP19moonbitDB5Deque3new();
  joinlet_4586:;
  #line 674 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MP19moonbitDB5Deque10push__back(_M0L5dequeS1525, _M0L5valueS1534);
  _M0L4dataS3767 = _M0L4selfS1523->$0;
  moonbit_incref(_M0L5dequeS1525);
  _M0L4ListS3768
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4List));
  Moonbit_object_header(_M0L4ListS3768)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 19, 2);
  ((struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4ListS3768)->$0
  = _M0L5dequeS1525;
  moonbit_incref(_M0L4dataS3767);
  #line 675 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3767, _M0L3keyS1524, _M0L4ListS3768);
  moonbit_decref(_M0L4dataS3767);
  moonbit_decref(_M0L4ListS3768);
  #line 676 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_4588 = _M0MP19moonbitDB5Deque6length(_M0L5dequeS1525);
  moonbit_decref(_M0L5dequeS1525);
  return _result_4588;
}

int32_t _M0MP19moonbitDB8Database4hlen(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1514,
  moonbit_string_t _M0L3keyS1515
) {
  #line 646 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 647 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1514, _M0L3keyS1515)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1517;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3765 =
      _M0L4selfS1514->$0;
    void* _M0L7_2abindS1518;
    int32_t _result_4590;
    moonbit_incref(_M0L4dataS3765);
    #line 650 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1518
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3765, _M0L3keyS1515);
    moonbit_decref(_M0L4dataS3765);
    if (_M0L7_2abindS1518 == 0) {
      if (_M0L7_2abindS1518) {
        moonbit_decref(_M0L7_2abindS1518);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1519 = _M0L7_2abindS1518;
      void* _M0L4_2axS1520 = _M0L7_2aSomeS1519;
      switch (Moonbit_object_tag(_M0L4_2axS1520)) {
        case 1: {
          struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1521 =
            (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1520;
          struct _M0TPB3MapGssE* _M0L8_2afieldS4020 = _M0L7_2aHashS1521->$0;
          int32_t _M0L6_2acntS4448 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1521));
          struct _M0TPB3MapGssE* _M0L4_2ahS1522;
          if (_M0L6_2acntS4448 > 1) {
            int32_t _M0L11_2anew__cntS4449 = _M0L6_2acntS4448 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1521), _M0L11_2anew__cntS4449);
            moonbit_incref(_M0L8_2afieldS4020);
          } else if (_M0L6_2acntS4448 == 1) {
            #line 650 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aHashS1521);
          }
          _M0L4_2ahS1522 = _M0L8_2afieldS4020;
          _M0L1hS1517 = _M0L4_2ahS1522;
          goto join_1516;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1520);
          return 0;
          break;
        }
      }
    }
    join_1516:;
    #line 651 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4590 = _M0MPB3Map6lengthGssE(_M0L1hS1517);
    moonbit_decref(_M0L1hS1517);
    return _result_4590;
  }
}

struct _M0TPB3MapGssE* _M0MP19moonbitDB8Database7hgetall(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1502,
  moonbit_string_t _M0L3keyS1503
) {
  #line 635 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 636 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1502, _M0L3keyS1503)
  ) {
    struct _M0TUssE** _M0L7_2abindS1504 =
      (struct _M0TUssE**)moonbit_empty_ref_array;
    struct _M0TUssE** _M0L6_2atmpS3761 = _M0L7_2abindS1504;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3760 =
      (struct _M0TPB9ArrayViewGUssEE){.$0 = _M0L6_2atmpS3761,
                                        .$1 = 0,
                                        .$2 = 0};
    struct _M0TPB3MapGssE* _result_4591;
    #line 637 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4591 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3760, 0ll);
    moonbit_decref(_M0L6_2atmpS3760.$0);
    return _result_4591;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1508;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3764 =
      _M0L4selfS1502->$0;
    void* _M0L7_2abindS1509;
    struct _M0TUssE** _M0L7_2abindS1506;
    struct _M0TUssE** _M0L6_2atmpS3763;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3762;
    struct _M0TPB3MapGssE* _result_4594;
    moonbit_incref(_M0L4dataS3764);
    #line 639 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1509
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3764, _M0L3keyS1503);
    moonbit_decref(_M0L4dataS3764);
    if (_M0L7_2abindS1509 == 0) {
      if (_M0L7_2abindS1509) {
        moonbit_decref(_M0L7_2abindS1509);
      }
      goto join_1505;
    } else {
      void* _M0L7_2aSomeS1510 = _M0L7_2abindS1509;
      void* _M0L4_2axS1511 = _M0L7_2aSomeS1510;
      switch (Moonbit_object_tag(_M0L4_2axS1511)) {
        case 1: {
          struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1512 =
            (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1511;
          struct _M0TPB3MapGssE* _M0L8_2afieldS4022 = _M0L7_2aHashS1512->$0;
          int32_t _M0L6_2acntS4450 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1512));
          struct _M0TPB3MapGssE* _M0L4_2ahS1513;
          if (_M0L6_2acntS4450 > 1) {
            int32_t _M0L11_2anew__cntS4451 = _M0L6_2acntS4450 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1512), _M0L11_2anew__cntS4451);
            moonbit_incref(_M0L8_2afieldS4022);
          } else if (_M0L6_2acntS4450 == 1) {
            #line 639 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aHashS1512);
          }
          _M0L4_2ahS1513 = _M0L8_2afieldS4022;
          _M0L1hS1508 = _M0L4_2ahS1513;
          goto join_1507;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1511);
          goto join_1505;
          break;
        }
      }
    }
    join_1507:;
    return _M0L1hS1508;
    join_1505:;
    _M0L7_2abindS1506 = (struct _M0TUssE**)moonbit_empty_ref_array;
    _M0L6_2atmpS3763 = _M0L7_2abindS1506;
    _M0L6_2atmpS3762
    = (struct _M0TPB9ArrayViewGUssEE){
      .$0 = _M0L6_2atmpS3763, .$1 = 0, .$2 = 0
    };
    #line 641 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4594 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3762, 0ll);
    moonbit_decref(_M0L6_2atmpS3762.$0);
    return _result_4594;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database4hget(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1491,
  moonbit_string_t _M0L3keyS1492,
  moonbit_string_t _M0L5fieldS1496
) {
  #line 606 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 607 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1491, _M0L3keyS1492)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1495;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3759 =
      _M0L4selfS1491->$0;
    void* _M0L7_2abindS1497;
    moonbit_string_t _result_4597;
    moonbit_incref(_M0L4dataS3759);
    #line 610 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1497
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3759, _M0L3keyS1492);
    moonbit_decref(_M0L4dataS3759);
    if (_M0L7_2abindS1497 == 0) {
      if (_M0L7_2abindS1497) {
        moonbit_decref(_M0L7_2abindS1497);
      }
      goto join_1493;
    } else {
      void* _M0L7_2aSomeS1498 = _M0L7_2abindS1497;
      void* _M0L4_2axS1499 = _M0L7_2aSomeS1498;
      switch (Moonbit_object_tag(_M0L4_2axS1499)) {
        case 1: {
          struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1500 =
            (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1499;
          struct _M0TPB3MapGssE* _M0L8_2afieldS4024 = _M0L7_2aHashS1500->$0;
          int32_t _M0L6_2acntS4452 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1500));
          struct _M0TPB3MapGssE* _M0L4_2ahS1501;
          if (_M0L6_2acntS4452 > 1) {
            int32_t _M0L11_2anew__cntS4453 = _M0L6_2acntS4452 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1500), _M0L11_2anew__cntS4453);
            moonbit_incref(_M0L8_2afieldS4024);
          } else if (_M0L6_2acntS4452 == 1) {
            #line 610 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aHashS1500);
          }
          _M0L4_2ahS1501 = _M0L8_2afieldS4024;
          _M0L1hS1495 = _M0L4_2ahS1501;
          goto join_1494;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1499);
          goto join_1493;
          break;
        }
      }
    }
    join_1494:;
    #line 611 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4597 = _M0MPB3Map3getGssE(_M0L1hS1495, _M0L5fieldS1496);
    moonbit_decref(_M0L1hS1495);
    return _result_4597;
    join_1493:;
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database4hset(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1477,
  moonbit_string_t _M0L3keyS1478,
  moonbit_string_t _M0L5fieldS1489,
  moonbit_string_t _M0L5valueS1490
) {
  int32_t _M0L6_2atmpS3753;
  struct _M0TPB3MapGssE* _M0L4hashS1479;
  struct _M0TPB3MapGssE* _M0L1hS1483;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3758;
  void* _M0L7_2abindS1484;
  struct _M0TUssE** _M0L7_2abindS1481;
  struct _M0TUssE** _M0L6_2atmpS3757;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3756;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3754;
  void* _M0L4HashS3755;
  #line 596 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 597 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3753
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1477, _M0L3keyS1478);
  _M0L4dataS3758 = _M0L4selfS1477->$0;
  moonbit_incref(_M0L4dataS3758);
  #line 598 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1484
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3758, _M0L3keyS1478);
  moonbit_decref(_M0L4dataS3758);
  if (_M0L7_2abindS1484 == 0) {
    if (_M0L7_2abindS1484) {
      moonbit_decref(_M0L7_2abindS1484);
    }
    goto join_1480;
  } else {
    void* _M0L7_2aSomeS1485 = _M0L7_2abindS1484;
    void* _M0L4_2axS1486 = _M0L7_2aSomeS1485;
    switch (Moonbit_object_tag(_M0L4_2axS1486)) {
      case 1: {
        struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1487 =
          (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1486;
        struct _M0TPB3MapGssE* _M0L8_2afieldS4027 = _M0L7_2aHashS1487->$0;
        int32_t _M0L6_2acntS4454 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1487));
        struct _M0TPB3MapGssE* _M0L4_2ahS1488;
        if (_M0L6_2acntS4454 > 1) {
          int32_t _M0L11_2anew__cntS4455 = _M0L6_2acntS4454 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1487), _M0L11_2anew__cntS4455);
          moonbit_incref(_M0L8_2afieldS4027);
        } else if (_M0L6_2acntS4454 == 1) {
          #line 598 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aHashS1487);
        }
        _M0L4_2ahS1488 = _M0L8_2afieldS4027;
        _M0L1hS1483 = _M0L4_2ahS1488;
        goto join_1482;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1486);
        goto join_1480;
        break;
      }
    }
  }
  goto joinlet_4599;
  join_1482:;
  _M0L4hashS1479 = _M0L1hS1483;
  joinlet_4599:;
  goto joinlet_4598;
  join_1480:;
  _M0L7_2abindS1481 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3757 = _M0L7_2abindS1481;
  _M0L6_2atmpS3756
  = (struct _M0TPB9ArrayViewGUssEE){
    .$0 = _M0L6_2atmpS3757, .$1 = 0, .$2 = 0
  };
  #line 600 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4hashS1479 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3756, 10ll);
  moonbit_decref(_M0L6_2atmpS3756.$0);
  joinlet_4598:;
  #line 602 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGssE(_M0L4hashS1479, _M0L5fieldS1489, _M0L5valueS1490);
  _M0L4dataS3754 = _M0L4selfS1477->$0;
  _M0L4HashS3755
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4Hash));
  Moonbit_object_header(_M0L4HashS3755)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 22, 1);
  ((struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4HashS3755)->$0
  = _M0L4hashS1479;
  moonbit_incref(_M0L4dataS3754);
  #line 603 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3754, _M0L3keyS1478, _M0L4HashS3755);
  moonbit_decref(_M0L4dataS3754);
  moonbit_decref(_M0L4HashS3755);
  return 0;
}

int64_t _M0MP19moonbitDB8Database4decr(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1461,
  moonbit_string_t _M0L3keyS1462
) {
  int32_t _M0L6_2atmpS3746;
  moonbit_string_t _M0L1sS1465;
  moonbit_string_t _M0L7currentS1463;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3752;
  void* _M0L7_2abindS1466;
  int32_t _M0L1nS1472;
  int64_t _M0L7_2abindS1474;
  int32_t _M0L6_2atmpS3751;
  moonbit_string_t _M0L8new__valS1473;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3747;
  void* _M0L6StringS3748;
  struct _M0TPB3MapGsiE* _M0L7expiresS3749;
  int32_t _M0L6_2atmpS3750;
  #line 579 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 580 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3746
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1461, _M0L3keyS1462);
  _M0L4dataS3752 = _M0L4selfS1461->$0;
  moonbit_incref(_M0L4dataS3752);
  #line 581 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1466
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3752, _M0L3keyS1462);
  moonbit_decref(_M0L4dataS3752);
  if (_M0L7_2abindS1466 == 0) {
    if (_M0L7_2abindS1466) {
      moonbit_decref(_M0L7_2abindS1466);
    }
    _M0L7currentS1463 = (moonbit_string_t)moonbit_string_literal_85.data;
  } else {
    void* _M0L7_2aSomeS1467 = _M0L7_2abindS1466;
    void* _M0L4_2axS1468 = _M0L7_2aSomeS1467;
    switch (Moonbit_object_tag(_M0L4_2axS1468)) {
      case 0: {
        struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1469 =
          (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1468;
        moonbit_string_t _M0L8_2afieldS4031 = _M0L9_2aStringS1469->$0;
        int32_t _M0L6_2acntS4456 =
          Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1469));
        moonbit_string_t _M0L4_2asS1470;
        if (_M0L6_2acntS4456 > 1) {
          int32_t _M0L11_2anew__cntS4457 = _M0L6_2acntS4456 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1469), _M0L11_2anew__cntS4457);
          moonbit_incref(_M0L8_2afieldS4031);
        } else if (_M0L6_2acntS4456 == 1) {
          #line 581 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L9_2aStringS1469);
        }
        _M0L4_2asS1470 = _M0L8_2afieldS4031;
        _M0L1sS1465 = _M0L4_2asS1470;
        goto join_1464;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1468);
        _M0L7currentS1463 = (moonbit_string_t)moonbit_string_literal_85.data;
        break;
      }
    }
  }
  goto joinlet_4600;
  join_1464:;
  _M0L7currentS1463 = _M0L1sS1465;
  joinlet_4600:;
  #line 585 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1474 = _M0FP19moonbitDB10parse__int(_M0L7currentS1463);
  moonbit_decref(_M0L7currentS1463);
  if (_M0L7_2abindS1474 == 4294967296ll) {
    return 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS1475 = _M0L7_2abindS1474;
    int32_t _M0L4_2anS1476 = (int32_t)_M0L7_2aSomeS1475;
    _M0L1nS1472 = _M0L4_2anS1476;
    goto join_1471;
  }
  join_1471:;
  _M0L6_2atmpS3751 = _M0L1nS1472 - 1;
  #line 587 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L8new__valS1473 = _M0FP19moonbitDB15int__to__string(_M0L6_2atmpS3751);
  _M0L4dataS3747 = _M0L4selfS1461->$0;
  _M0L6StringS3748
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3748)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 25, 0);
  ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS3748)->$0
  = _M0L8new__valS1473;
  moonbit_incref(_M0L4dataS3747);
  #line 588 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3747, _M0L3keyS1462, _M0L6StringS3748);
  moonbit_decref(_M0L4dataS3747);
  moonbit_decref(_M0L6StringS3748);
  _M0L7expiresS3749 = _M0L4selfS1461->$1;
  moonbit_incref(_M0L7expiresS3749);
  #line 589 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3749, _M0L3keyS1462);
  moonbit_decref(_M0L7expiresS3749);
  _M0L6_2atmpS3750 = _M0L1nS1472 - 1;
  return (int64_t)_M0L6_2atmpS3750;
}

int64_t _M0MP19moonbitDB8Database4incr(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1445,
  moonbit_string_t _M0L3keyS1446
) {
  int32_t _M0L6_2atmpS3739;
  moonbit_string_t _M0L1sS1449;
  moonbit_string_t _M0L7currentS1447;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3745;
  void* _M0L7_2abindS1450;
  int32_t _M0L1nS1456;
  int64_t _M0L7_2abindS1458;
  int32_t _M0L6_2atmpS3744;
  moonbit_string_t _M0L8new__valS1457;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3740;
  void* _M0L6StringS3741;
  struct _M0TPB3MapGsiE* _M0L7expiresS3742;
  int32_t _M0L6_2atmpS3743;
  #line 562 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 563 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3739
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1445, _M0L3keyS1446);
  _M0L4dataS3745 = _M0L4selfS1445->$0;
  moonbit_incref(_M0L4dataS3745);
  #line 564 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1450
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3745, _M0L3keyS1446);
  moonbit_decref(_M0L4dataS3745);
  if (_M0L7_2abindS1450 == 0) {
    if (_M0L7_2abindS1450) {
      moonbit_decref(_M0L7_2abindS1450);
    }
    _M0L7currentS1447 = (moonbit_string_t)moonbit_string_literal_85.data;
  } else {
    void* _M0L7_2aSomeS1451 = _M0L7_2abindS1450;
    void* _M0L4_2axS1452 = _M0L7_2aSomeS1451;
    switch (Moonbit_object_tag(_M0L4_2axS1452)) {
      case 0: {
        struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1453 =
          (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1452;
        moonbit_string_t _M0L8_2afieldS4035 = _M0L9_2aStringS1453->$0;
        int32_t _M0L6_2acntS4458 =
          Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1453));
        moonbit_string_t _M0L4_2asS1454;
        if (_M0L6_2acntS4458 > 1) {
          int32_t _M0L11_2anew__cntS4459 = _M0L6_2acntS4458 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1453), _M0L11_2anew__cntS4459);
          moonbit_incref(_M0L8_2afieldS4035);
        } else if (_M0L6_2acntS4458 == 1) {
          #line 564 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L9_2aStringS1453);
        }
        _M0L4_2asS1454 = _M0L8_2afieldS4035;
        _M0L1sS1449 = _M0L4_2asS1454;
        goto join_1448;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1452);
        _M0L7currentS1447 = (moonbit_string_t)moonbit_string_literal_85.data;
        break;
      }
    }
  }
  goto joinlet_4602;
  join_1448:;
  _M0L7currentS1447 = _M0L1sS1449;
  joinlet_4602:;
  #line 568 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1458 = _M0FP19moonbitDB10parse__int(_M0L7currentS1447);
  moonbit_decref(_M0L7currentS1447);
  if (_M0L7_2abindS1458 == 4294967296ll) {
    return 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS1459 = _M0L7_2abindS1458;
    int32_t _M0L4_2anS1460 = (int32_t)_M0L7_2aSomeS1459;
    _M0L1nS1456 = _M0L4_2anS1460;
    goto join_1455;
  }
  join_1455:;
  _M0L6_2atmpS3744 = _M0L1nS1456 + 1;
  #line 570 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L8new__valS1457 = _M0FP19moonbitDB15int__to__string(_M0L6_2atmpS3744);
  _M0L4dataS3740 = _M0L4selfS1445->$0;
  _M0L6StringS3741
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3741)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 25, 0);
  ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS3741)->$0
  = _M0L8new__valS1457;
  moonbit_incref(_M0L4dataS3740);
  #line 571 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3740, _M0L3keyS1446, _M0L6StringS3741);
  moonbit_decref(_M0L4dataS3740);
  moonbit_decref(_M0L6StringS3741);
  _M0L7expiresS3742 = _M0L4selfS1445->$1;
  moonbit_incref(_M0L7expiresS3742);
  #line 572 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3742, _M0L3keyS1446);
  moonbit_decref(_M0L7expiresS3742);
  _M0L6_2atmpS3743 = _M0L1nS1456 + 1;
  return (int64_t)_M0L6_2atmpS3743;
}

moonbit_string_t _M0FP19moonbitDB15int__to__string(int32_t _M0L1nS1436) {
  #line 526 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (_M0L1nS1436 == 0) {
    return (moonbit_string_t)moonbit_string_literal_85.data;
  } else {
    struct _M0TPB8MutLocalGiE* _M0L3numS1437 =
      (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    moonbit_string_t* _M0L6_2atmpS3738;
    struct _M0TPB5ArrayGsE* _M0L5charsS1438;
    int32_t _M0L3valS3724;
    moonbit_string_t* _M0L6_2atmpS3737;
    struct _M0TPB5ArrayGsE* _M0L6resultS1441;
    int32_t _M0L6_2atmpS3734;
    int32_t _M0L6_2atmpS3733;
    int32_t _M0L1iS1442;
    moonbit_string_t _M0L7_2abindS1444;
    int32_t _M0L6_2atmpS3736;
    struct _M0TPC16string10StringView _M0L6_2atmpS3735;
    moonbit_string_t _result_4606;
    Moonbit_object_header(_M0L3numS1437)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L3numS1437->$0 = _M0L1nS1436;
    _M0L6_2atmpS3738 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L5charsS1438
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L5charsS1438)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _M0L5charsS1438->$0 = _M0L6_2atmpS3738;
    _M0L5charsS1438->$1 = 0;
    _M0L3valS3724 = _M0L3numS1437->$0;
    if (_M0L3valS3724 < 0) {
      int32_t _M0L3valS3726 = _M0L3numS1437->$0;
      int32_t _M0L6_2atmpS3725 = -_M0L3valS3726;
      _M0L3numS1437->$0 = _M0L6_2atmpS3725;
    }
    while (1) {
      int32_t _M0L3valS3727 = _M0L3numS1437->$0;
      if (_M0L3valS3727 > 0) {
        int32_t _M0L3valS3728 = _M0L3numS1437->$0;
        int32_t _M0L7_2abindS1439 = _M0L3valS3728 % 10;
        int32_t _M0L3valS3730;
        int32_t _M0L6_2atmpS3729;
        switch (_M0L7_2abindS1439) {
          case 0: {
            #line 537 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1438, (moonbit_string_t)moonbit_string_literal_85.data);
            break;
          }
          
          case 1: {
            #line 538 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1438, (moonbit_string_t)moonbit_string_literal_86.data);
            break;
          }
          
          case 2: {
            #line 539 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1438, (moonbit_string_t)moonbit_string_literal_87.data);
            break;
          }
          
          case 3: {
            #line 540 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1438, (moonbit_string_t)moonbit_string_literal_88.data);
            break;
          }
          
          case 4: {
            #line 541 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1438, (moonbit_string_t)moonbit_string_literal_89.data);
            break;
          }
          
          case 5: {
            #line 542 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1438, (moonbit_string_t)moonbit_string_literal_90.data);
            break;
          }
          
          case 6: {
            #line 543 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1438, (moonbit_string_t)moonbit_string_literal_91.data);
            break;
          }
          
          case 7: {
            #line 544 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1438, (moonbit_string_t)moonbit_string_literal_92.data);
            break;
          }
          
          case 8: {
            #line 545 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1438, (moonbit_string_t)moonbit_string_literal_93.data);
            break;
          }
          
          case 9: {
            #line 546 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1438, (moonbit_string_t)moonbit_string_literal_94.data);
            break;
          }
          default: {
            #line 547 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1438, (moonbit_string_t)moonbit_string_literal_85.data);
            break;
          }
        }
        _M0L3valS3730 = _M0L3numS1437->$0;
        _M0L6_2atmpS3729 = _M0L3valS3730 / 10;
        _M0L3numS1437->$0 = _M0L6_2atmpS3729;
        continue;
      } else {
        moonbit_decref(_M0L3numS1437);
      }
      break;
    }
    if (_M0L1nS1436 < 0) {
      #line 552 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L5charsS1438, (moonbit_string_t)moonbit_string_literal_95.data);
    }
    _M0L6_2atmpS3737 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1441
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1441)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _M0L6resultS1441->$0 = _M0L6_2atmpS3737;
    _M0L6resultS1441->$1 = 0;
    #line 555 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3734 = _M0MPC15array5Array6lengthGsE(_M0L5charsS1438);
    _M0L6_2atmpS3733 = _M0L6_2atmpS3734 - 1;
    _M0L1iS1442 = _M0L6_2atmpS3733;
    while (1) {
      if (_M0L1iS1442 >= 0) {
        moonbit_string_t _M0L6_2atmpS3731;
        int32_t _M0L6_2atmpS3732;
        #line 556 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3731
        = _M0MPC15array5Array2atGsE(_M0L5charsS1438, _M0L1iS1442);
        #line 556 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1441, _M0L6_2atmpS3731);
        moonbit_decref(_M0L6_2atmpS3731);
        _M0L6_2atmpS3732 = _M0L1iS1442 - 1;
        _M0L1iS1442 = _M0L6_2atmpS3732;
        continue;
      } else {
        moonbit_decref(_M0L5charsS1438);
      }
      break;
    }
    _M0L7_2abindS1444 = (moonbit_string_t)moonbit_string_literal_96.data;
    _M0L6_2atmpS3736 = Moonbit_array_length(_M0L7_2abindS1444);
    _M0L6_2atmpS3735
    = (struct _M0TPC16string10StringView){
      .$0 = _M0L7_2abindS1444, .$1 = 0, .$2 = _M0L6_2atmpS3736
    };
    #line 558 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4606
    = _M0MPC15array5Array4joinGsE(_M0L6resultS1441, _M0L6_2atmpS3735);
    moonbit_decref(_M0L6resultS1441);
    moonbit_decref(_M0L6_2atmpS3735.$0);
    return _result_4606;
  }
}

int64_t _M0FP19moonbitDB10parse__int(moonbit_string_t _M0L1sS1425) {
  int32_t _M0L6_2atmpS3711;
  #line 503 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3711 = Moonbit_array_length(_M0L1sS1425);
  if (_M0L6_2atmpS3711 == 0) {
    return 4294967296ll;
  } else {
    struct _M0TPB8MutLocalGiE* _M0L6resultS1426 =
      (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    struct _M0TPB8MutLocalGiE* _M0L4signS1427;
    struct _M0TPB8MutLocalGiE* _M0L5startS1428;
    int32_t _M0L6_2atmpS3712;
    int32_t _M0L3valS3720;
    int32_t _M0L1iS1429;
    int32_t _M0L3valS3722;
    int32_t _M0L3valS3723;
    int32_t _M0L6_2atmpS3721;
    Moonbit_object_header(_M0L6resultS1426)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L6resultS1426->$0 = 0;
    _M0L4signS1427
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L4signS1427)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L4signS1427->$0 = 1;
    _M0L5startS1428
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L5startS1428)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L5startS1428->$0 = 0;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1425)) {
      #line 510 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3712 = _M0L1sS1425[0];
    if (_M0L6_2atmpS3712 == 45) {
      _M0L4signS1427->$0 = -1;
      _M0L5startS1428->$0 = 1;
    } else {
      int32_t _M0L6_2atmpS3713;
      if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1425)) {
        #line 513 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3713 = _M0L1sS1425[0];
      if (_M0L6_2atmpS3713 == 43) {
        _M0L5startS1428->$0 = 1;
      }
    }
    _M0L3valS3720 = _M0L5startS1428->$0;
    moonbit_decref(_M0L5startS1428);
    _M0L1iS1429 = _M0L3valS3720;
    while (1) {
      int32_t _M0L6_2atmpS3714 = Moonbit_array_length(_M0L1sS1425);
      if (_M0L1iS1429 < _M0L6_2atmpS3714) {
        int32_t _M0L5digitS1431;
        int32_t _M0L6_2atmpS3718;
        int64_t _M0L7_2abindS1432;
        int32_t _M0L3valS3717;
        int32_t _M0L6_2atmpS3716;
        int32_t _M0L6_2atmpS3715;
        int32_t _M0L6_2atmpS3719;
        if (
          _M0L1iS1429 < 0 || _M0L1iS1429 >= Moonbit_array_length(_M0L1sS1425)
        ) {
          #line 517 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3718 = _M0L1sS1425[_M0L1iS1429];
        #line 517 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L7_2abindS1432
        = _M0FP19moonbitDB17uint16__to__digit(_M0L6_2atmpS3718);
        if (_M0L7_2abindS1432 == 4294967296ll) {
          moonbit_decref(_M0L4signS1427);
          moonbit_decref(_M0L6resultS1426);
          return 4294967296ll;
        } else {
          int64_t _M0L7_2aSomeS1433 = _M0L7_2abindS1432;
          int32_t _M0L8_2adigitS1434 = (int32_t)_M0L7_2aSomeS1433;
          _M0L5digitS1431 = _M0L8_2adigitS1434;
          goto join_1430;
        }
        goto joinlet_4608;
        join_1430:;
        _M0L3valS3717 = _M0L6resultS1426->$0;
        _M0L6_2atmpS3716 = _M0L3valS3717 * 10;
        _M0L6_2atmpS3715 = _M0L6_2atmpS3716 + _M0L5digitS1431;
        _M0L6resultS1426->$0 = _M0L6_2atmpS3715;
        joinlet_4608:;
        _M0L6_2atmpS3719 = _M0L1iS1429 + 1;
        _M0L1iS1429 = _M0L6_2atmpS3719;
        continue;
      }
      break;
    }
    _M0L3valS3722 = _M0L6resultS1426->$0;
    moonbit_decref(_M0L6resultS1426);
    _M0L3valS3723 = _M0L4signS1427->$0;
    moonbit_decref(_M0L4signS1427);
    _M0L6_2atmpS3721 = _M0L3valS3722 * _M0L3valS3723;
    return (int64_t)_M0L6_2atmpS3721;
  }
}

int64_t _M0FP19moonbitDB17uint16__to__digit(int32_t _M0L1cS1424) {
  #line 471 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  switch (_M0L1cS1424) {
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
  struct _M0TP19moonbitDB8Database* _M0L4selfS1415,
  moonbit_string_t _M0L3keyS1416
) {
  #line 460 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 461 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1415, _M0L3keyS1416)
  ) {
    return 0;
  } else {
    moonbit_string_t _M0L1sS1418;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3710 =
      _M0L4selfS1415->$0;
    void* _M0L7_2abindS1419;
    int32_t _result_4610;
    moonbit_incref(_M0L4dataS3710);
    #line 464 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1419
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3710, _M0L3keyS1416);
    moonbit_decref(_M0L4dataS3710);
    if (_M0L7_2abindS1419 == 0) {
      if (_M0L7_2abindS1419) {
        moonbit_decref(_M0L7_2abindS1419);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1420 = _M0L7_2abindS1419;
      void* _M0L4_2axS1421 = _M0L7_2aSomeS1420;
      switch (Moonbit_object_tag(_M0L4_2axS1421)) {
        case 0: {
          struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1422 =
            (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1421;
          moonbit_string_t _M0L8_2afieldS4037 = _M0L9_2aStringS1422->$0;
          int32_t _M0L6_2acntS4460 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1422));
          moonbit_string_t _M0L4_2asS1423;
          if (_M0L6_2acntS4460 > 1) {
            int32_t _M0L11_2anew__cntS4461 = _M0L6_2acntS4460 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1422), _M0L11_2anew__cntS4461);
            moonbit_incref(_M0L8_2afieldS4037);
          } else if (_M0L6_2acntS4460 == 1) {
            #line 464 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L9_2aStringS1422);
          }
          _M0L4_2asS1423 = _M0L8_2afieldS4037;
          _M0L1sS1418 = _M0L4_2asS1423;
          goto join_1417;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1421);
          return 0;
          break;
        }
      }
    }
    join_1417:;
    _result_4610 = Moonbit_array_length(_M0L1sS1418);
    moonbit_decref(_M0L1sS1418);
    return _result_4610;
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database4keys(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1407
) {
  moonbit_string_t* _M0L6_2atmpS3709;
  struct _M0TPB5ArrayGsE* _M0L6resultS1405;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3708;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L5_2aitS1406;
  #line 377 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3709 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1405
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1405)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6resultS1405->$0 = _M0L6_2atmpS3709;
  _M0L6resultS1405->$1 = 0;
  _M0L4dataS3708 = _M0L4selfS1407->$0;
  moonbit_incref(_M0L4dataS3708);
  #line 378 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1406
  = _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(_M0L4dataS3708);
  moonbit_decref(_M0L4dataS3708);
  while (1) {
    moonbit_string_t _M0L3keyS1409;
    struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2abindS1411;
    int32_t _M0L6_2atmpS3707;
    #line 379 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1411
    = _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(_M0L5_2aitS1406);
    if (_M0L7_2abindS1411 == 0) {
      if (_M0L7_2abindS1411) {
        moonbit_decref(_M0L7_2abindS1411);
      }
      moonbit_decref(_M0L5_2aitS1406);
    } else {
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1412 =
        _M0L7_2abindS1411;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L4_2axS1413 =
        _M0L7_2aSomeS1412;
      moonbit_string_t _M0L8_2afieldS4039 = _M0L4_2axS1413->$0;
      int32_t _M0L6_2acntS4462 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1413));
      moonbit_string_t _M0L6_2akeyS1414;
      if (_M0L6_2acntS4462 > 1) {
        int32_t _M0L11_2anew__cntS4464 = _M0L6_2acntS4462 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1413), _M0L11_2anew__cntS4464);
        moonbit_incref(_M0L8_2afieldS4039);
      } else if (_M0L6_2acntS4462 == 1) {
        void* _M0L8_2afieldS4463 = _M0L4_2axS1413->$1;
        moonbit_decref(_M0L8_2afieldS4463);
        #line 379 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1413);
      }
      _M0L6_2akeyS1414 = _M0L8_2afieldS4039;
      _M0L3keyS1409 = _M0L6_2akeyS1414;
      goto join_1408;
    }
    goto joinlet_4612;
    join_1408:;
    #line 380 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3707
    = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1407, _M0L3keyS1409);
    if (!_M0L6_2atmpS3707) {
      #line 381 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1405, _M0L3keyS1409);
      moonbit_decref(_M0L3keyS1409);
    } else {
      moonbit_decref(_M0L3keyS1409);
    }
    continue;
    joinlet_4612:;
    break;
  }
  return _M0L6resultS1405;
}

int32_t _M0MP19moonbitDB8Database6exists(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1403,
  moonbit_string_t _M0L3keyS1404
) {
  #line 369 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 370 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1403, _M0L3keyS1404)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3706 =
      _M0L4selfS1403->$0;
    int32_t _result_4613;
    moonbit_incref(_M0L4dataS3706);
    #line 373 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4613
    = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS3706, _M0L3keyS1404);
    moonbit_decref(_M0L4dataS3706);
    return _result_4613;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database3get(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1393,
  moonbit_string_t _M0L3keyS1394
) {
  #line 345 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 346 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1393, _M0L3keyS1394)
  ) {
    return 0;
  } else {
    moonbit_string_t _M0L1sS1397;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3705 =
      _M0L4selfS1393->$0;
    void* _M0L7_2abindS1398;
    moonbit_incref(_M0L4dataS3705);
    #line 349 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1398
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3705, _M0L3keyS1394);
    moonbit_decref(_M0L4dataS3705);
    if (_M0L7_2abindS1398 == 0) {
      if (_M0L7_2abindS1398) {
        moonbit_decref(_M0L7_2abindS1398);
      }
      goto join_1395;
    } else {
      void* _M0L7_2aSomeS1399 = _M0L7_2abindS1398;
      void* _M0L4_2axS1400 = _M0L7_2aSomeS1399;
      switch (Moonbit_object_tag(_M0L4_2axS1400)) {
        case 0: {
          struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1401 =
            (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1400;
          moonbit_string_t _M0L8_2afieldS4042 = _M0L9_2aStringS1401->$0;
          int32_t _M0L6_2acntS4465 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1401));
          moonbit_string_t _M0L4_2asS1402;
          if (_M0L6_2acntS4465 > 1) {
            int32_t _M0L11_2anew__cntS4466 = _M0L6_2acntS4465 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1401), _M0L11_2anew__cntS4466);
            moonbit_incref(_M0L8_2afieldS4042);
          } else if (_M0L6_2acntS4465 == 1) {
            #line 349 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L9_2aStringS1401);
          }
          _M0L4_2asS1402 = _M0L8_2afieldS4042;
          _M0L1sS1397 = _M0L4_2asS1402;
          goto join_1396;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1400);
          goto join_1395;
          break;
        }
      }
    }
    join_1396:;
    return _M0L1sS1397;
    join_1395:;
    return 0;
  }
}

struct _M0TPB5ArrayGOsE* _M0MP19moonbitDB8Database4mget(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1383,
  struct _M0TPB5ArrayGsE* _M0L4keysS1380
) {
  moonbit_string_t* _M0L6_2atmpS3704;
  struct _M0TPB5ArrayGOsE* _M0L6resultS1378;
  int32_t _M0L7_2abindS1379;
  int32_t _M0L2__S1381;
  #line 276 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3704 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1378
  = (struct _M0TPB5ArrayGOsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGOsE));
  Moonbit_object_header(_M0L6resultS1378)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 28, 0);
  _M0L6resultS1378->$0 = _M0L6_2atmpS3704;
  _M0L6resultS1378->$1 = 0;
  _M0L7_2abindS1379 = _M0L4keysS1380->$1;
  _M0L2__S1381 = 0;
  while (1) {
    if (_M0L2__S1381 < _M0L7_2abindS1379) {
      moonbit_string_t* _M0L3bufS3703 = _M0L4keysS1380->$0;
      moonbit_string_t _M0L3keyS1382 =
        (moonbit_string_t)_M0L3bufS3703[_M0L2__S1381];
      int32_t _M0L6_2atmpS3702;
      moonbit_incref(_M0L3keyS1382);
      #line 279 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      if (
        _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1383, _M0L3keyS1382)
      ) {
        moonbit_string_t _M0L6_2atmpS3698;
        moonbit_decref(_M0L3keyS1382);
        _M0L6_2atmpS3698 = 0;
        #line 280 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1378, _M0L6_2atmpS3698);
        if (_M0L6_2atmpS3698) {
          moonbit_decref(_M0L6_2atmpS3698);
        }
      } else {
        moonbit_string_t _M0L1sS1386;
        struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3701 =
          _M0L4selfS1383->$0;
        void* _M0L7_2abindS1387;
        moonbit_string_t _M0L6_2atmpS3700;
        moonbit_string_t _M0L6_2atmpS3699;
        moonbit_incref(_M0L4dataS3701);
        #line 282 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L7_2abindS1387
        = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3701, _M0L3keyS1382);
        moonbit_decref(_M0L4dataS3701);
        moonbit_decref(_M0L3keyS1382);
        if (_M0L7_2abindS1387 == 0) {
          if (_M0L7_2abindS1387) {
            moonbit_decref(_M0L7_2abindS1387);
          }
          goto join_1384;
        } else {
          void* _M0L7_2aSomeS1388 = _M0L7_2abindS1387;
          void* _M0L4_2axS1389 = _M0L7_2aSomeS1388;
          switch (Moonbit_object_tag(_M0L4_2axS1389)) {
            case 0: {
              struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1390 =
                (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1389;
              moonbit_string_t _M0L8_2afieldS4044 = _M0L9_2aStringS1390->$0;
              int32_t _M0L6_2acntS4467 =
                Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1390));
              moonbit_string_t _M0L4_2asS1391;
              if (_M0L6_2acntS4467 > 1) {
                int32_t _M0L11_2anew__cntS4468 = _M0L6_2acntS4467 - 1;
                Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1390), _M0L11_2anew__cntS4468);
                moonbit_incref(_M0L8_2afieldS4044);
              } else if (_M0L6_2acntS4467 == 1) {
                #line 282 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
                moonbit_free(_M0L9_2aStringS1390);
              }
              _M0L4_2asS1391 = _M0L8_2afieldS4044;
              _M0L1sS1386 = _M0L4_2asS1391;
              goto join_1385;
              break;
            }
            default: {
              moonbit_decref(_M0L4_2axS1389);
              goto join_1384;
              break;
            }
          }
        }
        goto joinlet_4618;
        join_1385:;
        _M0L6_2atmpS3700 = _M0L1sS1386;
        #line 283 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1378, _M0L6_2atmpS3700);
        if (_M0L6_2atmpS3700) {
          moonbit_decref(_M0L6_2atmpS3700);
        }
        joinlet_4618:;
        goto joinlet_4617;
        join_1384:;
        _M0L6_2atmpS3699 = 0;
        #line 284 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1378, _M0L6_2atmpS3699);
        if (_M0L6_2atmpS3699) {
          moonbit_decref(_M0L6_2atmpS3699);
        }
        joinlet_4617:;
      }
      _M0L6_2atmpS3702 = _M0L2__S1381 + 1;
      _M0L2__S1381 = _M0L6_2atmpS3702;
      continue;
    }
    break;
  }
  return _M0L6resultS1378;
}

int32_t _M0MP19moonbitDB8Database4mset(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1376,
  struct _M0TPB5ArrayGsE* _M0L4keysS1374,
  struct _M0TPB5ArrayGsE* _M0L6valuesS1375
) {
  int32_t _M0L1iS1373;
  #line 269 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L1iS1373 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3690;
    int32_t _if__result_4620;
    #line 270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3690 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1374);
    if (_M0L1iS1373 < _M0L6_2atmpS3690) {
      int32_t _M0L6_2atmpS3689;
      #line 270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3689 = _M0MPC15array5Array6lengthGsE(_M0L6valuesS1375);
      _if__result_4620 = _M0L1iS1373 < _M0L6_2atmpS3689;
    } else {
      _if__result_4620 = 0;
    }
    if (_if__result_4620) {
      struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3691 =
        _M0L4selfS1376->$0;
      moonbit_string_t _M0L6_2atmpS3692;
      moonbit_string_t _M0L6_2atmpS3694;
      void* _M0L6StringS3693;
      struct _M0TPB3MapGsiE* _M0L7expiresS3695;
      moonbit_string_t _M0L6_2atmpS3696;
      int32_t _M0L6_2atmpS3697;
      moonbit_incref(_M0L4dataS3691);
      #line 271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3692
      = _M0MPC15array5Array2atGsE(_M0L4keysS1374, _M0L1iS1373);
      #line 271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3694
      = _M0MPC15array5Array2atGsE(_M0L6valuesS1375, _M0L1iS1373);
      _M0L6StringS3693
      = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
      Moonbit_object_header(_M0L6StringS3693)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 25, 0);
      ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS3693)->$0
      = _M0L6_2atmpS3694;
      #line 271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3691, _M0L6_2atmpS3692, _M0L6StringS3693);
      moonbit_decref(_M0L4dataS3691);
      moonbit_decref(_M0L6_2atmpS3692);
      moonbit_decref(_M0L6StringS3693);
      _M0L7expiresS3695 = _M0L4selfS1376->$1;
      moonbit_incref(_M0L7expiresS3695);
      #line 272 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3696
      = _M0MPC15array5Array2atGsE(_M0L4keysS1374, _M0L1iS1373);
      #line 272 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map6removeGsiE(_M0L7expiresS3695, _M0L6_2atmpS3696);
      moonbit_decref(_M0L7expiresS3695);
      moonbit_decref(_M0L6_2atmpS3696);
      _M0L6_2atmpS3697 = _M0L1iS1373 + 1;
      _M0L1iS1373 = _M0L6_2atmpS3697;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MP19moonbitDB8Database3ttl(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1370,
  moonbit_string_t _M0L3keyS1371
) {
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3681;
  int32_t _M0L6_2atmpS3680;
  #line 226 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4dataS3681 = _M0L4selfS1370->$0;
  moonbit_incref(_M0L4dataS3681);
  #line 227 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3680
  = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS3681, _M0L3keyS1371);
  moonbit_decref(_M0L4dataS3681);
  if (!_M0L6_2atmpS3680) {
    return -2;
  } else {
    struct _M0TPB3MapGsiE* _M0L7expiresS3682 = _M0L4selfS1370->$1;
    int32_t _result_4621;
    moonbit_incref(_M0L7expiresS3682);
    #line 229 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4621 = _M0MPB3Map8containsGsiE(_M0L7expiresS3682, _M0L3keyS1371);
    moonbit_decref(_M0L7expiresS3682);
    if (_result_4621) {
      struct _M0TPB3MapGsiE* _M0L7expiresS3688 = _M0L4selfS1370->$1;
      int64_t _M0L6_2atmpS3687;
      int32_t _M0L6_2atmpS3685;
      int32_t _M0L13current__timeS3686;
      int32_t _M0L9remainingS1372;
      moonbit_incref(_M0L7expiresS3688);
      #line 230 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3687 = _M0MPB3Map3getGsiE(_M0L7expiresS3688, _M0L3keyS1371);
      moonbit_decref(_M0L7expiresS3688);
      #line 230 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3685 = _M0MPC16option6Option6unwrapGiE(_M0L6_2atmpS3687);
      _M0L13current__timeS3686 = _M0L4selfS1370->$2;
      _M0L9remainingS1372 = _M0L6_2atmpS3685 - _M0L13current__timeS3686;
      if (_M0L9remainingS1372 <= 0) {
        struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3683 =
          _M0L4selfS1370->$0;
        struct _M0TPB3MapGsiE* _M0L7expiresS3684;
        moonbit_incref(_M0L4dataS3683);
        #line 232 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(_M0L4dataS3683, _M0L3keyS1371);
        moonbit_decref(_M0L4dataS3683);
        _M0L7expiresS3684 = _M0L4selfS1370->$1;
        moonbit_incref(_M0L7expiresS3684);
        #line 233 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPB3Map6removeGsiE(_M0L7expiresS3684, _M0L3keyS1371);
        moonbit_decref(_M0L7expiresS3684);
        return -2;
      } else {
        return _M0L9remainingS1372 / 1000;
      }
    } else {
      return -1;
    }
  }
}

int32_t _M0MP19moonbitDB8Database6expire(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1367,
  moonbit_string_t _M0L3keyS1368,
  int32_t _M0L7secondsS1369
) {
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3675;
  int32_t _result_4622;
  #line 208 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4dataS3675 = _M0L4selfS1367->$0;
  moonbit_incref(_M0L4dataS3675);
  #line 209 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_4622
  = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS3675, _M0L3keyS1368);
  moonbit_decref(_M0L4dataS3675);
  if (_result_4622) {
    struct _M0TPB3MapGsiE* _M0L7expiresS3676 = _M0L4selfS1367->$1;
    int32_t _M0L13current__timeS3678 = _M0L4selfS1367->$2;
    int32_t _M0L6_2atmpS3679 = _M0L7secondsS1369 * 1000;
    int32_t _M0L6_2atmpS3677 = _M0L13current__timeS3678 + _M0L6_2atmpS3679;
    moonbit_incref(_M0L7expiresS3676);
    #line 210 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsiE(_M0L7expiresS3676, _M0L3keyS1368, _M0L6_2atmpS3677);
    moonbit_decref(_M0L7expiresS3676);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database3set(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1364,
  moonbit_string_t _M0L3keyS1365,
  moonbit_string_t _M0L5valueS1366
) {
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3672;
  void* _M0L6StringS3673;
  struct _M0TPB3MapGsiE* _M0L7expiresS3674;
  #line 203 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4dataS3672 = _M0L4selfS1364->$0;
  moonbit_incref(_M0L5valueS1366);
  _M0L6StringS3673
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3673)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 25, 0);
  ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS3673)->$0
  = _M0L5valueS1366;
  moonbit_incref(_M0L4dataS3672);
  #line 204 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3672, _M0L3keyS1365, _M0L6StringS3673);
  moonbit_decref(_M0L4dataS3672);
  moonbit_decref(_M0L6StringS3673);
  _M0L7expiresS3674 = _M0L4selfS1364->$1;
  moonbit_incref(_M0L7expiresS3674);
  #line 205 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3674, _M0L3keyS1365);
  moonbit_decref(_M0L7expiresS3674);
  return 0;
}

int32_t _M0MP19moonbitDB8Database14check__expired(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1359,
  moonbit_string_t _M0L3keyS1360
) {
  int32_t _M0L12expire__timeS1358;
  struct _M0TPB3MapGsiE* _M0L7expiresS3671;
  int64_t _M0L7_2abindS1361;
  int32_t _M0L13current__timeS3668;
  #line 188 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7expiresS3671 = _M0L4selfS1359->$1;
  moonbit_incref(_M0L7expiresS3671);
  #line 189 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1361 = _M0MPB3Map3getGsiE(_M0L7expiresS3671, _M0L3keyS1360);
  moonbit_decref(_M0L7expiresS3671);
  if (_M0L7_2abindS1361 == 4294967296ll) {
    return 0;
  } else {
    int64_t _M0L7_2aSomeS1362 = _M0L7_2abindS1361;
    int32_t _M0L15_2aexpire__timeS1363 = (int32_t)_M0L7_2aSomeS1362;
    _M0L12expire__timeS1358 = _M0L15_2aexpire__timeS1363;
    goto join_1357;
  }
  join_1357:;
  _M0L13current__timeS3668 = _M0L4selfS1359->$2;
  if (_M0L12expire__timeS1358 <= _M0L13current__timeS3668) {
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3669 =
      _M0L4selfS1359->$0;
    struct _M0TPB3MapGsiE* _M0L7expiresS3670;
    moonbit_incref(_M0L4dataS3669);
    #line 192 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(_M0L4dataS3669, _M0L3keyS1360);
    moonbit_decref(_M0L4dataS3669);
    _M0L7expiresS3670 = _M0L4selfS1359->$1;
    moonbit_incref(_M0L7expiresS3670);
    #line 193 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map6removeGsiE(_M0L7expiresS3670, _M0L3keyS1360);
    moonbit_decref(_M0L7expiresS3670);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database13advance__time(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1355,
  int32_t _M0L2msS1356
) {
  int32_t _M0L13current__timeS3667;
  int32_t _M0L6_2atmpS3666;
  #line 184 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L13current__timeS3667 = _M0L4selfS1355->$2;
  _M0L6_2atmpS3666 = _M0L13current__timeS3667 + _M0L2msS1356;
  _M0L4selfS1355->$2 = _M0L6_2atmpS3666;
  return 0;
}

struct _M0TP19moonbitDB8Database* _M0MP19moonbitDB8Database3new() {
  struct _M0TUsRP19moonbitDB10RedisValueE** _M0L7_2abindS1353;
  struct _M0TUsRP19moonbitDB10RedisValueE** _M0L6_2atmpS3665;
  struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE _M0L6_2atmpS3664;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3660;
  struct _M0TUsiE** _M0L7_2abindS1354;
  struct _M0TUsiE** _M0L6_2atmpS3663;
  struct _M0TPB9ArrayViewGUsiEE _M0L6_2atmpS3662;
  struct _M0TPB3MapGsiE* _M0L6_2atmpS3661;
  struct _M0TP19moonbitDB8Database* _block_4624;
  #line 176 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1353
  = (struct _M0TUsRP19moonbitDB10RedisValueE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3665 = _M0L7_2abindS1353;
  _M0L6_2atmpS3664
  = (struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE){
    .$0 = _M0L6_2atmpS3665, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3660
  = _M0MPB3Map3MapGsRP19moonbitDB10RedisValueE(_M0L6_2atmpS3664, 1000ll);
  moonbit_decref(_M0L6_2atmpS3664.$0);
  _M0L7_2abindS1354 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3663 = _M0L7_2abindS1354;
  _M0L6_2atmpS3662
  = (struct _M0TPB9ArrayViewGUsiEE){
    .$0 = _M0L6_2atmpS3663, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3661 = _M0MPB3Map3MapGsiE(_M0L6_2atmpS3662, 1000ll);
  moonbit_decref(_M0L6_2atmpS3662.$0);
  _block_4624
  = (struct _M0TP19moonbitDB8Database*)moonbit_malloc(sizeof(struct _M0TP19moonbitDB8Database));
  Moonbit_object_header(_block_4624)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 31, 0);
  _block_4624->$0 = _M0L6_2atmpS3660;
  _block_4624->$1 = _M0L6_2atmpS3661;
  _block_4624->$2 = 0;
  return _block_4624;
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB5Deque9to__array(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1346
) {
  moonbit_string_t* _M0L6_2atmpS3659;
  struct _M0TPB5ArrayGsE* _M0L6resultS1344;
  struct _M0TPB5ArrayGsE* _M0L5frontS3656;
  int32_t _M0L6_2atmpS3655;
  int32_t _M0L6_2atmpS3654;
  int32_t _M0L1iS1345;
  struct _M0TPB5ArrayGsE* _M0L7_2abindS1348;
  int32_t _M0L7_2abindS1349;
  int32_t _M0L2__S1350;
  #line 48 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3659 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1344
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1344)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6resultS1344->$0 = _M0L6_2atmpS3659;
  _M0L6resultS1344->$1 = 0;
  _M0L5frontS3656 = _M0L4selfS1346->$0;
  moonbit_incref(_M0L5frontS3656);
  #line 50 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3655 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3656);
  moonbit_decref(_M0L5frontS3656);
  _M0L6_2atmpS3654 = _M0L6_2atmpS3655 - 1;
  _M0L1iS1345 = _M0L6_2atmpS3654;
  while (1) {
    if (_M0L1iS1345 >= 0) {
      struct _M0TPB5ArrayGsE* _M0L5frontS3652 = _M0L4selfS1346->$0;
      moonbit_string_t _M0L6_2atmpS3651;
      int32_t _M0L6_2atmpS3653;
      moonbit_incref(_M0L5frontS3652);
      #line 51 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3651
      = _M0MPC15array5Array2atGsE(_M0L5frontS3652, _M0L1iS1345);
      moonbit_decref(_M0L5frontS3652);
      #line 51 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1344, _M0L6_2atmpS3651);
      moonbit_decref(_M0L6_2atmpS3651);
      _M0L6_2atmpS3653 = _M0L1iS1345 - 1;
      _M0L1iS1345 = _M0L6_2atmpS3653;
      continue;
    }
    break;
  }
  _M0L7_2abindS1348 = _M0L4selfS1346->$1;
  _M0L7_2abindS1349 = _M0L7_2abindS1348->$1;
  moonbit_incref(_M0L7_2abindS1348);
  _M0L2__S1350 = 0;
  while (1) {
    if (_M0L2__S1350 < _M0L7_2abindS1349) {
      moonbit_string_t* _M0L3bufS3658 = _M0L7_2abindS1348->$0;
      moonbit_string_t _M0L4itemS1351 =
        (moonbit_string_t)_M0L3bufS3658[_M0L2__S1350];
      int32_t _M0L6_2atmpS3657;
      moonbit_incref(_M0L4itemS1351);
      #line 54 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1344, _M0L4itemS1351);
      moonbit_decref(_M0L4itemS1351);
      _M0L6_2atmpS3657 = _M0L2__S1350 + 1;
      _M0L2__S1350 = _M0L6_2atmpS3657;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1348);
    }
    break;
  }
  return _M0L6resultS1344;
}

int32_t _M0MP19moonbitDB5Deque6length(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1343
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3650;
  int32_t _M0L6_2atmpS3647;
  struct _M0TPB5ArrayGsE* _M0L4backS3649;
  int32_t _M0L6_2atmpS3648;
  #line 44 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5frontS3650 = _M0L4selfS1343->$0;
  moonbit_incref(_M0L5frontS3650);
  #line 45 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3647 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3650);
  moonbit_decref(_M0L5frontS3650);
  _M0L4backS3649 = _M0L4selfS1343->$1;
  moonbit_incref(_M0L4backS3649);
  #line 45 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3648 = _M0MPC15array5Array6lengthGsE(_M0L4backS3649);
  moonbit_decref(_M0L4backS3649);
  return _M0L6_2atmpS3647 + _M0L6_2atmpS3648;
}

moonbit_string_t _M0MP19moonbitDB5Deque9pop__back(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1336
) {
  struct _M0TPB5ArrayGsE* _M0L4backS3641;
  int32_t _M0L6_2atmpS3640;
  struct _M0TPB5ArrayGsE* _M0L4backS3646;
  moonbit_string_t _result_4629;
  #line 31 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4backS3641 = _M0L4selfS1336->$1;
  moonbit_incref(_M0L4backS3641);
  #line 32 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3640 = _M0MPC15array5Array6lengthGsE(_M0L4backS3641);
  moonbit_decref(_M0L4backS3641);
  if (_M0L6_2atmpS3640 == 0) {
    while (1) {
      struct _M0TPB5ArrayGsE* _M0L5frontS3643 = _M0L4selfS1336->$0;
      int32_t _M0L6_2atmpS3642;
      moonbit_incref(_M0L5frontS3643);
      #line 33 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3642 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3643);
      moonbit_decref(_M0L5frontS3643);
      if (_M0L6_2atmpS3642 > 0) {
        struct _M0TPB5ArrayGsE* _M0L5frontS3645 = _M0L4selfS1336->$0;
        moonbit_string_t _M0L4itemS1337;
        moonbit_string_t _M0L1vS1339;
        struct _M0TPB5ArrayGsE* _M0L4backS3644;
        moonbit_incref(_M0L5frontS3645);
        #line 34 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L4itemS1337 = _M0MPC15array5Array3popGsE(_M0L5frontS3645);
        moonbit_decref(_M0L5frontS3645);
        if (_M0L4itemS1337 == 0) {
          if (_M0L4itemS1337) {
            moonbit_decref(_M0L4itemS1337);
          }
          break;
        } else {
          moonbit_string_t _M0L7_2aSomeS1340 = _M0L4itemS1337;
          moonbit_string_t _M0L4_2avS1341 = _M0L7_2aSomeS1340;
          _M0L1vS1339 = _M0L4_2avS1341;
          goto join_1338;
        }
        goto joinlet_4628;
        join_1338:;
        _M0L4backS3644 = _M0L4selfS1336->$1;
        moonbit_incref(_M0L4backS3644);
        #line 36 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L4backS3644, _M0L1vS1339);
        moonbit_decref(_M0L4backS3644);
        moonbit_decref(_M0L1vS1339);
        joinlet_4628:;
        continue;
      }
      break;
    }
  }
  _M0L4backS3646 = _M0L4selfS1336->$1;
  moonbit_incref(_M0L4backS3646);
  #line 41 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_4629 = _M0MPC15array5Array3popGsE(_M0L4backS3646);
  moonbit_decref(_M0L4backS3646);
  return _result_4629;
}

moonbit_string_t _M0MP19moonbitDB5Deque10pop__front(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1329
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3634;
  int32_t _M0L6_2atmpS3633;
  struct _M0TPB5ArrayGsE* _M0L5frontS3639;
  moonbit_string_t _result_4632;
  #line 18 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5frontS3634 = _M0L4selfS1329->$0;
  moonbit_incref(_M0L5frontS3634);
  #line 19 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3633 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3634);
  moonbit_decref(_M0L5frontS3634);
  if (_M0L6_2atmpS3633 == 0) {
    while (1) {
      struct _M0TPB5ArrayGsE* _M0L4backS3636 = _M0L4selfS1329->$1;
      int32_t _M0L6_2atmpS3635;
      moonbit_incref(_M0L4backS3636);
      #line 20 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3635 = _M0MPC15array5Array6lengthGsE(_M0L4backS3636);
      moonbit_decref(_M0L4backS3636);
      if (_M0L6_2atmpS3635 > 0) {
        struct _M0TPB5ArrayGsE* _M0L4backS3638 = _M0L4selfS1329->$1;
        moonbit_string_t _M0L4itemS1330;
        moonbit_string_t _M0L1vS1332;
        struct _M0TPB5ArrayGsE* _M0L5frontS3637;
        moonbit_incref(_M0L4backS3638);
        #line 21 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L4itemS1330 = _M0MPC15array5Array3popGsE(_M0L4backS3638);
        moonbit_decref(_M0L4backS3638);
        if (_M0L4itemS1330 == 0) {
          if (_M0L4itemS1330) {
            moonbit_decref(_M0L4itemS1330);
          }
          break;
        } else {
          moonbit_string_t _M0L7_2aSomeS1333 = _M0L4itemS1330;
          moonbit_string_t _M0L4_2avS1334 = _M0L7_2aSomeS1333;
          _M0L1vS1332 = _M0L4_2avS1334;
          goto join_1331;
        }
        goto joinlet_4631;
        join_1331:;
        _M0L5frontS3637 = _M0L4selfS1329->$0;
        moonbit_incref(_M0L5frontS3637);
        #line 23 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L5frontS3637, _M0L1vS1332);
        moonbit_decref(_M0L5frontS3637);
        moonbit_decref(_M0L1vS1332);
        joinlet_4631:;
        continue;
      }
      break;
    }
  }
  _M0L5frontS3639 = _M0L4selfS1329->$0;
  moonbit_incref(_M0L5frontS3639);
  #line 28 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_4632 = _M0MPC15array5Array3popGsE(_M0L5frontS3639);
  moonbit_decref(_M0L5frontS3639);
  return _result_4632;
}

int32_t _M0MP19moonbitDB5Deque10push__back(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1327,
  moonbit_string_t _M0L5valueS1328
) {
  struct _M0TPB5ArrayGsE* _M0L4backS3632;
  #line 14 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4backS3632 = _M0L4selfS1327->$1;
  moonbit_incref(_M0L4backS3632);
  #line 15 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPC15array5Array4pushGsE(_M0L4backS3632, _M0L5valueS1328);
  moonbit_decref(_M0L4backS3632);
  return 0;
}

struct _M0TP19moonbitDB5Deque* _M0MP19moonbitDB5Deque3new() {
  moonbit_string_t* _M0L6_2atmpS3631;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3628;
  moonbit_string_t* _M0L6_2atmpS3630;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3629;
  struct _M0TP19moonbitDB5Deque* _block_4633;
  #line 6 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3631 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3628
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3628)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS3628->$0 = _M0L6_2atmpS3631;
  _M0L6_2atmpS3628->$1 = 0;
  _M0L6_2atmpS3630 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3629
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3629)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS3629->$0 = _M0L6_2atmpS3630;
  _M0L6_2atmpS3629->$1 = 0;
  _block_4633
  = (struct _M0TP19moonbitDB5Deque*)moonbit_malloc(sizeof(struct _M0TP19moonbitDB5Deque));
  Moonbit_object_header(_block_4633)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 35, 0);
  _block_4633->$0 = _M0L6_2atmpS3628;
  _block_4633->$1 = _M0L6_2atmpS3629;
  return _block_4633;
}

moonbit_string_t _M0IPC15float5FloatPB4Show10to__string(float _M0L4selfS1326) {
  double _M0L6_2atmpS3627;
  #line 16 "/home/developer/.moon/lib/core/float/methods.mbt"
  _M0L6_2atmpS3627 = (double)_M0L4selfS1326;
  #line 17 "/home/developer/.moon/lib/core/float/methods.mbt"
  return _M0MPC16double6Double10to__string(_M0L6_2atmpS3627);
}

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1324,
  struct _M0TPC16string10StringView _M0L9separatorS1325
) {
  moonbit_string_t* _M0L3bufS3625;
  int32_t _M0L3lenS3626;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS3624;
  moonbit_string_t _result_4634;
  #line 2184 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3bufS3625 = _M0L4selfS1324->$0;
  _M0L3lenS3626 = _M0L4selfS1324->$1;
  moonbit_incref(_M0L3bufS3625);
  _M0L6_2atmpS3624
  = (struct _M0TPB9ArrayViewGsE){
    .$0 = _M0L3bufS3625, .$1 = 0, .$2 = _M0L3lenS3626
  };
  #line 2188 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _result_4634
  = _M0MPC15array9ArrayView4joinGsE(_M0L6_2atmpS3624, _M0L9separatorS1325);
  moonbit_decref(_M0L6_2atmpS3624.$0);
  return _result_4634;
}

moonbit_string_t _M0MPC15array5Array3popGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1321
) {
  int32_t _M0L3lenS1320;
  #line 325 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1320 = _M0L4selfS1321->$1;
  if (_M0L3lenS1320 == 0) {
    return 0;
  } else {
    int32_t _M0L5indexS1322 = _M0L3lenS1320 - 1;
    moonbit_string_t* _M0L3bufS3623 = _M0L4selfS1321->$0;
    moonbit_string_t _M0L1vS1323 =
      (moonbit_string_t)_M0L3bufS3623[_M0L5indexS1322];
    moonbit_string_t* _M0L3bufS3622 = _M0L4selfS1321->$0;
    moonbit_string_t _M0L6_2aoldS4081;
    if (
      _M0L5indexS1322 < 0
      || _M0L5indexS1322 >= Moonbit_array_length(_M0L3bufS3622)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4081 = (moonbit_string_t)_M0L3bufS3622[_M0L5indexS1322];
    moonbit_incref(_M0L1vS1323);
    moonbit_decref(_M0L6_2aoldS4081);
    if (
      _M0L5indexS1322 < 0
      || _M0L5indexS1322 >= Moonbit_array_length(_M0L3bufS3622)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS3622[_M0L5indexS1322]
    = (moonbit_string_t)moonbit_string_literal_96.data;
    _M0L4selfS1321->$1 = _M0L5indexS1322;
    return _M0L1vS1323;
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1309,
  int32_t _M0L5indexS1310
) {
  int32_t _M0L3lenS1308;
  int32_t _if__result_4635;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1308 = _M0L4selfS1309->$1;
  if (_M0L5indexS1310 >= 0) {
    _if__result_4635 = _M0L5indexS1310 < _M0L3lenS1308;
  } else {
    _if__result_4635 = 0;
  }
  if (_if__result_4635) {
    moonbit_string_t* _M0L6_2atmpS3618;
    moonbit_string_t _M0L6_2atmpS4085;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3618 = _M0MPC15array5Array6bufferGsE(_M0L4selfS1309);
    _M0L6_2atmpS4085 = (moonbit_string_t)_M0L6_2atmpS3618[_M0L5indexS1310];
    moonbit_incref(_M0L6_2atmpS4085);
    moonbit_decref(_M0L6_2atmpS3618);
    return _M0L6_2atmpS4085;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t _M0MPC15array5Array2atGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS1312,
  int32_t _M0L5indexS1313
) {
  int32_t _M0L3lenS1311;
  int32_t _if__result_4636;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1311 = _M0L4selfS1312->$1;
  if (_M0L5indexS1313 >= 0) {
    _if__result_4636 = _M0L5indexS1313 < _M0L3lenS1311;
  } else {
    _if__result_4636 = 0;
  }
  if (_if__result_4636) {
    moonbit_string_t* _M0L6_2atmpS3619;
    moonbit_string_t _M0L6_2atmpS4086;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3619 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS1312);
    _M0L6_2atmpS4086 = (moonbit_string_t)_M0L6_2atmpS3619[_M0L5indexS1313];
    if (_M0L6_2atmpS4086) {
      moonbit_incref(_M0L6_2atmpS4086);
    }
    moonbit_decref(_M0L6_2atmpS3619);
    return _M0L6_2atmpS4086;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

struct _M0TP39moonbitDB8examples11leaderboard6Player* _M0MPC15array5Array2atGRP39moonbitDB8examples11leaderboard6PlayerE(
  struct _M0TPB5ArrayGRP39moonbitDB8examples11leaderboard6PlayerE* _M0L4selfS1315,
  int32_t _M0L5indexS1316
) {
  int32_t _M0L3lenS1314;
  int32_t _if__result_4637;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1314 = _M0L4selfS1315->$1;
  if (_M0L5indexS1316 >= 0) {
    _if__result_4637 = _M0L5indexS1316 < _M0L3lenS1314;
  } else {
    _if__result_4637 = 0;
  }
  if (_if__result_4637) {
    struct _M0TP39moonbitDB8examples11leaderboard6Player** _M0L6_2atmpS3620;
    struct _M0TP39moonbitDB8examples11leaderboard6Player* _M0L6_2atmpS4087;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3620
    = _M0MPC15array5Array6bufferGRP39moonbitDB8examples11leaderboard6PlayerE(_M0L4selfS1315);
    _M0L6_2atmpS4087
    = (struct _M0TP39moonbitDB8examples11leaderboard6Player*)_M0L6_2atmpS3620[
        _M0L5indexS1316
      ];
    if (_M0L6_2atmpS4087) {
      moonbit_incref(_M0L6_2atmpS4087);
    }
    moonbit_decref(_M0L6_2atmpS3620);
    return _M0L6_2atmpS4087;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

struct _M0TUsfE* _M0MPC15array5Array2atGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS1318,
  int32_t _M0L5indexS1319
) {
  int32_t _M0L3lenS1317;
  int32_t _if__result_4638;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1317 = _M0L4selfS1318->$1;
  if (_M0L5indexS1319 >= 0) {
    _if__result_4638 = _M0L5indexS1319 < _M0L3lenS1317;
  } else {
    _if__result_4638 = 0;
  }
  if (_if__result_4638) {
    struct _M0TUsfE** _M0L6_2atmpS3621;
    struct _M0TUsfE* _M0L6_2atmpS4088;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3621 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS1318);
    _M0L6_2atmpS4088 = (struct _M0TUsfE*)_M0L6_2atmpS3621[_M0L5indexS1319];
    if (_M0L6_2atmpS4088) {
      moonbit_incref(_M0L6_2atmpS4088);
    }
    moonbit_decref(_M0L6_2atmpS3621);
    return _M0L6_2atmpS4088;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1307) {
  moonbit_string_t _M0L6_2atmpS3617;
  #line 36 "/home/developer/.moon/lib/core/builtin/console.mbt"
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS3617
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1307);
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS3617);
  moonbit_decref(_M0L6_2atmpS3617);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1306) {
  #line 282 "/home/developer/.moon/lib/core/builtin/double.mbt"
  #line 284 "/home/developer/.moon/lib/core/builtin/double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1306);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1293) {
  uint64_t _M0L4bitsS1294;
  uint64_t _M0L6_2atmpS3616;
  uint64_t _M0L6_2atmpS3615;
  int32_t _M0L8ieeeSignS1295;
  uint64_t _M0L12ieeeMantissaS1296;
  uint64_t _M0L6_2atmpS3614;
  uint64_t _M0L6_2atmpS3613;
  int32_t _M0L12ieeeExponentS1297;
  int32_t _if__result_4639;
  struct _M0TPB17FloatingDecimal64* _M0L7_2abindS1298;
  struct _M0TPB17FloatingDecimal64* _M0L1vS1299;
  moonbit_string_t _result_4641;
  #line 659 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L3valS1293 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_85.data;
  }
  _M0L4bitsS1294 = *(int64_t*)&_M0L3valS1293;
  _M0L6_2atmpS3616 = _M0L4bitsS1294 >> 63;
  _M0L6_2atmpS3615 = _M0L6_2atmpS3616 & 1ull;
  _M0L8ieeeSignS1295 = _M0L6_2atmpS3615 != 0ull;
  _M0L12ieeeMantissaS1296 = _M0L4bitsS1294 & 4503599627370495ull;
  _M0L6_2atmpS3614 = _M0L4bitsS1294 >> 52;
  _M0L6_2atmpS3613 = _M0L6_2atmpS3614 & 2047ull;
  _M0L12ieeeExponentS1297 = (int32_t)_M0L6_2atmpS3613;
  if (_M0L12ieeeExponentS1297 == 2047) {
    _if__result_4639 = 1;
  } else if (_M0L12ieeeExponentS1297 == 0) {
    _if__result_4639 = _M0L12ieeeMantissaS1296 == 0ull;
  } else {
    _if__result_4639 = 0;
  }
  if (_if__result_4639) {
    int32_t _M0L6_2atmpS3604 = _M0L12ieeeExponentS1297 != 0;
    int32_t _M0L6_2atmpS3605 = _M0L12ieeeMantissaS1296 != 0ull;
    #line 676 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1295, _M0L6_2atmpS3604, _M0L6_2atmpS3605);
  }
  #line 678 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1298
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1296, _M0L12ieeeExponentS1297);
  if (_M0L7_2abindS1298 == 0) {
    uint32_t _M0L6_2atmpS3606;
    if (_M0L7_2abindS1298) {
      moonbit_decref(_M0L7_2abindS1298);
    }
    _M0L6_2atmpS3606 = *(uint32_t*)&_M0L12ieeeExponentS1297;
    #line 688 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L1vS1299 = _M0FPB3d2d(_M0L12ieeeMantissaS1296, _M0L6_2atmpS3606);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1300 = _M0L7_2abindS1298;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1301 = _M0L7_2aSomeS1300;
    struct _M0TPB17FloatingDecimal64* _M0L1xS1302 = _M0L4_2afS1301;
    while (1) {
      uint64_t _M0L8mantissaS3612 = _M0L1xS1302->$0;
      uint64_t _M0L1qS1303 = _M0L8mantissaS3612 / 10ull;
      uint64_t _M0L8mantissaS3610 = _M0L1xS1302->$0;
      uint64_t _M0L6_2atmpS3611 = 10ull * _M0L1qS1303;
      uint64_t _M0L1rS1304 = _M0L8mantissaS3610 - _M0L6_2atmpS3611;
      int32_t _M0L8exponentS3609;
      int32_t _M0L6_2atmpS3608;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3607;
      if (_M0L1rS1304 != 0ull) {
        _M0L1vS1299 = _M0L1xS1302;
        break;
      }
      _M0L8exponentS3609 = _M0L1xS1302->$1;
      moonbit_decref(_M0L1xS1302);
      _M0L6_2atmpS3608 = _M0L8exponentS3609 + 1;
      _M0L6_2atmpS3607
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0L6_2atmpS3607)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L6_2atmpS3607->$0 = _M0L1qS1303;
      _M0L6_2atmpS3607->$1 = _M0L6_2atmpS3608;
      _M0L1xS1302 = _M0L6_2atmpS3607;
      continue;
      break;
    }
  }
  #line 690 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_4641 = _M0FPB9to__chars(_M0L1vS1299, _M0L8ieeeSignS1295);
  moonbit_decref(_M0L1vS1299);
  return _result_4641;
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1288,
  int32_t _M0L12ieeeExponentS1290
) {
  uint64_t _M0L2m2S1287;
  int32_t _M0L6_2atmpS3603;
  int32_t _M0L2e2S1289;
  int32_t _M0L6_2atmpS3602;
  uint64_t _M0L6_2atmpS3601;
  uint64_t _M0L4maskS1291;
  uint64_t _M0L8fractionS1292;
  int32_t _M0L6_2atmpS3600;
  uint64_t _M0L6_2atmpS3599;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3598;
  #line 637 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2m2S1287 = 4503599627370496ull | _M0L12ieeeMantissaS1288;
  _M0L6_2atmpS3603 = _M0L12ieeeExponentS1290 - 1023;
  _M0L2e2S1289 = _M0L6_2atmpS3603 - 52;
  if (_M0L2e2S1289 > 0) {
    return 0;
  }
  if (_M0L2e2S1289 < -52) {
    return 0;
  }
  _M0L6_2atmpS3602 = -_M0L2e2S1289;
  _M0L6_2atmpS3601 = 1ull << (_M0L6_2atmpS3602 & 63);
  _M0L4maskS1291 = _M0L6_2atmpS3601 - 1ull;
  _M0L8fractionS1292 = _M0L2m2S1287 & _M0L4maskS1291;
  if (_M0L8fractionS1292 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3600 = -_M0L2e2S1289;
  _M0L6_2atmpS3599 = _M0L2m2S1287 >> (_M0L6_2atmpS3600 & 63);
  _M0L6_2atmpS3598
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS3598)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6_2atmpS3598->$0 = _M0L6_2atmpS3599;
  _M0L6_2atmpS3598->$1 = 0;
  return _M0L6_2atmpS3598;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1255,
  int32_t _M0L4signS1253
) {
  int32_t _M0L6_2atmpS3597;
  moonbit_bytes_t _M0L6resultS1251;
  int32_t _M0Lm5indexS1252;
  uint64_t _M0L6outputS1254;
  int32_t _M0L7olengthS1256;
  int32_t _M0L8exponentS3596;
  int32_t _M0L6_2atmpS3595;
  int32_t _M0Lm3expS1257;
  int32_t _M0L6_2atmpS3594;
  int32_t _M0L6_2atmpS3592;
  int32_t _M0L18scientificNotationS1258;
  #line 530 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 532 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3597 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1251
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS3597);
  _M0Lm5indexS1252 = 0;
  if (_M0L4signS1253) {
    int32_t _M0L6_2atmpS3466 = _M0Lm5indexS1252;
    int32_t _M0L6_2atmpS3467;
    if (
      _M0L6_2atmpS3466 < 0
      || _M0L6_2atmpS3466 >= Moonbit_array_length(_M0L6resultS1251)
    ) {
      #line 535 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1251[_M0L6_2atmpS3466] = 45;
    _M0L6_2atmpS3467 = _M0Lm5indexS1252;
    _M0Lm5indexS1252 = _M0L6_2atmpS3467 + 1;
  }
  _M0L6outputS1254 = _M0L1vS1255->$0;
  #line 539 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7olengthS1256 = _M0FPB17decimal__length17(_M0L6outputS1254);
  _M0L8exponentS3596 = _M0L1vS1255->$1;
  _M0L6_2atmpS3595 = _M0L8exponentS3596 + _M0L7olengthS1256;
  _M0Lm3expS1257 = _M0L6_2atmpS3595 - 1;
  _M0L6_2atmpS3594 = _M0Lm3expS1257;
  if (_M0L6_2atmpS3594 >= -6) {
    int32_t _M0L6_2atmpS3593 = _M0Lm3expS1257;
    _M0L6_2atmpS3592 = _M0L6_2atmpS3593 < 21;
  } else {
    _M0L6_2atmpS3592 = 0;
  }
  _M0L18scientificNotationS1258 = !_M0L6_2atmpS3592;
  if (_M0L18scientificNotationS1258) {
    int32_t _M0L7_2abindS1259 = _M0L7olengthS1256 - 1;
    uint64_t _M0L6outputS1260;
    int32_t _M0L1iS1261 = 0;
    uint64_t _M0L6outputS1262 = _M0L6outputS1254;
    int32_t _M0L6_2atmpS3468;
    int32_t _M0L6_2atmpS3472;
    int32_t _M0L6_2atmpS3471;
    int32_t _M0L6_2atmpS3470;
    int32_t _M0L6_2atmpS3469;
    int32_t _M0L6_2atmpS3476;
    int32_t _M0L6_2atmpS3477;
    int32_t _M0L6_2atmpS3478;
    int32_t _M0L6_2atmpS3479;
    int32_t _M0L6_2atmpS3480;
    int32_t _M0L6_2atmpS3486;
    int32_t _M0L6_2atmpS3519;
    moonbit_string_t _result_4643;
    while (1) {
      if (_M0L1iS1261 < _M0L7_2abindS1259) {
        uint64_t _M0L1cS1263 = _M0L6outputS1262 % 10ull;
        int32_t _M0L6_2atmpS3525 = _M0Lm5indexS1252;
        int32_t _M0L6_2atmpS3524 = _M0L6_2atmpS3525 + _M0L7olengthS1256;
        int32_t _M0L6_2atmpS3520 = _M0L6_2atmpS3524 - _M0L1iS1261;
        int32_t _M0L6_2atmpS3523 = (int32_t)_M0L1cS1263;
        int32_t _M0L6_2atmpS3522 = 48 + _M0L6_2atmpS3523;
        int32_t _M0L6_2atmpS3521 = _M0L6_2atmpS3522 & 0xff;
        int32_t _M0L6_2atmpS3526;
        uint64_t _M0L6_2atmpS3527;
        if (
          _M0L6_2atmpS3520 < 0
          || _M0L6_2atmpS3520 >= Moonbit_array_length(_M0L6resultS1251)
        ) {
          #line 547 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1251[_M0L6_2atmpS3520] = _M0L6_2atmpS3521;
        _M0L6_2atmpS3526 = _M0L1iS1261 + 1;
        _M0L6_2atmpS3527 = _M0L6outputS1262 / 10ull;
        _M0L1iS1261 = _M0L6_2atmpS3526;
        _M0L6outputS1262 = _M0L6_2atmpS3527;
        continue;
      } else {
        _M0L6outputS1260 = _M0L6outputS1262;
      }
      break;
    }
    _M0L6_2atmpS3468 = _M0Lm5indexS1252;
    _M0L6_2atmpS3472 = (int32_t)_M0L6outputS1260;
    _M0L6_2atmpS3471 = _M0L6_2atmpS3472 % 10;
    _M0L6_2atmpS3470 = 48 + _M0L6_2atmpS3471;
    _M0L6_2atmpS3469 = _M0L6_2atmpS3470 & 0xff;
    if (
      _M0L6_2atmpS3468 < 0
      || _M0L6_2atmpS3468 >= Moonbit_array_length(_M0L6resultS1251)
    ) {
      #line 552 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1251[_M0L6_2atmpS3468] = _M0L6_2atmpS3469;
    if (_M0L7olengthS1256 > 1) {
      int32_t _M0L6_2atmpS3474 = _M0Lm5indexS1252;
      int32_t _M0L6_2atmpS3473 = _M0L6_2atmpS3474 + 1;
      if (
        _M0L6_2atmpS3473 < 0
        || _M0L6_2atmpS3473 >= Moonbit_array_length(_M0L6resultS1251)
      ) {
        #line 554 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1251[_M0L6_2atmpS3473] = 46;
    } else {
      int32_t _M0L6_2atmpS3475 = _M0Lm5indexS1252;
      _M0Lm5indexS1252 = _M0L6_2atmpS3475 - 1;
    }
    _M0L6_2atmpS3476 = _M0Lm5indexS1252;
    _M0L6_2atmpS3477 = _M0L7olengthS1256 + 1;
    _M0Lm5indexS1252 = _M0L6_2atmpS3476 + _M0L6_2atmpS3477;
    _M0L6_2atmpS3478 = _M0Lm5indexS1252;
    if (
      _M0L6_2atmpS3478 < 0
      || _M0L6_2atmpS3478 >= Moonbit_array_length(_M0L6resultS1251)
    ) {
      #line 562 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1251[_M0L6_2atmpS3478] = 101;
    _M0L6_2atmpS3479 = _M0Lm5indexS1252;
    _M0Lm5indexS1252 = _M0L6_2atmpS3479 + 1;
    _M0L6_2atmpS3480 = _M0Lm3expS1257;
    if (_M0L6_2atmpS3480 < 0) {
      int32_t _M0L6_2atmpS3481 = _M0Lm5indexS1252;
      int32_t _M0L6_2atmpS3482;
      int32_t _M0L6_2atmpS3483;
      if (
        _M0L6_2atmpS3481 < 0
        || _M0L6_2atmpS3481 >= Moonbit_array_length(_M0L6resultS1251)
      ) {
        #line 565 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1251[_M0L6_2atmpS3481] = 45;
      _M0L6_2atmpS3482 = _M0Lm5indexS1252;
      _M0Lm5indexS1252 = _M0L6_2atmpS3482 + 1;
      _M0L6_2atmpS3483 = _M0Lm3expS1257;
      _M0Lm3expS1257 = -_M0L6_2atmpS3483;
    } else {
      int32_t _M0L6_2atmpS3484 = _M0Lm5indexS1252;
      int32_t _M0L6_2atmpS3485;
      if (
        _M0L6_2atmpS3484 < 0
        || _M0L6_2atmpS3484 >= Moonbit_array_length(_M0L6resultS1251)
      ) {
        #line 569 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1251[_M0L6_2atmpS3484] = 43;
      _M0L6_2atmpS3485 = _M0Lm5indexS1252;
      _M0Lm5indexS1252 = _M0L6_2atmpS3485 + 1;
    }
    _M0L6_2atmpS3486 = _M0Lm3expS1257;
    if (_M0L6_2atmpS3486 >= 100) {
      int32_t _M0L6_2atmpS3502 = _M0Lm3expS1257;
      int32_t _M0L1aS1265 = _M0L6_2atmpS3502 / 100;
      int32_t _M0L6_2atmpS3501 = _M0Lm3expS1257;
      int32_t _M0L6_2atmpS3500 = _M0L6_2atmpS3501 / 10;
      int32_t _M0L1bS1266 = _M0L6_2atmpS3500 % 10;
      int32_t _M0L6_2atmpS3499 = _M0Lm3expS1257;
      int32_t _M0L1cS1267 = _M0L6_2atmpS3499 % 10;
      int32_t _M0L6_2atmpS3487 = _M0Lm5indexS1252;
      int32_t _M0L6_2atmpS3489 = 48 + _M0L1aS1265;
      int32_t _M0L6_2atmpS3488 = _M0L6_2atmpS3489 & 0xff;
      int32_t _M0L6_2atmpS3493;
      int32_t _M0L6_2atmpS3490;
      int32_t _M0L6_2atmpS3492;
      int32_t _M0L6_2atmpS3491;
      int32_t _M0L6_2atmpS3497;
      int32_t _M0L6_2atmpS3494;
      int32_t _M0L6_2atmpS3496;
      int32_t _M0L6_2atmpS3495;
      int32_t _M0L6_2atmpS3498;
      if (
        _M0L6_2atmpS3487 < 0
        || _M0L6_2atmpS3487 >= Moonbit_array_length(_M0L6resultS1251)
      ) {
        #line 576 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1251[_M0L6_2atmpS3487] = _M0L6_2atmpS3488;
      _M0L6_2atmpS3493 = _M0Lm5indexS1252;
      _M0L6_2atmpS3490 = _M0L6_2atmpS3493 + 1;
      _M0L6_2atmpS3492 = 48 + _M0L1bS1266;
      _M0L6_2atmpS3491 = _M0L6_2atmpS3492 & 0xff;
      if (
        _M0L6_2atmpS3490 < 0
        || _M0L6_2atmpS3490 >= Moonbit_array_length(_M0L6resultS1251)
      ) {
        #line 577 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1251[_M0L6_2atmpS3490] = _M0L6_2atmpS3491;
      _M0L6_2atmpS3497 = _M0Lm5indexS1252;
      _M0L6_2atmpS3494 = _M0L6_2atmpS3497 + 2;
      _M0L6_2atmpS3496 = 48 + _M0L1cS1267;
      _M0L6_2atmpS3495 = _M0L6_2atmpS3496 & 0xff;
      if (
        _M0L6_2atmpS3494 < 0
        || _M0L6_2atmpS3494 >= Moonbit_array_length(_M0L6resultS1251)
      ) {
        #line 578 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1251[_M0L6_2atmpS3494] = _M0L6_2atmpS3495;
      _M0L6_2atmpS3498 = _M0Lm5indexS1252;
      _M0Lm5indexS1252 = _M0L6_2atmpS3498 + 3;
    } else {
      int32_t _M0L6_2atmpS3503 = _M0Lm3expS1257;
      if (_M0L6_2atmpS3503 >= 10) {
        int32_t _M0L6_2atmpS3513 = _M0Lm3expS1257;
        int32_t _M0L1aS1268 = _M0L6_2atmpS3513 / 10;
        int32_t _M0L6_2atmpS3512 = _M0Lm3expS1257;
        int32_t _M0L1bS1269 = _M0L6_2atmpS3512 % 10;
        int32_t _M0L6_2atmpS3504 = _M0Lm5indexS1252;
        int32_t _M0L6_2atmpS3506 = 48 + _M0L1aS1268;
        int32_t _M0L6_2atmpS3505 = _M0L6_2atmpS3506 & 0xff;
        int32_t _M0L6_2atmpS3510;
        int32_t _M0L6_2atmpS3507;
        int32_t _M0L6_2atmpS3509;
        int32_t _M0L6_2atmpS3508;
        int32_t _M0L6_2atmpS3511;
        if (
          _M0L6_2atmpS3504 < 0
          || _M0L6_2atmpS3504 >= Moonbit_array_length(_M0L6resultS1251)
        ) {
          #line 583 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1251[_M0L6_2atmpS3504] = _M0L6_2atmpS3505;
        _M0L6_2atmpS3510 = _M0Lm5indexS1252;
        _M0L6_2atmpS3507 = _M0L6_2atmpS3510 + 1;
        _M0L6_2atmpS3509 = 48 + _M0L1bS1269;
        _M0L6_2atmpS3508 = _M0L6_2atmpS3509 & 0xff;
        if (
          _M0L6_2atmpS3507 < 0
          || _M0L6_2atmpS3507 >= Moonbit_array_length(_M0L6resultS1251)
        ) {
          #line 584 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1251[_M0L6_2atmpS3507] = _M0L6_2atmpS3508;
        _M0L6_2atmpS3511 = _M0Lm5indexS1252;
        _M0Lm5indexS1252 = _M0L6_2atmpS3511 + 2;
      } else {
        int32_t _M0L6_2atmpS3514 = _M0Lm5indexS1252;
        int32_t _M0L6_2atmpS3517 = _M0Lm3expS1257;
        int32_t _M0L6_2atmpS3516 = 48 + _M0L6_2atmpS3517;
        int32_t _M0L6_2atmpS3515 = _M0L6_2atmpS3516 & 0xff;
        int32_t _M0L6_2atmpS3518;
        if (
          _M0L6_2atmpS3514 < 0
          || _M0L6_2atmpS3514 >= Moonbit_array_length(_M0L6resultS1251)
        ) {
          #line 587 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1251[_M0L6_2atmpS3514] = _M0L6_2atmpS3515;
        _M0L6_2atmpS3518 = _M0Lm5indexS1252;
        _M0Lm5indexS1252 = _M0L6_2atmpS3518 + 1;
      }
    }
    _M0L6_2atmpS3519 = _M0Lm5indexS1252;
    #line 590 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4643
    = _M0FPB19string__from__bytes(_M0L6resultS1251, 0, _M0L6_2atmpS3519);
    moonbit_decref(_M0L6resultS1251);
    return _result_4643;
  } else {
    int32_t _M0L6_2atmpS3528 = _M0Lm3expS1257;
    int32_t _M0L6_2atmpS3591;
    moonbit_string_t _result_4649;
    if (_M0L6_2atmpS3528 < 0) {
      int32_t _M0L6_2atmpS3529 = _M0Lm5indexS1252;
      int32_t _M0L6_2atmpS3531;
      int32_t _M0L6_2atmpS3530;
      int32_t _M0L6_2atmpS3532;
      int32_t _M0L1iS1270;
      int32_t _M0L6_2atmpS3547;
      int32_t _M0L6_2atmpS3549;
      int32_t _M0L6_2atmpS3548;
      int32_t _M0L7currentS1272;
      int32_t _M0L1iS1273;
      uint64_t _M0L6outputS1274;
      if (
        _M0L6_2atmpS3529 < 0
        || _M0L6_2atmpS3529 >= Moonbit_array_length(_M0L6resultS1251)
      ) {
        #line 595 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1251[_M0L6_2atmpS3529] = 48;
      _M0L6_2atmpS3531 = _M0Lm5indexS1252;
      _M0L6_2atmpS3530 = _M0L6_2atmpS3531 + 1;
      if (
        _M0L6_2atmpS3530 < 0
        || _M0L6_2atmpS3530 >= Moonbit_array_length(_M0L6resultS1251)
      ) {
        #line 596 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1251[_M0L6_2atmpS3530] = 46;
      _M0L6_2atmpS3532 = _M0Lm5indexS1252;
      _M0Lm5indexS1252 = _M0L6_2atmpS3532 + 2;
      _M0L1iS1270 = -1;
      while (1) {
        int32_t _M0L6_2atmpS3533 = _M0Lm3expS1257;
        if (_M0L1iS1270 > _M0L6_2atmpS3533) {
          int32_t _M0L6_2atmpS3536 = _M0Lm5indexS1252;
          int32_t _M0L6_2atmpS3535 = _M0L6_2atmpS3536 - _M0L1iS1270;
          int32_t _M0L6_2atmpS3534 = _M0L6_2atmpS3535 - 1;
          int32_t _M0L6_2atmpS3537;
          if (
            _M0L6_2atmpS3534 < 0
            || _M0L6_2atmpS3534 >= Moonbit_array_length(_M0L6resultS1251)
          ) {
            #line 599 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1251[_M0L6_2atmpS3534] = 48;
          _M0L6_2atmpS3537 = _M0L1iS1270 - 1;
          _M0L1iS1270 = _M0L6_2atmpS3537;
          continue;
        }
        break;
      }
      _M0L6_2atmpS3547 = _M0Lm5indexS1252;
      _M0L6_2atmpS3549 = _M0Lm3expS1257;
      _M0L6_2atmpS3548 = -1 - _M0L6_2atmpS3549;
      _M0L7currentS1272 = _M0L6_2atmpS3547 + _M0L6_2atmpS3548;
      _M0L1iS1273 = 0;
      _M0L6outputS1274 = _M0L6outputS1254;
      while (1) {
        if (_M0L1iS1273 < _M0L7olengthS1256) {
          int32_t _M0L6_2atmpS3544 = _M0L7currentS1272 + _M0L7olengthS1256;
          int32_t _M0L6_2atmpS3543 = _M0L6_2atmpS3544 - _M0L1iS1273;
          int32_t _M0L6_2atmpS3538 = _M0L6_2atmpS3543 - 1;
          uint64_t _M0L6_2atmpS3542 = _M0L6outputS1274 % 10ull;
          int32_t _M0L6_2atmpS3541 = (int32_t)_M0L6_2atmpS3542;
          int32_t _M0L6_2atmpS3540 = 48 + _M0L6_2atmpS3541;
          int32_t _M0L6_2atmpS3539 = _M0L6_2atmpS3540 & 0xff;
          int32_t _M0L6_2atmpS3545;
          uint64_t _M0L6_2atmpS3546;
          if (
            _M0L6_2atmpS3538 < 0
            || _M0L6_2atmpS3538 >= Moonbit_array_length(_M0L6resultS1251)
          ) {
            #line 603 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1251[_M0L6_2atmpS3538] = _M0L6_2atmpS3539;
          _M0L6_2atmpS3545 = _M0L1iS1273 + 1;
          _M0L6_2atmpS3546 = _M0L6outputS1274 / 10ull;
          _M0L1iS1273 = _M0L6_2atmpS3545;
          _M0L6outputS1274 = _M0L6_2atmpS3546;
          continue;
        }
        break;
      }
      _M0Lm5indexS1252 = _M0L7currentS1272 + _M0L7olengthS1256;
    } else {
      int32_t _M0L6_2atmpS3551 = _M0Lm3expS1257;
      int32_t _M0L6_2atmpS3550 = _M0L6_2atmpS3551 + 1;
      if (_M0L6_2atmpS3550 >= _M0L7olengthS1256) {
        int32_t _M0L1iS1276 = 0;
        uint64_t _M0L6outputS1277 = _M0L6outputS1254;
        int32_t _M0L6_2atmpS3562;
        int32_t _M0L6_2atmpS3567;
        int32_t _M0L7_2abindS1279;
        int32_t _M0L1iS1280;
        int32_t _M0L6_2atmpS3568;
        int32_t _M0L6_2atmpS3571;
        int32_t _M0L6_2atmpS3570;
        int32_t _M0L6_2atmpS3569;
        while (1) {
          if (_M0L1iS1276 < _M0L7olengthS1256) {
            int32_t _M0L6_2atmpS3559 = _M0Lm5indexS1252;
            int32_t _M0L6_2atmpS3558 = _M0L6_2atmpS3559 + _M0L7olengthS1256;
            int32_t _M0L6_2atmpS3557 = _M0L6_2atmpS3558 - _M0L1iS1276;
            int32_t _M0L6_2atmpS3552 = _M0L6_2atmpS3557 - 1;
            uint64_t _M0L6_2atmpS3556 = _M0L6outputS1277 % 10ull;
            int32_t _M0L6_2atmpS3555 = (int32_t)_M0L6_2atmpS3556;
            int32_t _M0L6_2atmpS3554 = 48 + _M0L6_2atmpS3555;
            int32_t _M0L6_2atmpS3553 = _M0L6_2atmpS3554 & 0xff;
            int32_t _M0L6_2atmpS3560;
            uint64_t _M0L6_2atmpS3561;
            if (
              _M0L6_2atmpS3552 < 0
              || _M0L6_2atmpS3552 >= Moonbit_array_length(_M0L6resultS1251)
            ) {
              #line 610 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1251[_M0L6_2atmpS3552] = _M0L6_2atmpS3553;
            _M0L6_2atmpS3560 = _M0L1iS1276 + 1;
            _M0L6_2atmpS3561 = _M0L6outputS1277 / 10ull;
            _M0L1iS1276 = _M0L6_2atmpS3560;
            _M0L6outputS1277 = _M0L6_2atmpS3561;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3562 = _M0Lm5indexS1252;
        _M0Lm5indexS1252 = _M0L6_2atmpS3562 + _M0L7olengthS1256;
        _M0L6_2atmpS3567 = _M0Lm3expS1257;
        _M0L7_2abindS1279 = _M0L6_2atmpS3567 + 1;
        _M0L1iS1280 = _M0L7olengthS1256;
        while (1) {
          if (_M0L1iS1280 < _M0L7_2abindS1279) {
            int32_t _M0L6_2atmpS3565 = _M0Lm5indexS1252;
            int32_t _M0L6_2atmpS3564 = _M0L6_2atmpS3565 + _M0L1iS1280;
            int32_t _M0L6_2atmpS3563 = _M0L6_2atmpS3564 - _M0L7olengthS1256;
            int32_t _M0L6_2atmpS3566;
            if (
              _M0L6_2atmpS3563 < 0
              || _M0L6_2atmpS3563 >= Moonbit_array_length(_M0L6resultS1251)
            ) {
              #line 615 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1251[_M0L6_2atmpS3563] = 48;
            _M0L6_2atmpS3566 = _M0L1iS1280 + 1;
            _M0L1iS1280 = _M0L6_2atmpS3566;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3568 = _M0Lm5indexS1252;
        _M0L6_2atmpS3571 = _M0Lm3expS1257;
        _M0L6_2atmpS3570 = _M0L6_2atmpS3571 + 1;
        _M0L6_2atmpS3569 = _M0L6_2atmpS3570 - _M0L7olengthS1256;
        _M0Lm5indexS1252 = _M0L6_2atmpS3568 + _M0L6_2atmpS3569;
      } else {
        int32_t _M0L6_2atmpS3588 = _M0Lm5indexS1252;
        int32_t _M0L6_2atmpS3587 = _M0L6_2atmpS3588 + 1;
        int32_t _M0L1iS1282 = 0;
        int32_t _M0L7currentS1283 = _M0L6_2atmpS3587;
        uint64_t _M0L6outputS1284 = _M0L6outputS1254;
        int32_t _M0L6_2atmpS3589;
        int32_t _M0L6_2atmpS3590;
        while (1) {
          if (_M0L1iS1282 < _M0L7olengthS1256) {
            int32_t _M0L6_2atmpS3583 = _M0L7olengthS1256 - _M0L1iS1282;
            int32_t _M0L6_2atmpS3581 = _M0L6_2atmpS3583 - 1;
            int32_t _M0L6_2atmpS3582 = _M0Lm3expS1257;
            int32_t _M0L7currentS1285;
            int32_t _M0L6_2atmpS3578;
            int32_t _M0L6_2atmpS3577;
            int32_t _M0L6_2atmpS3572;
            uint64_t _M0L6_2atmpS3576;
            int32_t _M0L6_2atmpS3575;
            int32_t _M0L6_2atmpS3574;
            int32_t _M0L6_2atmpS3573;
            int32_t _M0L6_2atmpS3579;
            uint64_t _M0L6_2atmpS3580;
            if (_M0L6_2atmpS3581 == _M0L6_2atmpS3582) {
              int32_t _M0L6_2atmpS3586 =
                _M0L7currentS1283 + _M0L7olengthS1256;
              int32_t _M0L6_2atmpS3585 = _M0L6_2atmpS3586 - _M0L1iS1282;
              int32_t _M0L6_2atmpS3584 = _M0L6_2atmpS3585 - 1;
              if (
                _M0L6_2atmpS3584 < 0
                || _M0L6_2atmpS3584 >= Moonbit_array_length(_M0L6resultS1251)
              ) {
                #line 622 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1251[_M0L6_2atmpS3584] = 46;
              _M0L7currentS1285 = _M0L7currentS1283 - 1;
            } else {
              _M0L7currentS1285 = _M0L7currentS1283;
            }
            _M0L6_2atmpS3578 = _M0L7currentS1285 + _M0L7olengthS1256;
            _M0L6_2atmpS3577 = _M0L6_2atmpS3578 - _M0L1iS1282;
            _M0L6_2atmpS3572 = _M0L6_2atmpS3577 - 1;
            _M0L6_2atmpS3576 = _M0L6outputS1284 % 10ull;
            _M0L6_2atmpS3575 = (int32_t)_M0L6_2atmpS3576;
            _M0L6_2atmpS3574 = 48 + _M0L6_2atmpS3575;
            _M0L6_2atmpS3573 = _M0L6_2atmpS3574 & 0xff;
            if (
              _M0L6_2atmpS3572 < 0
              || _M0L6_2atmpS3572 >= Moonbit_array_length(_M0L6resultS1251)
            ) {
              #line 627 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1251[_M0L6_2atmpS3572] = _M0L6_2atmpS3573;
            _M0L6_2atmpS3579 = _M0L1iS1282 + 1;
            _M0L6_2atmpS3580 = _M0L6outputS1284 / 10ull;
            _M0L1iS1282 = _M0L6_2atmpS3579;
            _M0L7currentS1283 = _M0L7currentS1285;
            _M0L6outputS1284 = _M0L6_2atmpS3580;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3589 = _M0Lm5indexS1252;
        _M0L6_2atmpS3590 = _M0L7olengthS1256 + 1;
        _M0Lm5indexS1252 = _M0L6_2atmpS3589 + _M0L6_2atmpS3590;
      }
    }
    _M0L6_2atmpS3591 = _M0Lm5indexS1252;
    #line 632 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4649
    = _M0FPB19string__from__bytes(_M0L6resultS1251, 0, _M0L6_2atmpS3591);
    moonbit_decref(_M0L6resultS1251);
    return _result_4649;
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1197,
  uint32_t _M0L12ieeeExponentS1196
) {
  int32_t _M0Lm2e2S1194;
  uint64_t _M0Lm2m2S1195;
  uint64_t _M0L6_2atmpS3465;
  uint64_t _M0L6_2atmpS3464;
  int32_t _M0L4evenS1198;
  uint64_t _M0L6_2atmpS3463;
  uint64_t _M0L2mvS1199;
  int32_t _M0L7mmShiftS1200;
  uint64_t _M0Lm2vrS1201;
  uint64_t _M0Lm2vpS1202;
  uint64_t _M0Lm2vmS1203;
  int32_t _M0Lm3e10S1204;
  int32_t _M0Lm17vmIsTrailingZerosS1205;
  int32_t _M0Lm17vrIsTrailingZerosS1206;
  int32_t _M0L6_2atmpS3365;
  int32_t _M0Lm7removedS1225;
  int32_t _M0Lm16lastRemovedDigitS1226;
  uint64_t _M0Lm6outputS1227;
  int32_t _M0L6_2atmpS3461;
  int32_t _M0L6_2atmpS3462;
  int32_t _M0L3expS1250;
  uint64_t _M0L6_2atmpS3460;
  struct _M0TPB17FloatingDecimal64* _block_4655;
  #line 347 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0Lm2e2S1194 = 0;
  _M0Lm2m2S1195 = 0ull;
  if (_M0L12ieeeExponentS1196 == 0u) {
    _M0Lm2e2S1194 = -1076;
    _M0Lm2m2S1195 = _M0L12ieeeMantissaS1197;
  } else {
    int32_t _M0L6_2atmpS3364 = *(int32_t*)&_M0L12ieeeExponentS1196;
    int32_t _M0L6_2atmpS3363 = _M0L6_2atmpS3364 - 1023;
    int32_t _M0L6_2atmpS3362 = _M0L6_2atmpS3363 - 52;
    _M0Lm2e2S1194 = _M0L6_2atmpS3362 - 2;
    _M0Lm2m2S1195 = 4503599627370496ull | _M0L12ieeeMantissaS1197;
  }
  _M0L6_2atmpS3465 = _M0Lm2m2S1195;
  _M0L6_2atmpS3464 = _M0L6_2atmpS3465 & 1ull;
  _M0L4evenS1198 = _M0L6_2atmpS3464 == 0ull;
  _M0L6_2atmpS3463 = _M0Lm2m2S1195;
  _M0L2mvS1199 = 4ull * _M0L6_2atmpS3463;
  if (_M0L12ieeeMantissaS1197 != 0ull) {
    _M0L7mmShiftS1200 = 1;
  } else {
    _M0L7mmShiftS1200 = _M0L12ieeeExponentS1196 <= 1u;
  }
  _M0Lm2vrS1201 = 0ull;
  _M0Lm2vpS1202 = 0ull;
  _M0Lm2vmS1203 = 0ull;
  _M0Lm3e10S1204 = 0;
  _M0Lm17vmIsTrailingZerosS1205 = 0;
  _M0Lm17vrIsTrailingZerosS1206 = 0;
  _M0L6_2atmpS3365 = _M0Lm2e2S1194;
  if (_M0L6_2atmpS3365 >= 0) {
    int32_t _M0L6_2atmpS3387 = _M0Lm2e2S1194;
    int32_t _M0L6_2atmpS3383;
    int32_t _M0L6_2atmpS3386;
    int32_t _M0L6_2atmpS3385;
    int32_t _M0L6_2atmpS3384;
    int32_t _M0L1qS1207;
    int32_t _M0L6_2atmpS3382;
    int32_t _M0L6_2atmpS3381;
    int32_t _M0L1kS1208;
    int32_t _M0L6_2atmpS3380;
    int32_t _M0L6_2atmpS3379;
    int32_t _M0L6_2atmpS3378;
    int32_t _M0L1iS1209;
    struct _M0TPB8Pow5Pair _M0L4pow5S1210;
    uint64_t _M0L6_2atmpS3377;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1211;
    uint64_t _M0L8_2avrOutS1212;
    uint64_t _M0L8_2avpOutS1213;
    uint64_t _M0L8_2avmOutS1214;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3383 = _M0FPB9log10Pow2(_M0L6_2atmpS3387);
    _M0L6_2atmpS3386 = _M0Lm2e2S1194;
    _M0L6_2atmpS3385 = _M0L6_2atmpS3386 > 3;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3384 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS3385);
    _M0L1qS1207 = _M0L6_2atmpS3383 - _M0L6_2atmpS3384;
    _M0Lm3e10S1204 = _M0L1qS1207;
    #line 385 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3382 = _M0FPB8pow5bits(_M0L1qS1207);
    _M0L6_2atmpS3381 = 125 + _M0L6_2atmpS3382;
    _M0L1kS1208 = _M0L6_2atmpS3381 - 1;
    _M0L6_2atmpS3380 = _M0Lm2e2S1194;
    _M0L6_2atmpS3379 = -_M0L6_2atmpS3380;
    _M0L6_2atmpS3378 = _M0L6_2atmpS3379 + _M0L1qS1207;
    _M0L1iS1209 = _M0L6_2atmpS3378 + _M0L1kS1208;
    #line 387 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1210 = _M0FPB22double__computeInvPow5(_M0L1qS1207);
    _M0L6_2atmpS3377 = _M0Lm2m2S1195;
    #line 388 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1211
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS3377, _M0L4pow5S1210, _M0L1iS1209, _M0L7mmShiftS1200);
    _M0L8_2avrOutS1212 = _M0L7_2abindS1211.$0;
    _M0L8_2avpOutS1213 = _M0L7_2abindS1211.$1;
    _M0L8_2avmOutS1214 = _M0L7_2abindS1211.$2;
    _M0Lm2vrS1201 = _M0L8_2avrOutS1212;
    _M0Lm2vpS1202 = _M0L8_2avpOutS1213;
    _M0Lm2vmS1203 = _M0L8_2avmOutS1214;
    if (_M0L1qS1207 <= 21) {
      int32_t _M0L6_2atmpS3373 = (int32_t)_M0L2mvS1199;
      uint64_t _M0L6_2atmpS3376 = _M0L2mvS1199 / 5ull;
      int32_t _M0L6_2atmpS3375 = (int32_t)_M0L6_2atmpS3376;
      int32_t _M0L6_2atmpS3374 = 5 * _M0L6_2atmpS3375;
      int32_t _M0L6mvMod5S1215 = _M0L6_2atmpS3373 - _M0L6_2atmpS3374;
      if (_M0L6mvMod5S1215 == 0) {
        #line 400 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1206
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1199, _M0L1qS1207);
      } else if (_M0L4evenS1198) {
        uint64_t _M0L6_2atmpS3367 = _M0L2mvS1199 - 1ull;
        uint64_t _M0L6_2atmpS3368;
        uint64_t _M0L6_2atmpS3366;
        #line 406 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3368 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1200);
        _M0L6_2atmpS3366 = _M0L6_2atmpS3367 - _M0L6_2atmpS3368;
        #line 405 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1205
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS3366, _M0L1qS1207);
      } else {
        uint64_t _M0L6_2atmpS3369 = _M0Lm2vpS1202;
        uint64_t _M0L6_2atmpS3372 = _M0L2mvS1199 + 2ull;
        int32_t _M0L6_2atmpS3371;
        uint64_t _M0L6_2atmpS3370;
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3371
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS3372, _M0L1qS1207);
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3370 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS3371);
        _M0Lm2vpS1202 = _M0L6_2atmpS3369 - _M0L6_2atmpS3370;
      }
    }
  } else {
    int32_t _M0L6_2atmpS3401 = _M0Lm2e2S1194;
    int32_t _M0L6_2atmpS3400 = -_M0L6_2atmpS3401;
    int32_t _M0L6_2atmpS3395;
    int32_t _M0L6_2atmpS3399;
    int32_t _M0L6_2atmpS3398;
    int32_t _M0L6_2atmpS3397;
    int32_t _M0L6_2atmpS3396;
    int32_t _M0L1qS1216;
    int32_t _M0L6_2atmpS3388;
    int32_t _M0L6_2atmpS3394;
    int32_t _M0L6_2atmpS3393;
    int32_t _M0L1iS1217;
    int32_t _M0L6_2atmpS3392;
    int32_t _M0L1kS1218;
    int32_t _M0L1jS1219;
    struct _M0TPB8Pow5Pair _M0L4pow5S1220;
    uint64_t _M0L6_2atmpS3391;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1221;
    uint64_t _M0L8_2avrOutS1222;
    uint64_t _M0L8_2avpOutS1223;
    uint64_t _M0L8_2avmOutS1224;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3395 = _M0FPB9log10Pow5(_M0L6_2atmpS3400);
    _M0L6_2atmpS3399 = _M0Lm2e2S1194;
    _M0L6_2atmpS3398 = -_M0L6_2atmpS3399;
    _M0L6_2atmpS3397 = _M0L6_2atmpS3398 > 1;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3396 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS3397);
    _M0L1qS1216 = _M0L6_2atmpS3395 - _M0L6_2atmpS3396;
    _M0L6_2atmpS3388 = _M0Lm2e2S1194;
    _M0Lm3e10S1204 = _M0L1qS1216 + _M0L6_2atmpS3388;
    _M0L6_2atmpS3394 = _M0Lm2e2S1194;
    _M0L6_2atmpS3393 = -_M0L6_2atmpS3394;
    _M0L1iS1217 = _M0L6_2atmpS3393 - _M0L1qS1216;
    #line 418 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3392 = _M0FPB8pow5bits(_M0L1iS1217);
    _M0L1kS1218 = _M0L6_2atmpS3392 - 125;
    _M0L1jS1219 = _M0L1qS1216 - _M0L1kS1218;
    #line 420 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1220 = _M0FPB19double__computePow5(_M0L1iS1217);
    _M0L6_2atmpS3391 = _M0Lm2m2S1195;
    #line 421 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1221
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS3391, _M0L4pow5S1220, _M0L1jS1219, _M0L7mmShiftS1200);
    _M0L8_2avrOutS1222 = _M0L7_2abindS1221.$0;
    _M0L8_2avpOutS1223 = _M0L7_2abindS1221.$1;
    _M0L8_2avmOutS1224 = _M0L7_2abindS1221.$2;
    _M0Lm2vrS1201 = _M0L8_2avrOutS1222;
    _M0Lm2vpS1202 = _M0L8_2avpOutS1223;
    _M0Lm2vmS1203 = _M0L8_2avmOutS1224;
    if (_M0L1qS1216 <= 1) {
      _M0Lm17vrIsTrailingZerosS1206 = 1;
      if (_M0L4evenS1198) {
        int32_t _M0L6_2atmpS3389;
        #line 432 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3389 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1200);
        _M0Lm17vmIsTrailingZerosS1205 = _M0L6_2atmpS3389 == 1;
      } else {
        uint64_t _M0L6_2atmpS3390 = _M0Lm2vpS1202;
        _M0Lm2vpS1202 = _M0L6_2atmpS3390 - 1ull;
      }
    } else if (_M0L1qS1216 < 63) {
      #line 437 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS1206
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS1199, _M0L1qS1216);
    }
  }
  _M0Lm7removedS1225 = 0;
  _M0Lm16lastRemovedDigitS1226 = 0;
  _M0Lm6outputS1227 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS1205 || _M0Lm17vrIsTrailingZerosS1206) {
    int32_t _if__result_4652;
    uint64_t _M0L6_2atmpS3431;
    uint64_t _M0L6_2atmpS3437;
    uint64_t _M0L6_2atmpS3438;
    int32_t _if__result_4653;
    int32_t _M0L6_2atmpS3434;
    int64_t _M0L6_2atmpS3433;
    uint64_t _M0L6_2atmpS3432;
    while (1) {
      uint64_t _M0L6_2atmpS3414 = _M0Lm2vpS1202;
      uint64_t _M0L7vpDiv10S1228 = _M0L6_2atmpS3414 / 10ull;
      uint64_t _M0L6_2atmpS3413 = _M0Lm2vmS1203;
      uint64_t _M0L7vmDiv10S1229 = _M0L6_2atmpS3413 / 10ull;
      uint64_t _M0L6_2atmpS3412;
      int32_t _M0L6_2atmpS3409;
      int32_t _M0L6_2atmpS3411;
      int32_t _M0L6_2atmpS3410;
      int32_t _M0L7vmMod10S1231;
      uint64_t _M0L6_2atmpS3408;
      uint64_t _M0L7vrDiv10S1232;
      uint64_t _M0L6_2atmpS3407;
      int32_t _M0L6_2atmpS3404;
      int32_t _M0L6_2atmpS3406;
      int32_t _M0L6_2atmpS3405;
      int32_t _M0L7vrMod10S1233;
      int32_t _M0L6_2atmpS3403;
      if (_M0L7vpDiv10S1228 <= _M0L7vmDiv10S1229) {
        break;
      }
      _M0L6_2atmpS3412 = _M0Lm2vmS1203;
      _M0L6_2atmpS3409 = (int32_t)_M0L6_2atmpS3412;
      _M0L6_2atmpS3411 = (int32_t)_M0L7vmDiv10S1229;
      _M0L6_2atmpS3410 = 10 * _M0L6_2atmpS3411;
      _M0L7vmMod10S1231 = _M0L6_2atmpS3409 - _M0L6_2atmpS3410;
      _M0L6_2atmpS3408 = _M0Lm2vrS1201;
      _M0L7vrDiv10S1232 = _M0L6_2atmpS3408 / 10ull;
      _M0L6_2atmpS3407 = _M0Lm2vrS1201;
      _M0L6_2atmpS3404 = (int32_t)_M0L6_2atmpS3407;
      _M0L6_2atmpS3406 = (int32_t)_M0L7vrDiv10S1232;
      _M0L6_2atmpS3405 = 10 * _M0L6_2atmpS3406;
      _M0L7vrMod10S1233 = _M0L6_2atmpS3404 - _M0L6_2atmpS3405;
      if (_M0Lm17vmIsTrailingZerosS1205) {
        _M0Lm17vmIsTrailingZerosS1205 = _M0L7vmMod10S1231 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1205 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1206) {
        int32_t _M0L6_2atmpS3402 = _M0Lm16lastRemovedDigitS1226;
        _M0Lm17vrIsTrailingZerosS1206 = _M0L6_2atmpS3402 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1206 = 0;
      }
      _M0Lm16lastRemovedDigitS1226 = _M0L7vrMod10S1233;
      _M0Lm2vrS1201 = _M0L7vrDiv10S1232;
      _M0Lm2vpS1202 = _M0L7vpDiv10S1228;
      _M0Lm2vmS1203 = _M0L7vmDiv10S1229;
      _M0L6_2atmpS3403 = _M0Lm7removedS1225;
      _M0Lm7removedS1225 = _M0L6_2atmpS3403 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1205) {
      while (1) {
        uint64_t _M0L6_2atmpS3427 = _M0Lm2vmS1203;
        uint64_t _M0L7vmDiv10S1234 = _M0L6_2atmpS3427 / 10ull;
        uint64_t _M0L6_2atmpS3426 = _M0Lm2vmS1203;
        int32_t _M0L6_2atmpS3423 = (int32_t)_M0L6_2atmpS3426;
        int32_t _M0L6_2atmpS3425 = (int32_t)_M0L7vmDiv10S1234;
        int32_t _M0L6_2atmpS3424 = 10 * _M0L6_2atmpS3425;
        int32_t _M0L7vmMod10S1235 = _M0L6_2atmpS3423 - _M0L6_2atmpS3424;
        uint64_t _M0L6_2atmpS3422;
        uint64_t _M0L7vpDiv10S1237;
        uint64_t _M0L6_2atmpS3421;
        uint64_t _M0L7vrDiv10S1238;
        uint64_t _M0L6_2atmpS3420;
        int32_t _M0L6_2atmpS3417;
        int32_t _M0L6_2atmpS3419;
        int32_t _M0L6_2atmpS3418;
        int32_t _M0L7vrMod10S1239;
        int32_t _M0L6_2atmpS3416;
        if (_M0L7vmMod10S1235 != 0) {
          break;
        }
        _M0L6_2atmpS3422 = _M0Lm2vpS1202;
        _M0L7vpDiv10S1237 = _M0L6_2atmpS3422 / 10ull;
        _M0L6_2atmpS3421 = _M0Lm2vrS1201;
        _M0L7vrDiv10S1238 = _M0L6_2atmpS3421 / 10ull;
        _M0L6_2atmpS3420 = _M0Lm2vrS1201;
        _M0L6_2atmpS3417 = (int32_t)_M0L6_2atmpS3420;
        _M0L6_2atmpS3419 = (int32_t)_M0L7vrDiv10S1238;
        _M0L6_2atmpS3418 = 10 * _M0L6_2atmpS3419;
        _M0L7vrMod10S1239 = _M0L6_2atmpS3417 - _M0L6_2atmpS3418;
        if (_M0Lm17vrIsTrailingZerosS1206) {
          int32_t _M0L6_2atmpS3415 = _M0Lm16lastRemovedDigitS1226;
          _M0Lm17vrIsTrailingZerosS1206 = _M0L6_2atmpS3415 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1206 = 0;
        }
        _M0Lm16lastRemovedDigitS1226 = _M0L7vrMod10S1239;
        _M0Lm2vrS1201 = _M0L7vrDiv10S1238;
        _M0Lm2vpS1202 = _M0L7vpDiv10S1237;
        _M0Lm2vmS1203 = _M0L7vmDiv10S1234;
        _M0L6_2atmpS3416 = _M0Lm7removedS1225;
        _M0Lm7removedS1225 = _M0L6_2atmpS3416 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1206) {
      int32_t _M0L6_2atmpS3430 = _M0Lm16lastRemovedDigitS1226;
      if (_M0L6_2atmpS3430 == 5) {
        uint64_t _M0L6_2atmpS3429 = _M0Lm2vrS1201;
        uint64_t _M0L6_2atmpS3428 = _M0L6_2atmpS3429 % 2ull;
        _if__result_4652 = _M0L6_2atmpS3428 == 0ull;
      } else {
        _if__result_4652 = 0;
      }
    } else {
      _if__result_4652 = 0;
    }
    if (_if__result_4652) {
      _M0Lm16lastRemovedDigitS1226 = 4;
    }
    _M0L6_2atmpS3431 = _M0Lm2vrS1201;
    _M0L6_2atmpS3437 = _M0Lm2vrS1201;
    _M0L6_2atmpS3438 = _M0Lm2vmS1203;
    if (_M0L6_2atmpS3437 == _M0L6_2atmpS3438) {
      if (!_M0L4evenS1198) {
        _if__result_4653 = 1;
      } else {
        int32_t _M0L6_2atmpS3436 = _M0Lm17vmIsTrailingZerosS1205;
        _if__result_4653 = !_M0L6_2atmpS3436;
      }
    } else {
      _if__result_4653 = 0;
    }
    if (_if__result_4653) {
      _M0L6_2atmpS3434 = 1;
    } else {
      int32_t _M0L6_2atmpS3435 = _M0Lm16lastRemovedDigitS1226;
      _M0L6_2atmpS3434 = _M0L6_2atmpS3435 >= 5;
    }
    #line 487 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3433 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS3434);
    _M0L6_2atmpS3432 = *(uint64_t*)&_M0L6_2atmpS3433;
    _M0Lm6outputS1227 = _M0L6_2atmpS3431 + _M0L6_2atmpS3432;
  } else {
    int32_t _M0Lm7roundUpS1240 = 0;
    uint64_t _M0L6_2atmpS3459 = _M0Lm2vpS1202;
    uint64_t _M0L8vpDiv100S1241 = _M0L6_2atmpS3459 / 100ull;
    uint64_t _M0L6_2atmpS3458 = _M0Lm2vmS1203;
    uint64_t _M0L8vmDiv100S1242 = _M0L6_2atmpS3458 / 100ull;
    uint64_t _M0L6_2atmpS3453;
    uint64_t _M0L6_2atmpS3456;
    uint64_t _M0L6_2atmpS3457;
    int32_t _M0L6_2atmpS3455;
    uint64_t _M0L6_2atmpS3454;
    if (_M0L8vpDiv100S1241 > _M0L8vmDiv100S1242) {
      uint64_t _M0L6_2atmpS3444 = _M0Lm2vrS1201;
      uint64_t _M0L8vrDiv100S1243 = _M0L6_2atmpS3444 / 100ull;
      uint64_t _M0L6_2atmpS3443 = _M0Lm2vrS1201;
      int32_t _M0L6_2atmpS3440 = (int32_t)_M0L6_2atmpS3443;
      int32_t _M0L6_2atmpS3442 = (int32_t)_M0L8vrDiv100S1243;
      int32_t _M0L6_2atmpS3441 = 100 * _M0L6_2atmpS3442;
      int32_t _M0L8vrMod100S1244 = _M0L6_2atmpS3440 - _M0L6_2atmpS3441;
      int32_t _M0L6_2atmpS3439;
      _M0Lm7roundUpS1240 = _M0L8vrMod100S1244 >= 50;
      _M0Lm2vrS1201 = _M0L8vrDiv100S1243;
      _M0Lm2vpS1202 = _M0L8vpDiv100S1241;
      _M0Lm2vmS1203 = _M0L8vmDiv100S1242;
      _M0L6_2atmpS3439 = _M0Lm7removedS1225;
      _M0Lm7removedS1225 = _M0L6_2atmpS3439 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS3452 = _M0Lm2vpS1202;
      uint64_t _M0L7vpDiv10S1245 = _M0L6_2atmpS3452 / 10ull;
      uint64_t _M0L6_2atmpS3451 = _M0Lm2vmS1203;
      uint64_t _M0L7vmDiv10S1246 = _M0L6_2atmpS3451 / 10ull;
      uint64_t _M0L6_2atmpS3450;
      uint64_t _M0L7vrDiv10S1248;
      uint64_t _M0L6_2atmpS3449;
      int32_t _M0L6_2atmpS3446;
      int32_t _M0L6_2atmpS3448;
      int32_t _M0L6_2atmpS3447;
      int32_t _M0L7vrMod10S1249;
      int32_t _M0L6_2atmpS3445;
      if (_M0L7vpDiv10S1245 <= _M0L7vmDiv10S1246) {
        break;
      }
      _M0L6_2atmpS3450 = _M0Lm2vrS1201;
      _M0L7vrDiv10S1248 = _M0L6_2atmpS3450 / 10ull;
      _M0L6_2atmpS3449 = _M0Lm2vrS1201;
      _M0L6_2atmpS3446 = (int32_t)_M0L6_2atmpS3449;
      _M0L6_2atmpS3448 = (int32_t)_M0L7vrDiv10S1248;
      _M0L6_2atmpS3447 = 10 * _M0L6_2atmpS3448;
      _M0L7vrMod10S1249 = _M0L6_2atmpS3446 - _M0L6_2atmpS3447;
      _M0Lm7roundUpS1240 = _M0L7vrMod10S1249 >= 5;
      _M0Lm2vrS1201 = _M0L7vrDiv10S1248;
      _M0Lm2vpS1202 = _M0L7vpDiv10S1245;
      _M0Lm2vmS1203 = _M0L7vmDiv10S1246;
      _M0L6_2atmpS3445 = _M0Lm7removedS1225;
      _M0Lm7removedS1225 = _M0L6_2atmpS3445 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS3453 = _M0Lm2vrS1201;
    _M0L6_2atmpS3456 = _M0Lm2vrS1201;
    _M0L6_2atmpS3457 = _M0Lm2vmS1203;
    _M0L6_2atmpS3455
    = _M0L6_2atmpS3456 == _M0L6_2atmpS3457 || _M0Lm7roundUpS1240;
    #line 522 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3454 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS3455);
    _M0Lm6outputS1227 = _M0L6_2atmpS3453 + _M0L6_2atmpS3454;
  }
  _M0L6_2atmpS3461 = _M0Lm3e10S1204;
  _M0L6_2atmpS3462 = _M0Lm7removedS1225;
  _M0L3expS1250 = _M0L6_2atmpS3461 + _M0L6_2atmpS3462;
  _M0L6_2atmpS3460 = _M0Lm6outputS1227;
  _block_4655
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_4655)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _block_4655->$0 = _M0L6_2atmpS3460;
  _block_4655->$1 = _M0L3expS1250;
  return _block_4655;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS1193) {
  #line 110 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1193) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS1192) {
  #line 58 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1192) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS1191) {
  #line 32 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1191) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS1190) {
  #line 280 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L1vS1190 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS1190 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS1190 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS1190 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS1190 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS1190 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS1190 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS1190 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS1190 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS1190 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS1190 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS1190 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS1190 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS1190 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS1190 >= 100ull) {
    return 3;
  }
  if (_M0L1vS1190 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS1173) {
  int32_t _M0L6_2atmpS3361;
  int32_t _M0L6_2atmpS3360;
  int32_t _M0L4baseS1172;
  int32_t _M0L5base2S1174;
  int32_t _M0L6offsetS1175;
  int32_t _M0L6_2atmpS3359;
  uint64_t _M0L4mul0S1176;
  int32_t _M0L6_2atmpS3358;
  int32_t _M0L6_2atmpS3357;
  uint64_t _M0L4mul1S1177;
  uint64_t _M0L1mS1178;
  struct _M0TPB7Umul128 _M0L7_2abindS1179;
  uint64_t _M0L7_2alow1S1180;
  uint64_t _M0L8_2ahigh1S1181;
  struct _M0TPB7Umul128 _M0L7_2abindS1182;
  uint64_t _M0L7_2alow0S1183;
  uint64_t _M0L8_2ahigh0S1184;
  uint64_t _M0L3sumS1185;
  uint64_t _M0Lm5high1S1186;
  int32_t _M0L6_2atmpS3355;
  int32_t _M0L6_2atmpS3356;
  int32_t _M0L5deltaS1187;
  uint64_t _M0L6_2atmpS3354;
  uint64_t _M0L6_2atmpS3346;
  int32_t _M0L6_2atmpS3353;
  uint32_t _M0L6_2atmpS3350;
  int32_t _M0L6_2atmpS3352;
  int32_t _M0L6_2atmpS3351;
  uint32_t _M0L6_2atmpS3349;
  uint32_t _M0L6_2atmpS3348;
  uint64_t _M0L6_2atmpS3347;
  uint64_t _M0L1aS1188;
  uint64_t _M0L6_2atmpS3345;
  uint64_t _M0L1bS1189;
  #line 239 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3361 = _M0L1iS1173 + 26;
  _M0L6_2atmpS3360 = _M0L6_2atmpS3361 - 1;
  _M0L4baseS1172 = _M0L6_2atmpS3360 / 26;
  _M0L5base2S1174 = _M0L4baseS1172 * 26;
  _M0L6offsetS1175 = _M0L5base2S1174 - _M0L1iS1173;
  _M0L6_2atmpS3359 = _M0L4baseS1172 * 2;
  #line 243 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1176
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS3359);
  _M0L6_2atmpS3358 = _M0L4baseS1172 * 2;
  _M0L6_2atmpS3357 = _M0L6_2atmpS3358 + 1;
  #line 244 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1177
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS3357);
  if (_M0L6offsetS1175 == 0) {
    return (struct _M0TPB8Pow5Pair){.$0 = _M0L4mul0S1176,
                                      .$1 = _M0L4mul1S1177};
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1mS1178
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1175);
  #line 249 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1179 = _M0FPB7umul128(_M0L1mS1178, _M0L4mul1S1177);
  _M0L7_2alow1S1180 = _M0L7_2abindS1179.$0;
  _M0L8_2ahigh1S1181 = _M0L7_2abindS1179.$1;
  #line 250 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1182 = _M0FPB7umul128(_M0L1mS1178, _M0L4mul0S1176);
  _M0L7_2alow0S1183 = _M0L7_2abindS1182.$0;
  _M0L8_2ahigh0S1184 = _M0L7_2abindS1182.$1;
  _M0L3sumS1185 = _M0L8_2ahigh0S1184 + _M0L7_2alow1S1180;
  _M0Lm5high1S1186 = _M0L8_2ahigh1S1181;
  if (_M0L3sumS1185 < _M0L8_2ahigh0S1184) {
    uint64_t _M0L6_2atmpS3344 = _M0Lm5high1S1186;
    _M0Lm5high1S1186 = _M0L6_2atmpS3344 + 1ull;
  }
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3355 = _M0FPB8pow5bits(_M0L5base2S1174);
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3356 = _M0FPB8pow5bits(_M0L1iS1173);
  _M0L5deltaS1187 = _M0L6_2atmpS3355 - _M0L6_2atmpS3356;
  #line 257 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3354
  = _M0FPB13shiftright128(_M0L7_2alow0S1183, _M0L3sumS1185, _M0L5deltaS1187);
  _M0L6_2atmpS3346 = _M0L6_2atmpS3354 + 1ull;
  _M0L6_2atmpS3353 = _M0L1iS1173 / 16;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3350
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS3353);
  _M0L6_2atmpS3352 = _M0L1iS1173 % 16;
  _M0L6_2atmpS3351 = _M0L6_2atmpS3352 << 1;
  _M0L6_2atmpS3349 = _M0L6_2atmpS3350 >> (_M0L6_2atmpS3351 & 31);
  _M0L6_2atmpS3348 = _M0L6_2atmpS3349 & 3u;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3347 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS3348);
  _M0L1aS1188 = _M0L6_2atmpS3346 + _M0L6_2atmpS3347;
  _M0L6_2atmpS3345 = _M0Lm5high1S1186;
  #line 260 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1189
  = _M0FPB13shiftright128(_M0L3sumS1185, _M0L6_2atmpS3345, _M0L5deltaS1187);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1188, .$1 = _M0L1bS1189};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS1155) {
  int32_t _M0L4baseS1154;
  int32_t _M0L5base2S1156;
  int32_t _M0L6offsetS1157;
  int32_t _M0L6_2atmpS3343;
  uint64_t _M0L4mul0S1158;
  int32_t _M0L6_2atmpS3342;
  int32_t _M0L6_2atmpS3341;
  uint64_t _M0L4mul1S1159;
  uint64_t _M0L1mS1160;
  struct _M0TPB7Umul128 _M0L7_2abindS1161;
  uint64_t _M0L7_2alow1S1162;
  uint64_t _M0L8_2ahigh1S1163;
  struct _M0TPB7Umul128 _M0L7_2abindS1164;
  uint64_t _M0L7_2alow0S1165;
  uint64_t _M0L8_2ahigh0S1166;
  uint64_t _M0L3sumS1167;
  uint64_t _M0Lm5high1S1168;
  int32_t _M0L6_2atmpS3339;
  int32_t _M0L6_2atmpS3340;
  int32_t _M0L5deltaS1169;
  uint64_t _M0L6_2atmpS3331;
  int32_t _M0L6_2atmpS3338;
  uint32_t _M0L6_2atmpS3335;
  int32_t _M0L6_2atmpS3337;
  int32_t _M0L6_2atmpS3336;
  uint32_t _M0L6_2atmpS3334;
  uint32_t _M0L6_2atmpS3333;
  uint64_t _M0L6_2atmpS3332;
  uint64_t _M0L1aS1170;
  uint64_t _M0L6_2atmpS3330;
  uint64_t _M0L1bS1171;
  #line 213 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4baseS1154 = _M0L1iS1155 / 26;
  _M0L5base2S1156 = _M0L4baseS1154 * 26;
  _M0L6offsetS1157 = _M0L1iS1155 - _M0L5base2S1156;
  _M0L6_2atmpS3343 = _M0L4baseS1154 * 2;
  #line 217 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1158
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS3343);
  _M0L6_2atmpS3342 = _M0L4baseS1154 * 2;
  _M0L6_2atmpS3341 = _M0L6_2atmpS3342 + 1;
  #line 218 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1159
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS3341);
  if (_M0L6offsetS1157 == 0) {
    return (struct _M0TPB8Pow5Pair){.$0 = _M0L4mul0S1158,
                                      .$1 = _M0L4mul1S1159};
  }
  #line 222 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1mS1160
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1157);
  #line 223 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1161 = _M0FPB7umul128(_M0L1mS1160, _M0L4mul1S1159);
  _M0L7_2alow1S1162 = _M0L7_2abindS1161.$0;
  _M0L8_2ahigh1S1163 = _M0L7_2abindS1161.$1;
  #line 224 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1164 = _M0FPB7umul128(_M0L1mS1160, _M0L4mul0S1158);
  _M0L7_2alow0S1165 = _M0L7_2abindS1164.$0;
  _M0L8_2ahigh0S1166 = _M0L7_2abindS1164.$1;
  _M0L3sumS1167 = _M0L8_2ahigh0S1166 + _M0L7_2alow1S1162;
  _M0Lm5high1S1168 = _M0L8_2ahigh1S1163;
  if (_M0L3sumS1167 < _M0L8_2ahigh0S1166) {
    uint64_t _M0L6_2atmpS3329 = _M0Lm5high1S1168;
    _M0Lm5high1S1168 = _M0L6_2atmpS3329 + 1ull;
  }
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3339 = _M0FPB8pow5bits(_M0L1iS1155);
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3340 = _M0FPB8pow5bits(_M0L5base2S1156);
  _M0L5deltaS1169 = _M0L6_2atmpS3339 - _M0L6_2atmpS3340;
  #line 231 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3331
  = _M0FPB13shiftright128(_M0L7_2alow0S1165, _M0L3sumS1167, _M0L5deltaS1169);
  _M0L6_2atmpS3338 = _M0L1iS1155 / 16;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3335
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS3338);
  _M0L6_2atmpS3337 = _M0L1iS1155 % 16;
  _M0L6_2atmpS3336 = _M0L6_2atmpS3337 << 1;
  _M0L6_2atmpS3334 = _M0L6_2atmpS3335 >> (_M0L6_2atmpS3336 & 31);
  _M0L6_2atmpS3333 = _M0L6_2atmpS3334 & 3u;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3332 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS3333);
  _M0L1aS1170 = _M0L6_2atmpS3331 + _M0L6_2atmpS3332;
  _M0L6_2atmpS3330 = _M0Lm5high1S1168;
  #line 233 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1171
  = _M0FPB13shiftright128(_M0L3sumS1167, _M0L6_2atmpS3330, _M0L5deltaS1169);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1170, .$1 = _M0L1bS1171};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS1128,
  struct _M0TPB8Pow5Pair _M0L3mulS1125,
  int32_t _M0L1jS1141,
  int32_t _M0L7mmShiftS1143
) {
  uint64_t _M0L7_2amul0S1124;
  uint64_t _M0L7_2amul1S1126;
  uint64_t _M0L1mS1127;
  struct _M0TPB7Umul128 _M0L7_2abindS1129;
  uint64_t _M0L5_2aloS1130;
  uint64_t _M0L6_2atmpS1131;
  struct _M0TPB7Umul128 _M0L7_2abindS1132;
  uint64_t _M0L6_2alo2S1133;
  uint64_t _M0L6_2ahi2S1134;
  uint64_t _M0L3midS1135;
  uint64_t _M0L6_2atmpS3328;
  uint64_t _M0L2hiS1136;
  uint64_t _M0L3lo2S1137;
  uint64_t _M0L6_2atmpS3326;
  uint64_t _M0L6_2atmpS3327;
  uint64_t _M0L4mid2S1138;
  uint64_t _M0L6_2atmpS3325;
  uint64_t _M0L3hi2S1139;
  int32_t _M0L6_2atmpS3324;
  int32_t _M0L6_2atmpS3323;
  uint64_t _M0L2vpS1140;
  uint64_t _M0Lm2vmS1142;
  int32_t _M0L6_2atmpS3322;
  int32_t _M0L6_2atmpS3321;
  uint64_t _M0L2vrS1153;
  uint64_t _M0L6_2atmpS3320;
  #line 129 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2amul0S1124 = _M0L3mulS1125.$0;
  _M0L7_2amul1S1126 = _M0L3mulS1125.$1;
  _M0L1mS1127 = _M0L1mS1128 << 1;
  #line 137 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1129 = _M0FPB7umul128(_M0L1mS1127, _M0L7_2amul0S1124);
  _M0L5_2aloS1130 = _M0L7_2abindS1129.$0;
  _M0L6_2atmpS1131 = _M0L7_2abindS1129.$1;
  #line 138 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1132 = _M0FPB7umul128(_M0L1mS1127, _M0L7_2amul1S1126);
  _M0L6_2alo2S1133 = _M0L7_2abindS1132.$0;
  _M0L6_2ahi2S1134 = _M0L7_2abindS1132.$1;
  _M0L3midS1135 = _M0L6_2atmpS1131 + _M0L6_2alo2S1133;
  if (_M0L3midS1135 < _M0L6_2atmpS1131) {
    _M0L6_2atmpS3328 = 1ull;
  } else {
    _M0L6_2atmpS3328 = 0ull;
  }
  _M0L2hiS1136 = _M0L6_2ahi2S1134 + _M0L6_2atmpS3328;
  _M0L3lo2S1137 = _M0L5_2aloS1130 + _M0L7_2amul0S1124;
  _M0L6_2atmpS3326 = _M0L3midS1135 + _M0L7_2amul1S1126;
  if (_M0L3lo2S1137 < _M0L5_2aloS1130) {
    _M0L6_2atmpS3327 = 1ull;
  } else {
    _M0L6_2atmpS3327 = 0ull;
  }
  _M0L4mid2S1138 = _M0L6_2atmpS3326 + _M0L6_2atmpS3327;
  if (_M0L4mid2S1138 < _M0L3midS1135) {
    _M0L6_2atmpS3325 = 1ull;
  } else {
    _M0L6_2atmpS3325 = 0ull;
  }
  _M0L3hi2S1139 = _M0L2hiS1136 + _M0L6_2atmpS3325;
  _M0L6_2atmpS3324 = _M0L1jS1141 - 64;
  _M0L6_2atmpS3323 = _M0L6_2atmpS3324 - 1;
  #line 144 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vpS1140
  = _M0FPB13shiftright128(_M0L4mid2S1138, _M0L3hi2S1139, _M0L6_2atmpS3323);
  _M0Lm2vmS1142 = 0ull;
  if (_M0L7mmShiftS1143) {
    uint64_t _M0L3lo3S1144 = _M0L5_2aloS1130 - _M0L7_2amul0S1124;
    uint64_t _M0L6_2atmpS3310 = _M0L3midS1135 - _M0L7_2amul1S1126;
    uint64_t _M0L6_2atmpS3311;
    uint64_t _M0L4mid3S1145;
    uint64_t _M0L6_2atmpS3309;
    uint64_t _M0L3hi3S1146;
    int32_t _M0L6_2atmpS3308;
    int32_t _M0L6_2atmpS3307;
    if (_M0L5_2aloS1130 < _M0L3lo3S1144) {
      _M0L6_2atmpS3311 = 1ull;
    } else {
      _M0L6_2atmpS3311 = 0ull;
    }
    _M0L4mid3S1145 = _M0L6_2atmpS3310 - _M0L6_2atmpS3311;
    if (_M0L3midS1135 < _M0L4mid3S1145) {
      _M0L6_2atmpS3309 = 1ull;
    } else {
      _M0L6_2atmpS3309 = 0ull;
    }
    _M0L3hi3S1146 = _M0L2hiS1136 - _M0L6_2atmpS3309;
    _M0L6_2atmpS3308 = _M0L1jS1141 - 64;
    _M0L6_2atmpS3307 = _M0L6_2atmpS3308 - 1;
    #line 150 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1142
    = _M0FPB13shiftright128(_M0L4mid3S1145, _M0L3hi3S1146, _M0L6_2atmpS3307);
  } else {
    uint64_t _M0L3lo3S1147 = _M0L5_2aloS1130 + _M0L5_2aloS1130;
    uint64_t _M0L6_2atmpS3318 = _M0L3midS1135 + _M0L3midS1135;
    uint64_t _M0L6_2atmpS3319;
    uint64_t _M0L4mid3S1148;
    uint64_t _M0L6_2atmpS3316;
    uint64_t _M0L6_2atmpS3317;
    uint64_t _M0L3hi3S1149;
    uint64_t _M0L3lo4S1150;
    uint64_t _M0L6_2atmpS3314;
    uint64_t _M0L6_2atmpS3315;
    uint64_t _M0L4mid4S1151;
    uint64_t _M0L6_2atmpS3313;
    uint64_t _M0L3hi4S1152;
    int32_t _M0L6_2atmpS3312;
    if (_M0L3lo3S1147 < _M0L5_2aloS1130) {
      _M0L6_2atmpS3319 = 1ull;
    } else {
      _M0L6_2atmpS3319 = 0ull;
    }
    _M0L4mid3S1148 = _M0L6_2atmpS3318 + _M0L6_2atmpS3319;
    _M0L6_2atmpS3316 = _M0L2hiS1136 + _M0L2hiS1136;
    if (_M0L4mid3S1148 < _M0L3midS1135) {
      _M0L6_2atmpS3317 = 1ull;
    } else {
      _M0L6_2atmpS3317 = 0ull;
    }
    _M0L3hi3S1149 = _M0L6_2atmpS3316 + _M0L6_2atmpS3317;
    _M0L3lo4S1150 = _M0L3lo3S1147 - _M0L7_2amul0S1124;
    _M0L6_2atmpS3314 = _M0L4mid3S1148 - _M0L7_2amul1S1126;
    if (_M0L3lo3S1147 < _M0L3lo4S1150) {
      _M0L6_2atmpS3315 = 1ull;
    } else {
      _M0L6_2atmpS3315 = 0ull;
    }
    _M0L4mid4S1151 = _M0L6_2atmpS3314 - _M0L6_2atmpS3315;
    if (_M0L4mid3S1148 < _M0L4mid4S1151) {
      _M0L6_2atmpS3313 = 1ull;
    } else {
      _M0L6_2atmpS3313 = 0ull;
    }
    _M0L3hi4S1152 = _M0L3hi3S1149 - _M0L6_2atmpS3313;
    _M0L6_2atmpS3312 = _M0L1jS1141 - 64;
    #line 158 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1142
    = _M0FPB13shiftright128(_M0L4mid4S1151, _M0L3hi4S1152, _M0L6_2atmpS3312);
  }
  _M0L6_2atmpS3322 = _M0L1jS1141 - 64;
  _M0L6_2atmpS3321 = _M0L6_2atmpS3322 - 1;
  #line 160 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vrS1153
  = _M0FPB13shiftright128(_M0L3midS1135, _M0L2hiS1136, _M0L6_2atmpS3321);
  _M0L6_2atmpS3320 = _M0Lm2vmS1142;
  return (struct _M0TPB19MulShiftAll64Result){.$0 = _M0L2vrS1153,
                                                .$1 = _M0L2vpS1140,
                                                .$2 = _M0L6_2atmpS3320};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS1122,
  int32_t _M0L1pS1123
) {
  uint64_t _M0L6_2atmpS3306;
  uint64_t _M0L6_2atmpS3305;
  uint64_t _M0L6_2atmpS3304;
  #line 124 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3306 = 1ull << (_M0L1pS1123 & 63);
  _M0L6_2atmpS3305 = _M0L6_2atmpS3306 - 1ull;
  _M0L6_2atmpS3304 = _M0L5valueS1122 & _M0L6_2atmpS3305;
  return _M0L6_2atmpS3304 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS1120,
  int32_t _M0L1pS1121
) {
  int32_t _M0L6_2atmpS3303;
  #line 119 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 120 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3303 = _M0FPB10pow5Factor(_M0L5valueS1120);
  return _M0L6_2atmpS3303 >= _M0L1pS1121;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS1115) {
  uint64_t _M0L6_2atmpS3294;
  uint64_t _M0L6_2atmpS3295;
  uint64_t _M0L6_2atmpS3296;
  uint64_t _M0L6_2atmpS3297;
  uint64_t _M0L6_2atmpS3302;
  int32_t _M0L5countS1116;
  uint64_t _M0L1vS1117;
  #line 94 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3294 = _M0L5valueS1115 % 5ull;
  if (_M0L6_2atmpS3294 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3295 = _M0L5valueS1115 % 25ull;
  if (_M0L6_2atmpS3295 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS3296 = _M0L5valueS1115 % 125ull;
  if (_M0L6_2atmpS3296 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS3297 = _M0L5valueS1115 % 625ull;
  if (_M0L6_2atmpS3297 != 0ull) {
    return 3;
  }
  _M0L6_2atmpS3302 = _M0L5valueS1115 / 625ull;
  _M0L5countS1116 = 4;
  _M0L1vS1117 = _M0L6_2atmpS3302;
  while (1) {
    if (_M0L1vS1117 > 0ull) {
      uint64_t _M0L6_2atmpS3298 = _M0L1vS1117 % 5ull;
      int32_t _M0L6_2atmpS3299;
      uint64_t _M0L6_2atmpS3300;
      if (_M0L6_2atmpS3298 != 0ull) {
        return _M0L5countS1116;
      }
      _M0L6_2atmpS3299 = _M0L5countS1116 + 1;
      _M0L6_2atmpS3300 = _M0L1vS1117 / 5ull;
      _M0L5countS1116 = _M0L6_2atmpS3299;
      _M0L1vS1117 = _M0L6_2atmpS3300;
      continue;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1119;
      moonbit_string_t _M0L6_2atmpS3301;
      int32_t _result_4657;
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L18_2astring__builderS1119
      = _M0MPB13StringBuilder21StringBuilder_2einner(25);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1119, (moonbit_string_t)moonbit_string_literal_97.data);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0MPB13StringBuilder13write__objectGmE(_M0L18_2astring__builderS1119, _M0L5valueS1115);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L6_2atmpS3301
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1119);
      moonbit_decref(_M0L18_2astring__builderS1119);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _result_4657 = _M0FPC15abort5abortGiE(_M0L6_2atmpS3301);
      moonbit_decref(_M0L6_2atmpS3301);
      return _result_4657;
    }
    break;
  }
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS1114,
  uint64_t _M0L2hiS1112,
  int32_t _M0L4distS1113
) {
  int32_t _M0L6_2atmpS3293;
  uint64_t _M0L6_2atmpS3291;
  uint64_t _M0L6_2atmpS3292;
  #line 89 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3293 = 64 - _M0L4distS1113;
  _M0L6_2atmpS3291 = _M0L2hiS1112 << (_M0L6_2atmpS3293 & 63);
  _M0L6_2atmpS3292 = _M0L2loS1114 >> (_M0L4distS1113 & 63);
  return _M0L6_2atmpS3291 | _M0L6_2atmpS3292;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS1102,
  uint64_t _M0L1bS1105
) {
  uint64_t _M0L3aLoS1101;
  uint64_t _M0L3aHiS1103;
  uint64_t _M0L3bLoS1104;
  uint64_t _M0L3bHiS1106;
  uint64_t _M0L1xS1107;
  uint64_t _M0L6_2atmpS3289;
  uint64_t _M0L6_2atmpS3290;
  uint64_t _M0L1yS1108;
  uint64_t _M0L6_2atmpS3287;
  uint64_t _M0L6_2atmpS3288;
  uint64_t _M0L1zS1109;
  uint64_t _M0L6_2atmpS3285;
  uint64_t _M0L6_2atmpS3286;
  uint64_t _M0L6_2atmpS3283;
  uint64_t _M0L6_2atmpS3284;
  uint64_t _M0L1wS1110;
  uint64_t _M0L2loS1111;
  #line 74 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3aLoS1101 = _M0L1aS1102 & 4294967295ull;
  _M0L3aHiS1103 = _M0L1aS1102 >> 32;
  _M0L3bLoS1104 = _M0L1bS1105 & 4294967295ull;
  _M0L3bHiS1106 = _M0L1bS1105 >> 32;
  _M0L1xS1107 = _M0L3aLoS1101 * _M0L3bLoS1104;
  _M0L6_2atmpS3289 = _M0L3aHiS1103 * _M0L3bLoS1104;
  _M0L6_2atmpS3290 = _M0L1xS1107 >> 32;
  _M0L1yS1108 = _M0L6_2atmpS3289 + _M0L6_2atmpS3290;
  _M0L6_2atmpS3287 = _M0L3aLoS1101 * _M0L3bHiS1106;
  _M0L6_2atmpS3288 = _M0L1yS1108 & 4294967295ull;
  _M0L1zS1109 = _M0L6_2atmpS3287 + _M0L6_2atmpS3288;
  _M0L6_2atmpS3285 = _M0L3aHiS1103 * _M0L3bHiS1106;
  _M0L6_2atmpS3286 = _M0L1yS1108 >> 32;
  _M0L6_2atmpS3283 = _M0L6_2atmpS3285 + _M0L6_2atmpS3286;
  _M0L6_2atmpS3284 = _M0L1zS1109 >> 32;
  _M0L1wS1110 = _M0L6_2atmpS3283 + _M0L6_2atmpS3284;
  _M0L2loS1111 = _M0L1aS1102 * _M0L1bS1105;
  return (struct _M0TPB7Umul128){.$0 = _M0L2loS1111, .$1 = _M0L1wS1110};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS1099,
  int32_t _M0L4fromS1096,
  int32_t _M0L2toS1095
) {
  int32_t _M0L3lenS1094;
  int32_t _M0L6_2atmpS3282;
  uint16_t* _M0L6bufferS1097;
  int32_t _M0L1iS1098;
  #line 52 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3lenS1094 = _M0L2toS1095 - _M0L4fromS1096;
  #line 54 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3282 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L6bufferS1097
  = (uint16_t*)moonbit_make_string(_M0L3lenS1094, _M0L6_2atmpS3282);
  _M0L1iS1098 = 0;
  while (1) {
    if (_M0L1iS1098 < _M0L3lenS1094) {
      int32_t _M0L6_2atmpS3280 = _M0L4fromS1096 + _M0L1iS1098;
      int32_t _M0L6_2atmpS3279;
      int32_t _M0L6_2atmpS3278;
      int32_t _M0L6_2atmpS3281;
      if (
        _M0L6_2atmpS3280 < 0
        || _M0L6_2atmpS3280 >= Moonbit_array_length(_M0L5bytesS1099)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3279 = (int32_t)_M0L5bytesS1099[_M0L6_2atmpS3280];
      _M0L6_2atmpS3278 = (uint16_t)_M0L6_2atmpS3279;
      if (
        _M0L1iS1098 < 0
        || _M0L1iS1098 >= Moonbit_array_length(_M0L6bufferS1097)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6bufferS1097[_M0L1iS1098] = _M0L6_2atmpS3278;
      _M0L6_2atmpS3281 = _M0L1iS1098 + 1;
      _M0L1iS1098 = _M0L6_2atmpS3281;
      continue;
    }
    break;
  }
  return _M0L6bufferS1097;
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS1093) {
  int32_t _M0L6_2atmpS3277;
  uint32_t _M0L6_2atmpS3276;
  uint32_t _M0L6_2atmpS3275;
  #line 44 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3277 = _M0L1eS1093 * 78913;
  _M0L6_2atmpS3276 = *(uint32_t*)&_M0L6_2atmpS3277;
  _M0L6_2atmpS3275 = _M0L6_2atmpS3276 >> 18;
  return *(int32_t*)&_M0L6_2atmpS3275;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS1092) {
  int32_t _M0L6_2atmpS3274;
  uint32_t _M0L6_2atmpS3273;
  uint32_t _M0L6_2atmpS3272;
  #line 37 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3274 = _M0L1eS1092 * 732923;
  _M0L6_2atmpS3273 = *(uint32_t*)&_M0L6_2atmpS3274;
  _M0L6_2atmpS3272 = _M0L6_2atmpS3273 >> 20;
  return *(int32_t*)&_M0L6_2atmpS3272;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS1090,
  int32_t _M0L8exponentS1091,
  int32_t _M0L8mantissaS1088
) {
  moonbit_string_t _M0L1sS1089;
  moonbit_string_t _result_4660;
  #line 23 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L8mantissaS1088) {
    return (moonbit_string_t)moonbit_string_literal_98.data;
  }
  if (_M0L4signS1090) {
    _M0L1sS1089 = (moonbit_string_t)moonbit_string_literal_95.data;
  } else {
    _M0L1sS1089 = (moonbit_string_t)moonbit_string_literal_96.data;
  }
  if (_M0L8exponentS1091) {
    moonbit_string_t _result_4659;
    #line 29 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4659
    = moonbit_add_string(_M0L1sS1089, (moonbit_string_t)moonbit_string_literal_99.data);
    moonbit_decref(_M0L1sS1089);
    return _result_4659;
  }
  #line 31 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_4660
  = moonbit_add_string(_M0L1sS1089, (moonbit_string_t)moonbit_string_literal_100.data);
  moonbit_decref(_M0L1sS1089);
  return _result_4660;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS1087) {
  int32_t _M0L6_2atmpS3271;
  uint32_t _M0L6_2atmpS3270;
  uint32_t _M0L6_2atmpS3269;
  int32_t _M0L6_2atmpS3268;
  #line 18 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3271 = _M0L1eS1087 * 1217359;
  _M0L6_2atmpS3270 = *(uint32_t*)&_M0L6_2atmpS3271;
  _M0L6_2atmpS3269 = _M0L6_2atmpS3270 >> 19;
  _M0L6_2atmpS3268 = *(int32_t*)&_M0L6_2atmpS3269;
  return _M0L6_2atmpS3268 + 1;
}

int32_t _M0IPC16string6StringPB4Hash4hash(moonbit_string_t _M0L4selfS1083) {
  int32_t _tmp_4661;
  uint32_t _M0L6_2atmpS3267;
  uint32_t _M0Lm3accS1081;
  int32_t _M0L7_2abindS1082;
  int32_t _M0L1iS1084;
  uint32_t _M0L6_2atmpS3266;
  #line 522 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _tmp_4661 = 0;
  _M0L6_2atmpS3267 = *(uint32_t*)&_tmp_4661;
  _M0Lm3accS1081 = _M0L6_2atmpS3267 + 374761393u;
  _M0L7_2abindS1082 = Moonbit_array_length(_M0L4selfS1083);
  _M0L1iS1084 = 0;
  while (1) {
    if (_M0L1iS1084 < _M0L7_2abindS1082) {
      uint32_t _M0L6_2atmpS3261 = _M0Lm3accS1081;
      int32_t _M0L6_2atmpS3264;
      int32_t _M0L6_2atmpS3263;
      uint32_t _M0L1vS1085;
      uint32_t _M0L6_2atmpS3262;
      int32_t _M0L6_2atmpS3265;
      _M0Lm3accS1081 = _M0L6_2atmpS3261 + 4u;
      _M0L6_2atmpS3264 = _M0L4selfS1083[_M0L1iS1084];
      _M0L6_2atmpS3263 = (int32_t)_M0L6_2atmpS3264;
      _M0L1vS1085 = *(uint32_t*)&_M0L6_2atmpS3263;
      _M0L6_2atmpS3262 = _M0Lm3accS1081;
      #line 527 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
      _M0Lm3accS1081 = _M0FPB13consume4__acc(_M0L6_2atmpS3262, _M0L1vS1085);
      _M0L6_2atmpS3265 = _M0L1iS1084 + 1;
      _M0L1iS1084 = _M0L6_2atmpS3265;
      continue;
    }
    break;
  }
  _M0L6_2atmpS3266 = _M0Lm3accS1081;
  #line 529 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  return _M0FPB13finalize__acc(_M0L6_2atmpS3266);
}

struct _M0TUssE* _M0MPB5Iter24nextGssE(
  struct _M0TPB4IterGUssEE* _M0L4selfS1077
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUssEE(_M0L4selfS1077);
}

struct _M0TUsbE* _M0MPB5Iter24nextGsbE(
  struct _M0TPB4IterGUsbEE* _M0L4selfS1078
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsbEE(_M0L4selfS1078);
}

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L4selfS1079
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsRP19moonbitDB10RedisValueEE(_M0L4selfS1079);
}

struct _M0TUsfE* _M0MPB5Iter24nextGsfE(
  struct _M0TPB4IterGUsfEE* _M0L4selfS1080
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsfEE(_M0L4selfS1080);
}

struct _M0TPB4IterGUssEE* _M0MPB3Map5iter2GssE(
  struct _M0TPB3MapGssE* _M0L4selfS1073
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGssE(_M0L4selfS1073);
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map5iter2GsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1074
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsbE(_M0L4selfS1074);
}

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS1075
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsRP19moonbitDB10RedisValueE(_M0L4selfS1075);
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map5iter2GsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1076
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsfE(_M0L4selfS1076);
}

struct _M0TPB4IterGUssEE* _M0MPB3Map4iterGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1030
) {
  struct _M0TPB5EntryGssE* _M0L4headS3230;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS1029;
  int32_t _M0L3lenS1031;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1032;
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3223__l711__* _closure_4663;
  struct _M0TWEOUssE* _M0L6_2atmpS3221;
  int64_t _M0L6_2atmpS3222;
  struct _M0TPB4IterGUssEE* _result_4664;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3230 = _M0L4selfS1030->$5;
  if (_M0L4headS3230) {
    moonbit_incref(_M0L4headS3230);
  }
  _M0L11curr__entryS1029
  = (struct _M0TPB8MutLocalGORPB5EntryGssEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGssEE));
  Moonbit_object_header(_M0L11curr__entryS1029)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 39, 0);
  _M0L11curr__entryS1029->$0 = _M0L4headS3230;
  _M0L3lenS1031 = _M0L4selfS1030->$1;
  _M0L9remainingS1032
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1032)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1032->$0 = _M0L3lenS1031;
  _closure_4663
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3223__l711__*)moonbit_malloc(sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3223__l711__));
  Moonbit_object_header(_closure_4663)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 42, 0);
  _closure_4663->code = &_M0MPB3Map4iterGssEC3223l711;
  _closure_4663->$0 = _M0L9remainingS1032;
  _closure_4663->$1 = _M0L11curr__entryS1029;
  _M0L6_2atmpS3221 = (struct _M0TWEOUssE*)_closure_4663;
  _M0L6_2atmpS3222 = (int64_t)_M0L3lenS1031;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4664 = _M0MPB4Iter3newGUssEE(_M0L6_2atmpS3221, _M0L6_2atmpS3222);
  moonbit_decref(_M0L6_2atmpS3221);
  return _result_4664;
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1041
) {
  struct _M0TPB5EntryGsbE* _M0L4headS3240;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS1040;
  int32_t _M0L3lenS1042;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1043;
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3233__l711__* _closure_4665;
  struct _M0TWEOUsbE* _M0L6_2atmpS3231;
  int64_t _M0L6_2atmpS3232;
  struct _M0TPB4IterGUsbEE* _result_4666;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3240 = _M0L4selfS1041->$5;
  if (_M0L4headS3240) {
    moonbit_incref(_M0L4headS3240);
  }
  _M0L11curr__entryS1040
  = (struct _M0TPB8MutLocalGORPB5EntryGsbEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE));
  Moonbit_object_header(_M0L11curr__entryS1040)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 46, 0);
  _M0L11curr__entryS1040->$0 = _M0L4headS3240;
  _M0L3lenS1042 = _M0L4selfS1041->$1;
  _M0L9remainingS1043
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1043)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1043->$0 = _M0L3lenS1042;
  _closure_4665
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3233__l711__*)moonbit_malloc(sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3233__l711__));
  Moonbit_object_header(_closure_4665)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 49, 0);
  _closure_4665->code = &_M0MPB3Map4iterGsbEC3233l711;
  _closure_4665->$0 = _M0L9remainingS1043;
  _closure_4665->$1 = _M0L11curr__entryS1040;
  _M0L6_2atmpS3231 = (struct _M0TWEOUsbE*)_closure_4665;
  _M0L6_2atmpS3232 = (int64_t)_M0L3lenS1042;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4666 = _M0MPB4Iter3newGUsbEE(_M0L6_2atmpS3231, _M0L6_2atmpS3232);
  moonbit_decref(_M0L6_2atmpS3231);
  return _result_4666;
}

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB3Map4iterGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS1052
) {
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4headS3250;
  struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE* _M0L11curr__entryS1051;
  int32_t _M0L3lenS1053;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1054;
  struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3243__l711__* _closure_4667;
  struct _M0TWEOUsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3241;
  int64_t _M0L6_2atmpS3242;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _result_4668;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3250 = _M0L4selfS1052->$5;
  if (_M0L4headS3250) {
    moonbit_incref(_M0L4headS3250);
  }
  _M0L11curr__entryS1051
  = (struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE));
  Moonbit_object_header(_M0L11curr__entryS1051)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 53, 0);
  _M0L11curr__entryS1051->$0 = _M0L4headS3250;
  _M0L3lenS1053 = _M0L4selfS1052->$1;
  _M0L9remainingS1054
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1054)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1054->$0 = _M0L3lenS1053;
  _closure_4667
  = (struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3243__l711__*)moonbit_malloc(sizeof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3243__l711__));
  Moonbit_object_header(_closure_4667)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 56, 0);
  _closure_4667->code = &_M0MPB3Map4iterGsRP19moonbitDB10RedisValueEC3243l711;
  _closure_4667->$0 = _M0L9remainingS1054;
  _closure_4667->$1 = _M0L11curr__entryS1051;
  _M0L6_2atmpS3241
  = (struct _M0TWEOUsRP19moonbitDB10RedisValueE*)_closure_4667;
  _M0L6_2atmpS3242 = (int64_t)_M0L3lenS1053;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4668
  = _M0MPB4Iter3newGUsRP19moonbitDB10RedisValueEE(_M0L6_2atmpS3241, _M0L6_2atmpS3242);
  moonbit_decref(_M0L6_2atmpS3241);
  return _result_4668;
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1063
) {
  struct _M0TPB5EntryGsfE* _M0L4headS3260;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS1062;
  int32_t _M0L3lenS1064;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1065;
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3253__l711__* _closure_4669;
  struct _M0TWEOUsfE* _M0L6_2atmpS3251;
  int64_t _M0L6_2atmpS3252;
  struct _M0TPB4IterGUsfEE* _result_4670;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3260 = _M0L4selfS1063->$5;
  if (_M0L4headS3260) {
    moonbit_incref(_M0L4headS3260);
  }
  _M0L11curr__entryS1062
  = (struct _M0TPB8MutLocalGORPB5EntryGsfEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE));
  Moonbit_object_header(_M0L11curr__entryS1062)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 60, 0);
  _M0L11curr__entryS1062->$0 = _M0L4headS3260;
  _M0L3lenS1064 = _M0L4selfS1063->$1;
  _M0L9remainingS1065
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1065)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1065->$0 = _M0L3lenS1064;
  _closure_4669
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3253__l711__*)moonbit_malloc(sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3253__l711__));
  Moonbit_object_header(_closure_4669)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 63, 0);
  _closure_4669->code = &_M0MPB3Map4iterGsfEC3253l711;
  _closure_4669->$0 = _M0L9remainingS1065;
  _closure_4669->$1 = _M0L11curr__entryS1062;
  _M0L6_2atmpS3251 = (struct _M0TWEOUsfE*)_closure_4669;
  _M0L6_2atmpS3252 = (int64_t)_M0L3lenS1064;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4670 = _M0MPB4Iter3newGUsfEE(_M0L6_2atmpS3251, _M0L6_2atmpS3252);
  moonbit_decref(_M0L6_2atmpS3251);
  return _result_4670;
}

struct _M0TUsfE* _M0MPB3Map4iterGsfEC3253l711(
  struct _M0TWEOUsfE* _M0L6_2aenvS3254
) {
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3253__l711__* _M0L14_2acasted__envS3255;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS1062;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1065;
  int32_t _M0L3valS3256;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3255
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3253__l711__*)_M0L6_2aenvS3254;
  _M0L11curr__entryS1062 = _M0L14_2acasted__envS3255->$1;
  _M0L9remainingS1065 = _M0L14_2acasted__envS3255->$0;
  _M0L3valS3256 = _M0L9remainingS1065->$0;
  if (_M0L3valS3256 > 0) {
    struct _M0TPB5EntryGsfE* _M0L7_2abindS1067 = _M0L11curr__entryS1062->$0;
    if (_M0L7_2abindS1067 == 0) {
      goto join_1066;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS1068 = _M0L7_2abindS1067;
      struct _M0TPB5EntryGsfE* _M0L4_2axS1069 = _M0L7_2aSomeS1068;
      moonbit_string_t _M0L6_2akeyS1070 = _M0L4_2axS1069->$4;
      float _M0L8_2avalueS1071 = _M0L4_2axS1069->$5;
      struct _M0TPB5EntryGsfE* _M0L7_2anextS1072 = _M0L4_2axS1069->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4093 = _M0L11curr__entryS1062->$0;
      int32_t _M0L3valS3258;
      int32_t _M0L6_2atmpS3257;
      struct _M0TUsfE* _M0L8_2atupleS3259;
      if (_M0L7_2anextS1072) {
        moonbit_incref(_M0L7_2anextS1072);
      }
      moonbit_incref(_M0L6_2akeyS1070);
      if (_M0L6_2aoldS4093) {
        moonbit_decref(_M0L6_2aoldS4093);
      }
      _M0L11curr__entryS1062->$0 = _M0L7_2anextS1072;
      _M0L3valS3258 = _M0L9remainingS1065->$0;
      _M0L6_2atmpS3257 = _M0L3valS3258 - 1;
      _M0L9remainingS1065->$0 = _M0L6_2atmpS3257;
      _M0L8_2atupleS3259
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS3259)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 13, 0);
      _M0L8_2atupleS3259->$0 = _M0L6_2akeyS1070;
      _M0L8_2atupleS3259->$1 = _M0L8_2avalueS1071;
      return _M0L8_2atupleS3259;
    }
  } else {
    goto join_1066;
  }
  join_1066:;
  return 0;
}

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB3Map4iterGsRP19moonbitDB10RedisValueEC3243l711(
  struct _M0TWEOUsRP19moonbitDB10RedisValueE* _M0L6_2aenvS3244
) {
  struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3243__l711__* _M0L14_2acasted__envS3245;
  struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE* _M0L11curr__entryS1051;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1054;
  int32_t _M0L3valS3246;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3245
  = (struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3243__l711__*)_M0L6_2aenvS3244;
  _M0L11curr__entryS1051 = _M0L14_2acasted__envS3245->$1;
  _M0L9remainingS1054 = _M0L14_2acasted__envS3245->$0;
  _M0L3valS3246 = _M0L9remainingS1054->$0;
  if (_M0L3valS3246 > 0) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS1056 =
      _M0L11curr__entryS1051->$0;
    if (_M0L7_2abindS1056 == 0) {
      goto join_1055;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1057 =
        _M0L7_2abindS1056;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4_2axS1058 =
        _M0L7_2aSomeS1057;
      moonbit_string_t _M0L6_2akeyS1059 = _M0L4_2axS1058->$4;
      void* _M0L8_2avalueS1060 = _M0L4_2axS1058->$5;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2anextS1061 =
        _M0L4_2axS1058->$1;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4097 =
        _M0L11curr__entryS1051->$0;
      int32_t _M0L3valS3248;
      int32_t _M0L6_2atmpS3247;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L8_2atupleS3249;
      if (_M0L7_2anextS1061) {
        moonbit_incref(_M0L7_2anextS1061);
      }
      moonbit_incref(_M0L8_2avalueS1060);
      moonbit_incref(_M0L6_2akeyS1059);
      if (_M0L6_2aoldS4097) {
        moonbit_decref(_M0L6_2aoldS4097);
      }
      _M0L11curr__entryS1051->$0 = _M0L7_2anextS1061;
      _M0L3valS3248 = _M0L9remainingS1054->$0;
      _M0L6_2atmpS3247 = _M0L3valS3248 - 1;
      _M0L9remainingS1054->$0 = _M0L6_2atmpS3247;
      _M0L8_2atupleS3249
      = (struct _M0TUsRP19moonbitDB10RedisValueE*)moonbit_malloc(sizeof(struct _M0TUsRP19moonbitDB10RedisValueE));
      Moonbit_object_header(_M0L8_2atupleS3249)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 67, 0);
      _M0L8_2atupleS3249->$0 = _M0L6_2akeyS1059;
      _M0L8_2atupleS3249->$1 = _M0L8_2avalueS1060;
      return _M0L8_2atupleS3249;
    }
  } else {
    goto join_1055;
  }
  join_1055:;
  return 0;
}

struct _M0TUsbE* _M0MPB3Map4iterGsbEC3233l711(
  struct _M0TWEOUsbE* _M0L6_2aenvS3234
) {
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3233__l711__* _M0L14_2acasted__envS3235;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS1040;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1043;
  int32_t _M0L3valS3236;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3235
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3233__l711__*)_M0L6_2aenvS3234;
  _M0L11curr__entryS1040 = _M0L14_2acasted__envS3235->$1;
  _M0L9remainingS1043 = _M0L14_2acasted__envS3235->$0;
  _M0L3valS3236 = _M0L9remainingS1043->$0;
  if (_M0L3valS3236 > 0) {
    struct _M0TPB5EntryGsbE* _M0L7_2abindS1045 = _M0L11curr__entryS1040->$0;
    if (_M0L7_2abindS1045 == 0) {
      goto join_1044;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS1046 = _M0L7_2abindS1045;
      struct _M0TPB5EntryGsbE* _M0L4_2axS1047 = _M0L7_2aSomeS1046;
      moonbit_string_t _M0L6_2akeyS1048 = _M0L4_2axS1047->$4;
      int32_t _M0L8_2avalueS1049 = _M0L4_2axS1047->$5;
      struct _M0TPB5EntryGsbE* _M0L7_2anextS1050 = _M0L4_2axS1047->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4102 = _M0L11curr__entryS1040->$0;
      int32_t _M0L3valS3238;
      int32_t _M0L6_2atmpS3237;
      struct _M0TUsbE* _M0L8_2atupleS3239;
      if (_M0L7_2anextS1050) {
        moonbit_incref(_M0L7_2anextS1050);
      }
      moonbit_incref(_M0L6_2akeyS1048);
      if (_M0L6_2aoldS4102) {
        moonbit_decref(_M0L6_2aoldS4102);
      }
      _M0L11curr__entryS1040->$0 = _M0L7_2anextS1050;
      _M0L3valS3238 = _M0L9remainingS1043->$0;
      _M0L6_2atmpS3237 = _M0L3valS3238 - 1;
      _M0L9remainingS1043->$0 = _M0L6_2atmpS3237;
      _M0L8_2atupleS3239
      = (struct _M0TUsbE*)moonbit_malloc(sizeof(struct _M0TUsbE));
      Moonbit_object_header(_M0L8_2atupleS3239)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 71, 0);
      _M0L8_2atupleS3239->$0 = _M0L6_2akeyS1048;
      _M0L8_2atupleS3239->$1 = _M0L8_2avalueS1049;
      return _M0L8_2atupleS3239;
    }
  } else {
    goto join_1044;
  }
  join_1044:;
  return 0;
}

struct _M0TUssE* _M0MPB3Map4iterGssEC3223l711(
  struct _M0TWEOUssE* _M0L6_2aenvS3224
) {
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3223__l711__* _M0L14_2acasted__envS3225;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS1029;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1032;
  int32_t _M0L3valS3226;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3225
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3223__l711__*)_M0L6_2aenvS3224;
  _M0L11curr__entryS1029 = _M0L14_2acasted__envS3225->$1;
  _M0L9remainingS1032 = _M0L14_2acasted__envS3225->$0;
  _M0L3valS3226 = _M0L9remainingS1032->$0;
  if (_M0L3valS3226 > 0) {
    struct _M0TPB5EntryGssE* _M0L7_2abindS1034 = _M0L11curr__entryS1029->$0;
    if (_M0L7_2abindS1034 == 0) {
      goto join_1033;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS1035 = _M0L7_2abindS1034;
      struct _M0TPB5EntryGssE* _M0L4_2axS1036 = _M0L7_2aSomeS1035;
      moonbit_string_t _M0L6_2akeyS1037 = _M0L4_2axS1036->$4;
      moonbit_string_t _M0L8_2avalueS1038 = _M0L4_2axS1036->$5;
      struct _M0TPB5EntryGssE* _M0L7_2anextS1039 = _M0L4_2axS1036->$1;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4106 = _M0L11curr__entryS1029->$0;
      int32_t _M0L3valS3228;
      int32_t _M0L6_2atmpS3227;
      struct _M0TUssE* _M0L8_2atupleS3229;
      if (_M0L7_2anextS1039) {
        moonbit_incref(_M0L7_2anextS1039);
      }
      moonbit_incref(_M0L8_2avalueS1038);
      moonbit_incref(_M0L6_2akeyS1037);
      if (_M0L6_2aoldS4106) {
        moonbit_decref(_M0L6_2aoldS4106);
      }
      _M0L11curr__entryS1029->$0 = _M0L7_2anextS1039;
      _M0L3valS3228 = _M0L9remainingS1032->$0;
      _M0L6_2atmpS3227 = _M0L3valS3228 - 1;
      _M0L9remainingS1032->$0 = _M0L6_2atmpS3227;
      _M0L8_2atupleS3229
      = (struct _M0TUssE*)moonbit_malloc(sizeof(struct _M0TUssE));
      Moonbit_object_header(_M0L8_2atupleS3229)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 74, 0);
      _M0L8_2atupleS3229->$0 = _M0L6_2akeyS1037;
      _M0L8_2atupleS3229->$1 = _M0L8_2avalueS1038;
      return _M0L8_2atupleS3229;
    }
  } else {
    goto join_1033;
  }
  join_1033:;
  return 0;
}

int32_t _M0MPB3Map6lengthGssE(struct _M0TPB3MapGssE* _M0L4selfS1026) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS1026->$1;
}

int32_t _M0MPB3Map6lengthGsbE(struct _M0TPB3MapGsbE* _M0L4selfS1027) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS1027->$1;
}

int32_t _M0MPB3Map6lengthGsfE(struct _M0TPB3MapGsfE* _M0L4selfS1028) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS1028->$1;
}

int32_t _M0MPB3Map6removeGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS1018,
  moonbit_string_t _M0L3keyS1019
) {
  int32_t _M0L6_2atmpS3217;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3217 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1019);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsiE(_M0L4selfS1018, _M0L3keyS1019, _M0L6_2atmpS3217);
  return 0;
}

int32_t _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS1020,
  moonbit_string_t _M0L3keyS1021
) {
  int32_t _M0L6_2atmpS3218;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3218 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1021);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsRP19moonbitDB10RedisValueE(_M0L4selfS1020, _M0L3keyS1021, _M0L6_2atmpS3218);
  return 0;
}

int32_t _M0MPB3Map6removeGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1022,
  moonbit_string_t _M0L3keyS1023
) {
  int32_t _M0L6_2atmpS3219;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3219 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1023);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsfE(_M0L4selfS1022, _M0L3keyS1023, _M0L6_2atmpS3219);
  return 0;
}

int32_t _M0MPB3Map6removeGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1024,
  moonbit_string_t _M0L3keyS1025
) {
  int32_t _M0L6_2atmpS3220;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3220 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS1025);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsbE(_M0L4selfS1024, _M0L3keyS1025, _M0L6_2atmpS3220);
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS985,
  moonbit_string_t _M0L3keyS989,
  int32_t _M0L4hashS988
) {
  int32_t _M0L14capacity__maskS3180;
  int32_t _M0L6_2atmpS3179;
  int32_t _M0L1iS982;
  int32_t _M0L3idxS983;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3180 = _M0L4selfS985->$3;
  _M0L6_2atmpS3179 = _M0L4hashS988 & _M0L14capacity__maskS3180;
  _M0L1iS982 = 0;
  _M0L3idxS983 = _M0L6_2atmpS3179;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3178 = _M0L4selfS985->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS984;
    if (
      _M0L3idxS983 < 0
      || _M0L3idxS983 >= Moonbit_array_length(_M0L7entriesS3178)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS984
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3178[_M0L3idxS983];
    if (_M0L7_2abindS984 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS986 = _M0L7_2abindS984;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS987 = _M0L7_2aSomeS986;
      int32_t _M0L4hashS3170 = _M0L8_2aentryS987->$3;
      int32_t _if__result_4676;
      int32_t _M0L3pslS3173;
      int32_t _M0L6_2atmpS3174;
      int32_t _M0L6_2atmpS3176;
      int32_t _M0L14capacity__maskS3177;
      int32_t _M0L6_2atmpS3175;
      if (_M0L4hashS3170 == _M0L4hashS988) {
        moonbit_string_t _M0L3keyS3169 = _M0L8_2aentryS987->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4676
        = _M0L3keyS3169 == _M0L3keyS989
          || Moonbit_array_length(_M0L3keyS3169)
             == Moonbit_array_length(_M0L3keyS989)
             && 0
                == memcmp(_M0L3keyS3169, _M0L3keyS989, Moonbit_array_length(_M0L3keyS3169) * 2);
      } else {
        _if__result_4676 = 0;
      }
      if (_if__result_4676) {
        int32_t _M0L4sizeS3172;
        int32_t _M0L6_2atmpS3171;
        moonbit_incref(_M0L8_2aentryS987);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsiE(_M0L4selfS985, _M0L8_2aentryS987);
        moonbit_decref(_M0L8_2aentryS987);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsiE(_M0L4selfS985, _M0L3idxS983);
        _M0L4sizeS3172 = _M0L4selfS985->$1;
        _M0L6_2atmpS3171 = _M0L4sizeS3172 - 1;
        _M0L4selfS985->$1 = _M0L6_2atmpS3171;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS987);
      }
      _M0L3pslS3173 = _M0L8_2aentryS987->$2;
      moonbit_decref(_M0L8_2aentryS987);
      if (_M0L1iS982 > _M0L3pslS3173) {
        break;
      }
      _M0L6_2atmpS3174 = _M0L1iS982 + 1;
      _M0L6_2atmpS3176 = _M0L3idxS983 + 1;
      _M0L14capacity__maskS3177 = _M0L4selfS985->$3;
      _M0L6_2atmpS3175 = _M0L6_2atmpS3176 & _M0L14capacity__maskS3177;
      _M0L1iS982 = _M0L6_2atmpS3174;
      _M0L3idxS983 = _M0L6_2atmpS3175;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS994,
  moonbit_string_t _M0L3keyS998,
  int32_t _M0L4hashS997
) {
  int32_t _M0L14capacity__maskS3192;
  int32_t _M0L6_2atmpS3191;
  int32_t _M0L1iS991;
  int32_t _M0L3idxS992;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3192 = _M0L4selfS994->$3;
  _M0L6_2atmpS3191 = _M0L4hashS997 & _M0L14capacity__maskS3192;
  _M0L1iS991 = 0;
  _M0L3idxS992 = _M0L6_2atmpS3191;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3190 =
      _M0L4selfS994->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS993;
    if (
      _M0L3idxS992 < 0
      || _M0L3idxS992 >= Moonbit_array_length(_M0L7entriesS3190)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS993
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3190[
        _M0L3idxS992
      ];
    if (_M0L7_2abindS993 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS995 =
        _M0L7_2abindS993;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L8_2aentryS996 =
        _M0L7_2aSomeS995;
      int32_t _M0L4hashS3182 = _M0L8_2aentryS996->$3;
      int32_t _if__result_4678;
      int32_t _M0L3pslS3185;
      int32_t _M0L6_2atmpS3186;
      int32_t _M0L6_2atmpS3188;
      int32_t _M0L14capacity__maskS3189;
      int32_t _M0L6_2atmpS3187;
      if (_M0L4hashS3182 == _M0L4hashS997) {
        moonbit_string_t _M0L3keyS3181 = _M0L8_2aentryS996->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4678
        = _M0L3keyS3181 == _M0L3keyS998
          || Moonbit_array_length(_M0L3keyS3181)
             == Moonbit_array_length(_M0L3keyS998)
             && 0
                == memcmp(_M0L3keyS3181, _M0L3keyS998, Moonbit_array_length(_M0L3keyS3181) * 2);
      } else {
        _if__result_4678 = 0;
      }
      if (_if__result_4678) {
        int32_t _M0L4sizeS3184;
        int32_t _M0L6_2atmpS3183;
        moonbit_incref(_M0L8_2aentryS996);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS994, _M0L8_2aentryS996);
        moonbit_decref(_M0L8_2aentryS996);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsRP19moonbitDB10RedisValueE(_M0L4selfS994, _M0L3idxS992);
        _M0L4sizeS3184 = _M0L4selfS994->$1;
        _M0L6_2atmpS3183 = _M0L4sizeS3184 - 1;
        _M0L4selfS994->$1 = _M0L6_2atmpS3183;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS996);
      }
      _M0L3pslS3185 = _M0L8_2aentryS996->$2;
      moonbit_decref(_M0L8_2aentryS996);
      if (_M0L1iS991 > _M0L3pslS3185) {
        break;
      }
      _M0L6_2atmpS3186 = _M0L1iS991 + 1;
      _M0L6_2atmpS3188 = _M0L3idxS992 + 1;
      _M0L14capacity__maskS3189 = _M0L4selfS994->$3;
      _M0L6_2atmpS3187 = _M0L6_2atmpS3188 & _M0L14capacity__maskS3189;
      _M0L1iS991 = _M0L6_2atmpS3186;
      _M0L3idxS992 = _M0L6_2atmpS3187;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1003,
  moonbit_string_t _M0L3keyS1007,
  int32_t _M0L4hashS1006
) {
  int32_t _M0L14capacity__maskS3204;
  int32_t _M0L6_2atmpS3203;
  int32_t _M0L1iS1000;
  int32_t _M0L3idxS1001;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3204 = _M0L4selfS1003->$3;
  _M0L6_2atmpS3203 = _M0L4hashS1006 & _M0L14capacity__maskS3204;
  _M0L1iS1000 = 0;
  _M0L3idxS1001 = _M0L6_2atmpS3203;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS3202 = _M0L4selfS1003->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS1002;
    if (
      _M0L3idxS1001 < 0
      || _M0L3idxS1001 >= Moonbit_array_length(_M0L7entriesS3202)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1002
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3202[_M0L3idxS1001];
    if (_M0L7_2abindS1002 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS1004 = _M0L7_2abindS1002;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS1005 = _M0L7_2aSomeS1004;
      int32_t _M0L4hashS3194 = _M0L8_2aentryS1005->$3;
      int32_t _if__result_4680;
      int32_t _M0L3pslS3197;
      int32_t _M0L6_2atmpS3198;
      int32_t _M0L6_2atmpS3200;
      int32_t _M0L14capacity__maskS3201;
      int32_t _M0L6_2atmpS3199;
      if (_M0L4hashS3194 == _M0L4hashS1006) {
        moonbit_string_t _M0L3keyS3193 = _M0L8_2aentryS1005->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4680
        = _M0L3keyS3193 == _M0L3keyS1007
          || Moonbit_array_length(_M0L3keyS3193)
             == Moonbit_array_length(_M0L3keyS1007)
             && 0
                == memcmp(_M0L3keyS3193, _M0L3keyS1007, Moonbit_array_length(_M0L3keyS3193) * 2);
      } else {
        _if__result_4680 = 0;
      }
      if (_if__result_4680) {
        int32_t _M0L4sizeS3196;
        int32_t _M0L6_2atmpS3195;
        moonbit_incref(_M0L8_2aentryS1005);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsfE(_M0L4selfS1003, _M0L8_2aentryS1005);
        moonbit_decref(_M0L8_2aentryS1005);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsfE(_M0L4selfS1003, _M0L3idxS1001);
        _M0L4sizeS3196 = _M0L4selfS1003->$1;
        _M0L6_2atmpS3195 = _M0L4sizeS3196 - 1;
        _M0L4selfS1003->$1 = _M0L6_2atmpS3195;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS1005);
      }
      _M0L3pslS3197 = _M0L8_2aentryS1005->$2;
      moonbit_decref(_M0L8_2aentryS1005);
      if (_M0L1iS1000 > _M0L3pslS3197) {
        break;
      }
      _M0L6_2atmpS3198 = _M0L1iS1000 + 1;
      _M0L6_2atmpS3200 = _M0L3idxS1001 + 1;
      _M0L14capacity__maskS3201 = _M0L4selfS1003->$3;
      _M0L6_2atmpS3199 = _M0L6_2atmpS3200 & _M0L14capacity__maskS3201;
      _M0L1iS1000 = _M0L6_2atmpS3198;
      _M0L3idxS1001 = _M0L6_2atmpS3199;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1012,
  moonbit_string_t _M0L3keyS1016,
  int32_t _M0L4hashS1015
) {
  int32_t _M0L14capacity__maskS3216;
  int32_t _M0L6_2atmpS3215;
  int32_t _M0L1iS1009;
  int32_t _M0L3idxS1010;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3216 = _M0L4selfS1012->$3;
  _M0L6_2atmpS3215 = _M0L4hashS1015 & _M0L14capacity__maskS3216;
  _M0L1iS1009 = 0;
  _M0L3idxS1010 = _M0L6_2atmpS3215;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS3214 = _M0L4selfS1012->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS1011;
    if (
      _M0L3idxS1010 < 0
      || _M0L3idxS1010 >= Moonbit_array_length(_M0L7entriesS3214)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS1011
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3214[_M0L3idxS1010];
    if (_M0L7_2abindS1011 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS1013 = _M0L7_2abindS1011;
      struct _M0TPB5EntryGsbE* _M0L8_2aentryS1014 = _M0L7_2aSomeS1013;
      int32_t _M0L4hashS3206 = _M0L8_2aentryS1014->$3;
      int32_t _if__result_4682;
      int32_t _M0L3pslS3209;
      int32_t _M0L6_2atmpS3210;
      int32_t _M0L6_2atmpS3212;
      int32_t _M0L14capacity__maskS3213;
      int32_t _M0L6_2atmpS3211;
      if (_M0L4hashS3206 == _M0L4hashS1015) {
        moonbit_string_t _M0L3keyS3205 = _M0L8_2aentryS1014->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4682
        = _M0L3keyS3205 == _M0L3keyS1016
          || Moonbit_array_length(_M0L3keyS3205)
             == Moonbit_array_length(_M0L3keyS1016)
             && 0
                == memcmp(_M0L3keyS3205, _M0L3keyS1016, Moonbit_array_length(_M0L3keyS3205) * 2);
      } else {
        _if__result_4682 = 0;
      }
      if (_if__result_4682) {
        int32_t _M0L4sizeS3208;
        int32_t _M0L6_2atmpS3207;
        moonbit_incref(_M0L8_2aentryS1014);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsbE(_M0L4selfS1012, _M0L8_2aentryS1014);
        moonbit_decref(_M0L8_2aentryS1014);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsbE(_M0L4selfS1012, _M0L3idxS1010);
        _M0L4sizeS3208 = _M0L4selfS1012->$1;
        _M0L6_2atmpS3207 = _M0L4sizeS3208 - 1;
        _M0L4selfS1012->$1 = _M0L6_2atmpS3207;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS1014);
      }
      _M0L3pslS3209 = _M0L8_2aentryS1014->$2;
      moonbit_decref(_M0L8_2aentryS1014);
      if (_M0L1iS1009 > _M0L3pslS3209) {
        break;
      }
      _M0L6_2atmpS3210 = _M0L1iS1009 + 1;
      _M0L6_2atmpS3212 = _M0L3idxS1010 + 1;
      _M0L14capacity__maskS3213 = _M0L4selfS1012->$3;
      _M0L6_2atmpS3211 = _M0L6_2atmpS3212 & _M0L14capacity__maskS3213;
      _M0L1iS1009 = _M0L6_2atmpS3210;
      _M0L3idxS1010 = _M0L6_2atmpS3211;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS944,
  int32_t _M0L3idxS951
) {
  int32_t _M0L3curS942;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS942 = _M0L3idxS951;
  _2afor_946:;
  while (1) {
    int32_t _M0L6_2atmpS3146 = _M0L3curS942 + 1;
    int32_t _M0L14capacity__maskS3147 = _M0L4selfS944->$3;
    int32_t _M0L4nextS943 = _M0L6_2atmpS3146 & _M0L14capacity__maskS3147;
    struct _M0TPB5EntryGsiE** _M0L7entriesS3145 = _M0L4selfS944->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS947;
    struct _M0TPB5EntryGsiE** _M0L7entriesS3141;
    struct _M0TPB5EntryGsiE* _M0L6_2atmpS3142;
    struct _M0TPB5EntryGsiE* _M0L6_2aoldS4123;
    int32_t _tmp_4685;
    if (
      _M0L4nextS943 < 0
      || _M0L4nextS943 >= Moonbit_array_length(_M0L7entriesS3145)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS947
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3145[_M0L4nextS943];
    if (_M0L7_2abindS947 == 0) {
      goto join_945;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS948 = _M0L7_2abindS947;
      struct _M0TPB5EntryGsiE* _M0L4_2axS949 = _M0L7_2aSomeS948;
      int32_t _M0L4_2axS950 = _M0L4_2axS949->$2;
      switch (_M0L4_2axS950) {
        case 0: {
          goto join_945;
          break;
        }
        default: {
          int32_t _M0L3pslS3144 = _M0L4_2axS949->$2;
          int32_t _M0L6_2atmpS3143 = _M0L3pslS3144 - 1;
          _M0L4_2axS949->$2 = _M0L6_2atmpS3143;
          moonbit_incref(_M0L4_2axS949);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsiE(_M0L4selfS944, _M0L4_2axS949, _M0L3curS942);
          moonbit_decref(_M0L4_2axS949);
          _M0L3curS942 = _M0L4nextS943;
          goto _2afor_946;
          break;
        }
      }
    }
    goto joinlet_4684;
    join_945:;
    _M0L7entriesS3141 = _M0L4selfS944->$0;
    _M0L6_2atmpS3142 = 0;
    if (
      _M0L3curS942 < 0
      || _M0L3curS942 >= Moonbit_array_length(_M0L7entriesS3141)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4123
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3141[_M0L3curS942];
    if (_M0L6_2aoldS4123) {
      moonbit_decref(_M0L6_2aoldS4123);
    }
    _M0L7entriesS3141[_M0L3curS942] = _M0L6_2atmpS3142;
    break;
    joinlet_4684:;
    _tmp_4685 = _M0L3curS942;
    _M0L3curS942 = _tmp_4685;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS954,
  int32_t _M0L3idxS961
) {
  int32_t _M0L3curS952;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS952 = _M0L3idxS961;
  _2afor_956:;
  while (1) {
    int32_t _M0L6_2atmpS3153 = _M0L3curS952 + 1;
    int32_t _M0L14capacity__maskS3154 = _M0L4selfS954->$3;
    int32_t _M0L4nextS953 = _M0L6_2atmpS3153 & _M0L14capacity__maskS3154;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3152 =
      _M0L4selfS954->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS957;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3148;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3149;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4127;
    int32_t _tmp_4688;
    if (
      _M0L4nextS953 < 0
      || _M0L4nextS953 >= Moonbit_array_length(_M0L7entriesS3152)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS957
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3152[
        _M0L4nextS953
      ];
    if (_M0L7_2abindS957 == 0) {
      goto join_955;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS958 =
        _M0L7_2abindS957;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4_2axS959 =
        _M0L7_2aSomeS958;
      int32_t _M0L4_2axS960 = _M0L4_2axS959->$2;
      switch (_M0L4_2axS960) {
        case 0: {
          goto join_955;
          break;
        }
        default: {
          int32_t _M0L3pslS3151 = _M0L4_2axS959->$2;
          int32_t _M0L6_2atmpS3150 = _M0L3pslS3151 - 1;
          _M0L4_2axS959->$2 = _M0L6_2atmpS3150;
          moonbit_incref(_M0L4_2axS959);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS954, _M0L4_2axS959, _M0L3curS952);
          moonbit_decref(_M0L4_2axS959);
          _M0L3curS952 = _M0L4nextS953;
          goto _2afor_956;
          break;
        }
      }
    }
    goto joinlet_4687;
    join_955:;
    _M0L7entriesS3148 = _M0L4selfS954->$0;
    _M0L6_2atmpS3149 = 0;
    if (
      _M0L3curS952 < 0
      || _M0L3curS952 >= Moonbit_array_length(_M0L7entriesS3148)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4127
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3148[
        _M0L3curS952
      ];
    if (_M0L6_2aoldS4127) {
      moonbit_decref(_M0L6_2aoldS4127);
    }
    _M0L7entriesS3148[_M0L3curS952] = _M0L6_2atmpS3149;
    break;
    joinlet_4687:;
    _tmp_4688 = _M0L3curS952;
    _M0L3curS952 = _tmp_4688;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS964,
  int32_t _M0L3idxS971
) {
  int32_t _M0L3curS962;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS962 = _M0L3idxS971;
  _2afor_966:;
  while (1) {
    int32_t _M0L6_2atmpS3160 = _M0L3curS962 + 1;
    int32_t _M0L14capacity__maskS3161 = _M0L4selfS964->$3;
    int32_t _M0L4nextS963 = _M0L6_2atmpS3160 & _M0L14capacity__maskS3161;
    struct _M0TPB5EntryGsfE** _M0L7entriesS3159 = _M0L4selfS964->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS967;
    struct _M0TPB5EntryGsfE** _M0L7entriesS3155;
    struct _M0TPB5EntryGsfE* _M0L6_2atmpS3156;
    struct _M0TPB5EntryGsfE* _M0L6_2aoldS4131;
    int32_t _tmp_4691;
    if (
      _M0L4nextS963 < 0
      || _M0L4nextS963 >= Moonbit_array_length(_M0L7entriesS3159)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS967
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3159[_M0L4nextS963];
    if (_M0L7_2abindS967 == 0) {
      goto join_965;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS968 = _M0L7_2abindS967;
      struct _M0TPB5EntryGsfE* _M0L4_2axS969 = _M0L7_2aSomeS968;
      int32_t _M0L4_2axS970 = _M0L4_2axS969->$2;
      switch (_M0L4_2axS970) {
        case 0: {
          goto join_965;
          break;
        }
        default: {
          int32_t _M0L3pslS3158 = _M0L4_2axS969->$2;
          int32_t _M0L6_2atmpS3157 = _M0L3pslS3158 - 1;
          _M0L4_2axS969->$2 = _M0L6_2atmpS3157;
          moonbit_incref(_M0L4_2axS969);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsfE(_M0L4selfS964, _M0L4_2axS969, _M0L3curS962);
          moonbit_decref(_M0L4_2axS969);
          _M0L3curS962 = _M0L4nextS963;
          goto _2afor_966;
          break;
        }
      }
    }
    goto joinlet_4690;
    join_965:;
    _M0L7entriesS3155 = _M0L4selfS964->$0;
    _M0L6_2atmpS3156 = 0;
    if (
      _M0L3curS962 < 0
      || _M0L3curS962 >= Moonbit_array_length(_M0L7entriesS3155)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4131
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3155[_M0L3curS962];
    if (_M0L6_2aoldS4131) {
      moonbit_decref(_M0L6_2aoldS4131);
    }
    _M0L7entriesS3155[_M0L3curS962] = _M0L6_2atmpS3156;
    break;
    joinlet_4690:;
    _tmp_4691 = _M0L3curS962;
    _M0L3curS962 = _tmp_4691;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS974,
  int32_t _M0L3idxS981
) {
  int32_t _M0L3curS972;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS972 = _M0L3idxS981;
  _2afor_976:;
  while (1) {
    int32_t _M0L6_2atmpS3167 = _M0L3curS972 + 1;
    int32_t _M0L14capacity__maskS3168 = _M0L4selfS974->$3;
    int32_t _M0L4nextS973 = _M0L6_2atmpS3167 & _M0L14capacity__maskS3168;
    struct _M0TPB5EntryGsbE** _M0L7entriesS3166 = _M0L4selfS974->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS977;
    struct _M0TPB5EntryGsbE** _M0L7entriesS3162;
    struct _M0TPB5EntryGsbE* _M0L6_2atmpS3163;
    struct _M0TPB5EntryGsbE* _M0L6_2aoldS4135;
    int32_t _tmp_4694;
    if (
      _M0L4nextS973 < 0
      || _M0L4nextS973 >= Moonbit_array_length(_M0L7entriesS3166)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS977
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3166[_M0L4nextS973];
    if (_M0L7_2abindS977 == 0) {
      goto join_975;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS978 = _M0L7_2abindS977;
      struct _M0TPB5EntryGsbE* _M0L4_2axS979 = _M0L7_2aSomeS978;
      int32_t _M0L4_2axS980 = _M0L4_2axS979->$2;
      switch (_M0L4_2axS980) {
        case 0: {
          goto join_975;
          break;
        }
        default: {
          int32_t _M0L3pslS3165 = _M0L4_2axS979->$2;
          int32_t _M0L6_2atmpS3164 = _M0L3pslS3165 - 1;
          _M0L4_2axS979->$2 = _M0L6_2atmpS3164;
          moonbit_incref(_M0L4_2axS979);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsbE(_M0L4selfS974, _M0L4_2axS979, _M0L3curS972);
          moonbit_decref(_M0L4_2axS979);
          _M0L3curS972 = _M0L4nextS973;
          goto _2afor_976;
          break;
        }
      }
    }
    goto joinlet_4693;
    join_975:;
    _M0L7entriesS3162 = _M0L4selfS974->$0;
    _M0L6_2atmpS3163 = 0;
    if (
      _M0L3curS972 < 0
      || _M0L3curS972 >= Moonbit_array_length(_M0L7entriesS3162)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4135
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3162[_M0L3curS972];
    if (_M0L6_2aoldS4135) {
      moonbit_decref(_M0L6_2aoldS4135);
    }
    _M0L7entriesS3162[_M0L3curS972] = _M0L6_2atmpS3163;
    break;
    joinlet_4693:;
    _tmp_4694 = _M0L3curS972;
    _M0L3curS972 = _tmp_4694;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS920,
  struct _M0TPB5EntryGsiE* _M0L5entryS919
) {
  int32_t _M0L7_2abindS918;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS921;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS918 = _M0L5entryS919->$0;
  switch (_M0L7_2abindS918) {
    case -1: {
      struct _M0TPB5EntryGsiE* _M0L4nextS3113 = _M0L5entryS919->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4140 = _M0L4selfS920->$5;
      if (_M0L4nextS3113) {
        moonbit_incref(_M0L4nextS3113);
      }
      if (_M0L6_2aoldS4140) {
        moonbit_decref(_M0L6_2aoldS4140);
      }
      _M0L4selfS920->$5 = _M0L4nextS3113;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS3117 = _M0L4selfS920->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS3116;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS3114;
      struct _M0TPB5EntryGsiE* _M0L4nextS3115;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4142;
      if (
        _M0L7_2abindS918 < 0
        || _M0L7_2abindS918 >= Moonbit_array_length(_M0L7entriesS3117)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3116
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3117[_M0L7_2abindS918];
      if (_M0L6_2atmpS3116) {
        moonbit_incref(_M0L6_2atmpS3116);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3114
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS3116);
      if (_M0L6_2atmpS3116) {
        moonbit_decref(_M0L6_2atmpS3116);
      }
      _M0L4nextS3115 = _M0L5entryS919->$1;
      _M0L6_2aoldS4142 = _M0L6_2atmpS3114->$1;
      if (_M0L4nextS3115) {
        moonbit_incref(_M0L4nextS3115);
      }
      if (_M0L6_2aoldS4142) {
        moonbit_decref(_M0L6_2aoldS4142);
      }
      _M0L6_2atmpS3114->$1 = _M0L4nextS3115;
      moonbit_decref(_M0L6_2atmpS3114);
      break;
    }
  }
  _M0L7_2abindS921 = _M0L5entryS919->$1;
  if (_M0L7_2abindS921 == 0) {
    int32_t _M0L4prevS3118 = _M0L5entryS919->$0;
    _M0L4selfS920->$6 = _M0L4prevS3118;
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS922 = _M0L7_2abindS921;
    struct _M0TPB5EntryGsiE* _M0L7_2anextS923 = _M0L7_2aSomeS922;
    int32_t _M0L4prevS3119 = _M0L5entryS919->$0;
    _M0L7_2anextS923->$0 = _M0L4prevS3119;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS926,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS925
) {
  int32_t _M0L7_2abindS924;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS927;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS924 = _M0L5entryS925->$0;
  switch (_M0L7_2abindS924) {
    case -1: {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4nextS3120 =
        _M0L5entryS925->$1;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4147 =
        _M0L4selfS926->$5;
      if (_M0L4nextS3120) {
        moonbit_incref(_M0L4nextS3120);
      }
      if (_M0L6_2aoldS4147) {
        moonbit_decref(_M0L6_2aoldS4147);
      }
      _M0L4selfS926->$5 = _M0L4nextS3120;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3124 =
        _M0L4selfS926->$0;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3123;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3121;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4nextS3122;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4149;
      if (
        _M0L7_2abindS924 < 0
        || _M0L7_2abindS924 >= Moonbit_array_length(_M0L7entriesS3124)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3123
      = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3124[
          _M0L7_2abindS924
        ];
      if (_M0L6_2atmpS3123) {
        moonbit_incref(_M0L6_2atmpS3123);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3121
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP19moonbitDB10RedisValueEE(_M0L6_2atmpS3123);
      if (_M0L6_2atmpS3123) {
        moonbit_decref(_M0L6_2atmpS3123);
      }
      _M0L4nextS3122 = _M0L5entryS925->$1;
      _M0L6_2aoldS4149 = _M0L6_2atmpS3121->$1;
      if (_M0L4nextS3122) {
        moonbit_incref(_M0L4nextS3122);
      }
      if (_M0L6_2aoldS4149) {
        moonbit_decref(_M0L6_2aoldS4149);
      }
      _M0L6_2atmpS3121->$1 = _M0L4nextS3122;
      moonbit_decref(_M0L6_2atmpS3121);
      break;
    }
  }
  _M0L7_2abindS927 = _M0L5entryS925->$1;
  if (_M0L7_2abindS927 == 0) {
    int32_t _M0L4prevS3125 = _M0L5entryS925->$0;
    _M0L4selfS926->$6 = _M0L4prevS3125;
  } else {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS928 =
      _M0L7_2abindS927;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2anextS929 =
      _M0L7_2aSomeS928;
    int32_t _M0L4prevS3126 = _M0L5entryS925->$0;
    _M0L7_2anextS929->$0 = _M0L4prevS3126;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS932,
  struct _M0TPB5EntryGsfE* _M0L5entryS931
) {
  int32_t _M0L7_2abindS930;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS933;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS930 = _M0L5entryS931->$0;
  switch (_M0L7_2abindS930) {
    case -1: {
      struct _M0TPB5EntryGsfE* _M0L4nextS3127 = _M0L5entryS931->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4154 = _M0L4selfS932->$5;
      if (_M0L4nextS3127) {
        moonbit_incref(_M0L4nextS3127);
      }
      if (_M0L6_2aoldS4154) {
        moonbit_decref(_M0L6_2aoldS4154);
      }
      _M0L4selfS932->$5 = _M0L4nextS3127;
      break;
    }
    default: {
      struct _M0TPB5EntryGsfE** _M0L7entriesS3131 = _M0L4selfS932->$0;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS3130;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS3128;
      struct _M0TPB5EntryGsfE* _M0L4nextS3129;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4156;
      if (
        _M0L7_2abindS930 < 0
        || _M0L7_2abindS930 >= Moonbit_array_length(_M0L7entriesS3131)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3130
      = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3131[_M0L7_2abindS930];
      if (_M0L6_2atmpS3130) {
        moonbit_incref(_M0L6_2atmpS3130);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3128
      = _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(_M0L6_2atmpS3130);
      if (_M0L6_2atmpS3130) {
        moonbit_decref(_M0L6_2atmpS3130);
      }
      _M0L4nextS3129 = _M0L5entryS931->$1;
      _M0L6_2aoldS4156 = _M0L6_2atmpS3128->$1;
      if (_M0L4nextS3129) {
        moonbit_incref(_M0L4nextS3129);
      }
      if (_M0L6_2aoldS4156) {
        moonbit_decref(_M0L6_2aoldS4156);
      }
      _M0L6_2atmpS3128->$1 = _M0L4nextS3129;
      moonbit_decref(_M0L6_2atmpS3128);
      break;
    }
  }
  _M0L7_2abindS933 = _M0L5entryS931->$1;
  if (_M0L7_2abindS933 == 0) {
    int32_t _M0L4prevS3132 = _M0L5entryS931->$0;
    _M0L4selfS932->$6 = _M0L4prevS3132;
  } else {
    struct _M0TPB5EntryGsfE* _M0L7_2aSomeS934 = _M0L7_2abindS933;
    struct _M0TPB5EntryGsfE* _M0L7_2anextS935 = _M0L7_2aSomeS934;
    int32_t _M0L4prevS3133 = _M0L5entryS931->$0;
    _M0L7_2anextS935->$0 = _M0L4prevS3133;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS938,
  struct _M0TPB5EntryGsbE* _M0L5entryS937
) {
  int32_t _M0L7_2abindS936;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS939;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS936 = _M0L5entryS937->$0;
  switch (_M0L7_2abindS936) {
    case -1: {
      struct _M0TPB5EntryGsbE* _M0L4nextS3134 = _M0L5entryS937->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4161 = _M0L4selfS938->$5;
      if (_M0L4nextS3134) {
        moonbit_incref(_M0L4nextS3134);
      }
      if (_M0L6_2aoldS4161) {
        moonbit_decref(_M0L6_2aoldS4161);
      }
      _M0L4selfS938->$5 = _M0L4nextS3134;
      break;
    }
    default: {
      struct _M0TPB5EntryGsbE** _M0L7entriesS3138 = _M0L4selfS938->$0;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS3137;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS3135;
      struct _M0TPB5EntryGsbE* _M0L4nextS3136;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4163;
      if (
        _M0L7_2abindS936 < 0
        || _M0L7_2abindS936 >= Moonbit_array_length(_M0L7entriesS3138)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3137
      = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3138[_M0L7_2abindS936];
      if (_M0L6_2atmpS3137) {
        moonbit_incref(_M0L6_2atmpS3137);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3135
      = _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(_M0L6_2atmpS3137);
      if (_M0L6_2atmpS3137) {
        moonbit_decref(_M0L6_2atmpS3137);
      }
      _M0L4nextS3136 = _M0L5entryS937->$1;
      _M0L6_2aoldS4163 = _M0L6_2atmpS3135->$1;
      if (_M0L4nextS3136) {
        moonbit_incref(_M0L4nextS3136);
      }
      if (_M0L6_2aoldS4163) {
        moonbit_decref(_M0L6_2aoldS4163);
      }
      _M0L6_2atmpS3135->$1 = _M0L4nextS3136;
      moonbit_decref(_M0L6_2atmpS3135);
      break;
    }
  }
  _M0L7_2abindS939 = _M0L5entryS937->$1;
  if (_M0L7_2abindS939 == 0) {
    int32_t _M0L4prevS3139 = _M0L5entryS937->$0;
    _M0L4selfS938->$6 = _M0L4prevS3139;
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS940 = _M0L7_2abindS939;
    struct _M0TPB5EntryGsbE* _M0L7_2anextS941 = _M0L7_2aSomeS940;
    int32_t _M0L4prevS3140 = _M0L5entryS937->$0;
    _M0L7_2anextS941->$0 = _M0L4prevS3140;
  }
  return 0;
}

int32_t _M0MPB3Map8containsGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS887,
  moonbit_string_t _M0L3keyS883
) {
  int32_t _M0L4hashS882;
  int32_t _M0L14capacity__maskS3082;
  int32_t _M0L6_2atmpS3081;
  int32_t _M0L1iS884;
  int32_t _M0L3idxS885;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS882 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS883);
  _M0L14capacity__maskS3082 = _M0L4selfS887->$3;
  _M0L6_2atmpS3081 = _M0L4hashS882 & _M0L14capacity__maskS3082;
  _M0L1iS884 = 0;
  _M0L3idxS885 = _M0L6_2atmpS3081;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS3080 = _M0L4selfS887->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS886;
    if (
      _M0L3idxS885 < 0
      || _M0L3idxS885 >= Moonbit_array_length(_M0L7entriesS3080)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS886
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3080[_M0L3idxS885];
    if (_M0L7_2abindS886 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS888 = _M0L7_2abindS886;
      struct _M0TPB5EntryGsbE* _M0L8_2aentryS889 = _M0L7_2aSomeS888;
      int32_t _M0L4hashS3074 = _M0L8_2aentryS889->$3;
      int32_t _if__result_4696;
      int32_t _M0L3pslS3075;
      int32_t _M0L6_2atmpS3076;
      int32_t _M0L6_2atmpS3078;
      int32_t _M0L14capacity__maskS3079;
      int32_t _M0L6_2atmpS3077;
      if (_M0L4hashS3074 == _M0L4hashS882) {
        moonbit_string_t _M0L3keyS3073 = _M0L8_2aentryS889->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4696
        = _M0L3keyS3073 == _M0L3keyS883
          || Moonbit_array_length(_M0L3keyS3073)
             == Moonbit_array_length(_M0L3keyS883)
             && 0
                == memcmp(_M0L3keyS3073, _M0L3keyS883, Moonbit_array_length(_M0L3keyS3073) * 2);
      } else {
        _if__result_4696 = 0;
      }
      if (_if__result_4696) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS889);
      }
      _M0L3pslS3075 = _M0L8_2aentryS889->$2;
      moonbit_decref(_M0L8_2aentryS889);
      if (_M0L1iS884 > _M0L3pslS3075) {
        return 0;
      }
      _M0L6_2atmpS3076 = _M0L1iS884 + 1;
      _M0L6_2atmpS3078 = _M0L3idxS885 + 1;
      _M0L14capacity__maskS3079 = _M0L4selfS887->$3;
      _M0L6_2atmpS3077 = _M0L6_2atmpS3078 & _M0L14capacity__maskS3079;
      _M0L1iS884 = _M0L6_2atmpS3076;
      _M0L3idxS885 = _M0L6_2atmpS3077;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS896,
  moonbit_string_t _M0L3keyS892
) {
  int32_t _M0L4hashS891;
  int32_t _M0L14capacity__maskS3092;
  int32_t _M0L6_2atmpS3091;
  int32_t _M0L1iS893;
  int32_t _M0L3idxS894;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS891 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS892);
  _M0L14capacity__maskS3092 = _M0L4selfS896->$3;
  _M0L6_2atmpS3091 = _M0L4hashS891 & _M0L14capacity__maskS3092;
  _M0L1iS893 = 0;
  _M0L3idxS894 = _M0L6_2atmpS3091;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS3090 = _M0L4selfS896->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS895;
    if (
      _M0L3idxS894 < 0
      || _M0L3idxS894 >= Moonbit_array_length(_M0L7entriesS3090)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS895
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3090[_M0L3idxS894];
    if (_M0L7_2abindS895 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS897 = _M0L7_2abindS895;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS898 = _M0L7_2aSomeS897;
      int32_t _M0L4hashS3084 = _M0L8_2aentryS898->$3;
      int32_t _if__result_4698;
      int32_t _M0L3pslS3085;
      int32_t _M0L6_2atmpS3086;
      int32_t _M0L6_2atmpS3088;
      int32_t _M0L14capacity__maskS3089;
      int32_t _M0L6_2atmpS3087;
      if (_M0L4hashS3084 == _M0L4hashS891) {
        moonbit_string_t _M0L3keyS3083 = _M0L8_2aentryS898->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4698
        = _M0L3keyS3083 == _M0L3keyS892
          || Moonbit_array_length(_M0L3keyS3083)
             == Moonbit_array_length(_M0L3keyS892)
             && 0
                == memcmp(_M0L3keyS3083, _M0L3keyS892, Moonbit_array_length(_M0L3keyS3083) * 2);
      } else {
        _if__result_4698 = 0;
      }
      if (_if__result_4698) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS898);
      }
      _M0L3pslS3085 = _M0L8_2aentryS898->$2;
      moonbit_decref(_M0L8_2aentryS898);
      if (_M0L1iS893 > _M0L3pslS3085) {
        return 0;
      }
      _M0L6_2atmpS3086 = _M0L1iS893 + 1;
      _M0L6_2atmpS3088 = _M0L3idxS894 + 1;
      _M0L14capacity__maskS3089 = _M0L4selfS896->$3;
      _M0L6_2atmpS3087 = _M0L6_2atmpS3088 & _M0L14capacity__maskS3089;
      _M0L1iS893 = _M0L6_2atmpS3086;
      _M0L3idxS894 = _M0L6_2atmpS3087;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS905,
  moonbit_string_t _M0L3keyS901
) {
  int32_t _M0L4hashS900;
  int32_t _M0L14capacity__maskS3102;
  int32_t _M0L6_2atmpS3101;
  int32_t _M0L1iS902;
  int32_t _M0L3idxS903;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS900 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS901);
  _M0L14capacity__maskS3102 = _M0L4selfS905->$3;
  _M0L6_2atmpS3101 = _M0L4hashS900 & _M0L14capacity__maskS3102;
  _M0L1iS902 = 0;
  _M0L3idxS903 = _M0L6_2atmpS3101;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3100 =
      _M0L4selfS905->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS904;
    if (
      _M0L3idxS903 < 0
      || _M0L3idxS903 >= Moonbit_array_length(_M0L7entriesS3100)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS904
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3100[
        _M0L3idxS903
      ];
    if (_M0L7_2abindS904 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS906 =
        _M0L7_2abindS904;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L8_2aentryS907 =
        _M0L7_2aSomeS906;
      int32_t _M0L4hashS3094 = _M0L8_2aentryS907->$3;
      int32_t _if__result_4700;
      int32_t _M0L3pslS3095;
      int32_t _M0L6_2atmpS3096;
      int32_t _M0L6_2atmpS3098;
      int32_t _M0L14capacity__maskS3099;
      int32_t _M0L6_2atmpS3097;
      if (_M0L4hashS3094 == _M0L4hashS900) {
        moonbit_string_t _M0L3keyS3093 = _M0L8_2aentryS907->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4700
        = _M0L3keyS3093 == _M0L3keyS901
          || Moonbit_array_length(_M0L3keyS3093)
             == Moonbit_array_length(_M0L3keyS901)
             && 0
                == memcmp(_M0L3keyS3093, _M0L3keyS901, Moonbit_array_length(_M0L3keyS3093) * 2);
      } else {
        _if__result_4700 = 0;
      }
      if (_if__result_4700) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS907);
      }
      _M0L3pslS3095 = _M0L8_2aentryS907->$2;
      moonbit_decref(_M0L8_2aentryS907);
      if (_M0L1iS902 > _M0L3pslS3095) {
        return 0;
      }
      _M0L6_2atmpS3096 = _M0L1iS902 + 1;
      _M0L6_2atmpS3098 = _M0L3idxS903 + 1;
      _M0L14capacity__maskS3099 = _M0L4selfS905->$3;
      _M0L6_2atmpS3097 = _M0L6_2atmpS3098 & _M0L14capacity__maskS3099;
      _M0L1iS902 = _M0L6_2atmpS3096;
      _M0L3idxS903 = _M0L6_2atmpS3097;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS914,
  moonbit_string_t _M0L3keyS910
) {
  int32_t _M0L4hashS909;
  int32_t _M0L14capacity__maskS3112;
  int32_t _M0L6_2atmpS3111;
  int32_t _M0L1iS911;
  int32_t _M0L3idxS912;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS909 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS910);
  _M0L14capacity__maskS3112 = _M0L4selfS914->$3;
  _M0L6_2atmpS3111 = _M0L4hashS909 & _M0L14capacity__maskS3112;
  _M0L1iS911 = 0;
  _M0L3idxS912 = _M0L6_2atmpS3111;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3110 = _M0L4selfS914->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS913;
    if (
      _M0L3idxS912 < 0
      || _M0L3idxS912 >= Moonbit_array_length(_M0L7entriesS3110)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS913
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3110[_M0L3idxS912];
    if (_M0L7_2abindS913 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS915 = _M0L7_2abindS913;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS916 = _M0L7_2aSomeS915;
      int32_t _M0L4hashS3104 = _M0L8_2aentryS916->$3;
      int32_t _if__result_4702;
      int32_t _M0L3pslS3105;
      int32_t _M0L6_2atmpS3106;
      int32_t _M0L6_2atmpS3108;
      int32_t _M0L14capacity__maskS3109;
      int32_t _M0L6_2atmpS3107;
      if (_M0L4hashS3104 == _M0L4hashS909) {
        moonbit_string_t _M0L3keyS3103 = _M0L8_2aentryS916->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4702
        = _M0L3keyS3103 == _M0L3keyS910
          || Moonbit_array_length(_M0L3keyS3103)
             == Moonbit_array_length(_M0L3keyS910)
             && 0
                == memcmp(_M0L3keyS3103, _M0L3keyS910, Moonbit_array_length(_M0L3keyS3103) * 2);
      } else {
        _if__result_4702 = 0;
      }
      if (_if__result_4702) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS916);
      }
      _M0L3pslS3105 = _M0L8_2aentryS916->$2;
      moonbit_decref(_M0L8_2aentryS916);
      if (_M0L1iS911 > _M0L3pslS3105) {
        return 0;
      }
      _M0L6_2atmpS3106 = _M0L1iS911 + 1;
      _M0L6_2atmpS3108 = _M0L3idxS912 + 1;
      _M0L14capacity__maskS3109 = _M0L4selfS914->$3;
      _M0L6_2atmpS3107 = _M0L6_2atmpS3108 & _M0L14capacity__maskS3109;
      _M0L1iS911 = _M0L6_2atmpS3106;
      _M0L3idxS912 = _M0L6_2atmpS3107;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS851,
  moonbit_string_t _M0L3keyS847
) {
  int32_t _M0L4hashS846;
  int32_t _M0L14capacity__maskS3032;
  int32_t _M0L6_2atmpS3031;
  int32_t _M0L1iS848;
  int32_t _M0L3idxS849;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS846 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS847);
  _M0L14capacity__maskS3032 = _M0L4selfS851->$3;
  _M0L6_2atmpS3031 = _M0L4hashS846 & _M0L14capacity__maskS3032;
  _M0L1iS848 = 0;
  _M0L3idxS849 = _M0L6_2atmpS3031;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3030 =
      _M0L4selfS851->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS850;
    if (
      _M0L3idxS849 < 0
      || _M0L3idxS849 >= Moonbit_array_length(_M0L7entriesS3030)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS850
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3030[
        _M0L3idxS849
      ];
    if (_M0L7_2abindS850 == 0) {
      void* _M0L6_2atmpS3019 = 0;
      return _M0L6_2atmpS3019;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS852 =
        _M0L7_2abindS850;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L8_2aentryS853 =
        _M0L7_2aSomeS852;
      int32_t _M0L4hashS3021 = _M0L8_2aentryS853->$3;
      int32_t _if__result_4704;
      int32_t _M0L3pslS3024;
      int32_t _M0L6_2atmpS3026;
      int32_t _M0L6_2atmpS3028;
      int32_t _M0L14capacity__maskS3029;
      int32_t _M0L6_2atmpS3027;
      if (_M0L4hashS3021 == _M0L4hashS846) {
        moonbit_string_t _M0L3keyS3020 = _M0L8_2aentryS853->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4704
        = _M0L3keyS3020 == _M0L3keyS847
          || Moonbit_array_length(_M0L3keyS3020)
             == Moonbit_array_length(_M0L3keyS847)
             && 0
                == memcmp(_M0L3keyS3020, _M0L3keyS847, Moonbit_array_length(_M0L3keyS3020) * 2);
      } else {
        _if__result_4704 = 0;
      }
      if (_if__result_4704) {
        void* _M0L5valueS3023 = _M0L8_2aentryS853->$5;
        void* _M0L6_2atmpS3022;
        moonbit_incref(_M0L5valueS3023);
        _M0L6_2atmpS3022 = _M0L5valueS3023;
        return _M0L6_2atmpS3022;
      } else {
        moonbit_incref(_M0L8_2aentryS853);
      }
      _M0L3pslS3024 = _M0L8_2aentryS853->$2;
      moonbit_decref(_M0L8_2aentryS853);
      if (_M0L1iS848 > _M0L3pslS3024) {
        void* _M0L6_2atmpS3025 = 0;
        return _M0L6_2atmpS3025;
      }
      _M0L6_2atmpS3026 = _M0L1iS848 + 1;
      _M0L6_2atmpS3028 = _M0L3idxS849 + 1;
      _M0L14capacity__maskS3029 = _M0L4selfS851->$3;
      _M0L6_2atmpS3027 = _M0L6_2atmpS3028 & _M0L14capacity__maskS3029;
      _M0L1iS848 = _M0L6_2atmpS3026;
      _M0L3idxS849 = _M0L6_2atmpS3027;
      continue;
    }
    break;
  }
}

moonbit_string_t _M0MPB3Map3getGssE(
  struct _M0TPB3MapGssE* _M0L4selfS860,
  moonbit_string_t _M0L3keyS856
) {
  int32_t _M0L4hashS855;
  int32_t _M0L14capacity__maskS3046;
  int32_t _M0L6_2atmpS3045;
  int32_t _M0L1iS857;
  int32_t _M0L3idxS858;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS855 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS856);
  _M0L14capacity__maskS3046 = _M0L4selfS860->$3;
  _M0L6_2atmpS3045 = _M0L4hashS855 & _M0L14capacity__maskS3046;
  _M0L1iS857 = 0;
  _M0L3idxS858 = _M0L6_2atmpS3045;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS3044 = _M0L4selfS860->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS859;
    if (
      _M0L3idxS858 < 0
      || _M0L3idxS858 >= Moonbit_array_length(_M0L7entriesS3044)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS859
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS3044[_M0L3idxS858];
    if (_M0L7_2abindS859 == 0) {
      moonbit_string_t _M0L6_2atmpS3033 = 0;
      return _M0L6_2atmpS3033;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS861 = _M0L7_2abindS859;
      struct _M0TPB5EntryGssE* _M0L8_2aentryS862 = _M0L7_2aSomeS861;
      int32_t _M0L4hashS3035 = _M0L8_2aentryS862->$3;
      int32_t _if__result_4706;
      int32_t _M0L3pslS3038;
      int32_t _M0L6_2atmpS3040;
      int32_t _M0L6_2atmpS3042;
      int32_t _M0L14capacity__maskS3043;
      int32_t _M0L6_2atmpS3041;
      if (_M0L4hashS3035 == _M0L4hashS855) {
        moonbit_string_t _M0L3keyS3034 = _M0L8_2aentryS862->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4706
        = _M0L3keyS3034 == _M0L3keyS856
          || Moonbit_array_length(_M0L3keyS3034)
             == Moonbit_array_length(_M0L3keyS856)
             && 0
                == memcmp(_M0L3keyS3034, _M0L3keyS856, Moonbit_array_length(_M0L3keyS3034) * 2);
      } else {
        _if__result_4706 = 0;
      }
      if (_if__result_4706) {
        moonbit_string_t _M0L5valueS3037 = _M0L8_2aentryS862->$5;
        moonbit_string_t _M0L6_2atmpS3036;
        moonbit_incref(_M0L5valueS3037);
        _M0L6_2atmpS3036 = _M0L5valueS3037;
        return _M0L6_2atmpS3036;
      } else {
        moonbit_incref(_M0L8_2aentryS862);
      }
      _M0L3pslS3038 = _M0L8_2aentryS862->$2;
      moonbit_decref(_M0L8_2aentryS862);
      if (_M0L1iS857 > _M0L3pslS3038) {
        moonbit_string_t _M0L6_2atmpS3039 = 0;
        return _M0L6_2atmpS3039;
      }
      _M0L6_2atmpS3040 = _M0L1iS857 + 1;
      _M0L6_2atmpS3042 = _M0L3idxS858 + 1;
      _M0L14capacity__maskS3043 = _M0L4selfS860->$3;
      _M0L6_2atmpS3041 = _M0L6_2atmpS3042 & _M0L14capacity__maskS3043;
      _M0L1iS857 = _M0L6_2atmpS3040;
      _M0L3idxS858 = _M0L6_2atmpS3041;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS869,
  moonbit_string_t _M0L3keyS865
) {
  int32_t _M0L4hashS864;
  int32_t _M0L14capacity__maskS3060;
  int32_t _M0L6_2atmpS3059;
  int32_t _M0L1iS866;
  int32_t _M0L3idxS867;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS864 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS865);
  _M0L14capacity__maskS3060 = _M0L4selfS869->$3;
  _M0L6_2atmpS3059 = _M0L4hashS864 & _M0L14capacity__maskS3060;
  _M0L1iS866 = 0;
  _M0L3idxS867 = _M0L6_2atmpS3059;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS3058 = _M0L4selfS869->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS868;
    if (
      _M0L3idxS867 < 0
      || _M0L3idxS867 >= Moonbit_array_length(_M0L7entriesS3058)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS868
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3058[_M0L3idxS867];
    if (_M0L7_2abindS868 == 0) {
      void* _M0L4NoneS3047 =
        (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      return _M0L4NoneS3047;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS870 = _M0L7_2abindS868;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS871 = _M0L7_2aSomeS870;
      int32_t _M0L4hashS3049 = _M0L8_2aentryS871->$3;
      int32_t _if__result_4708;
      int32_t _M0L3pslS3052;
      int32_t _M0L6_2atmpS3054;
      int32_t _M0L6_2atmpS3056;
      int32_t _M0L14capacity__maskS3057;
      int32_t _M0L6_2atmpS3055;
      if (_M0L4hashS3049 == _M0L4hashS864) {
        moonbit_string_t _M0L3keyS3048 = _M0L8_2aentryS871->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4708
        = _M0L3keyS3048 == _M0L3keyS865
          || Moonbit_array_length(_M0L3keyS3048)
             == Moonbit_array_length(_M0L3keyS865)
             && 0
                == memcmp(_M0L3keyS3048, _M0L3keyS865, Moonbit_array_length(_M0L3keyS3048) * 2);
      } else {
        _if__result_4708 = 0;
      }
      if (_if__result_4708) {
        float _M0L5valueS3051 = _M0L8_2aentryS871->$5;
        void* _M0L4SomeS3050 =
          (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGfE4Some));
        Moonbit_object_header(_M0L4SomeS3050)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 1);
        ((struct _M0DTPC16option6OptionGfE4Some*)_M0L4SomeS3050)->$0
        = _M0L5valueS3051;
        return _M0L4SomeS3050;
      } else {
        moonbit_incref(_M0L8_2aentryS871);
      }
      _M0L3pslS3052 = _M0L8_2aentryS871->$2;
      moonbit_decref(_M0L8_2aentryS871);
      if (_M0L1iS866 > _M0L3pslS3052) {
        void* _M0L4NoneS3053 =
          (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        return _M0L4NoneS3053;
      }
      _M0L6_2atmpS3054 = _M0L1iS866 + 1;
      _M0L6_2atmpS3056 = _M0L3idxS867 + 1;
      _M0L14capacity__maskS3057 = _M0L4selfS869->$3;
      _M0L6_2atmpS3055 = _M0L6_2atmpS3056 & _M0L14capacity__maskS3057;
      _M0L1iS866 = _M0L6_2atmpS3054;
      _M0L3idxS867 = _M0L6_2atmpS3055;
      continue;
    }
    break;
  }
}

int64_t _M0MPB3Map3getGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS878,
  moonbit_string_t _M0L3keyS874
) {
  int32_t _M0L4hashS873;
  int32_t _M0L14capacity__maskS3072;
  int32_t _M0L6_2atmpS3071;
  int32_t _M0L1iS875;
  int32_t _M0L3idxS876;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS873 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS874);
  _M0L14capacity__maskS3072 = _M0L4selfS878->$3;
  _M0L6_2atmpS3071 = _M0L4hashS873 & _M0L14capacity__maskS3072;
  _M0L1iS875 = 0;
  _M0L3idxS876 = _M0L6_2atmpS3071;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3070 = _M0L4selfS878->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS877;
    if (
      _M0L3idxS876 < 0
      || _M0L3idxS876 >= Moonbit_array_length(_M0L7entriesS3070)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS877
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3070[_M0L3idxS876];
    if (_M0L7_2abindS877 == 0) {
      return 4294967296ll;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS879 = _M0L7_2abindS877;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS880 = _M0L7_2aSomeS879;
      int32_t _M0L4hashS3062 = _M0L8_2aentryS880->$3;
      int32_t _if__result_4710;
      int32_t _M0L3pslS3065;
      int32_t _M0L6_2atmpS3066;
      int32_t _M0L6_2atmpS3068;
      int32_t _M0L14capacity__maskS3069;
      int32_t _M0L6_2atmpS3067;
      if (_M0L4hashS3062 == _M0L4hashS873) {
        moonbit_string_t _M0L3keyS3061 = _M0L8_2aentryS880->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4710
        = _M0L3keyS3061 == _M0L3keyS874
          || Moonbit_array_length(_M0L3keyS3061)
             == Moonbit_array_length(_M0L3keyS874)
             && 0
                == memcmp(_M0L3keyS3061, _M0L3keyS874, Moonbit_array_length(_M0L3keyS3061) * 2);
      } else {
        _if__result_4710 = 0;
      }
      if (_if__result_4710) {
        int32_t _M0L5valueS3064 = _M0L8_2aentryS880->$5;
        int64_t _M0L6_2atmpS3063 = (int64_t)_M0L5valueS3064;
        return _M0L6_2atmpS3063;
      } else {
        moonbit_incref(_M0L8_2aentryS880);
      }
      _M0L3pslS3065 = _M0L8_2aentryS880->$2;
      moonbit_decref(_M0L8_2aentryS880);
      if (_M0L1iS875 > _M0L3pslS3065) {
        return 4294967296ll;
      }
      _M0L6_2atmpS3066 = _M0L1iS875 + 1;
      _M0L6_2atmpS3068 = _M0L3idxS876 + 1;
      _M0L14capacity__maskS3069 = _M0L4selfS878->$3;
      _M0L6_2atmpS3067 = _M0L6_2atmpS3068 & _M0L14capacity__maskS3069;
      _M0L1iS875 = _M0L6_2atmpS3066;
      _M0L3idxS876 = _M0L6_2atmpS3067;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0MPB3Map3MapGsRP19moonbitDB10RedisValueE(
  struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE _M0L3arrS792,
  int64_t _M0L8capacityS794
) {
  int32_t _M0L3endS2973;
  int32_t _M0L5startS2974;
  int32_t _M0L6lengthS791;
  int32_t _M0L8capacityS793;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L1mS797;
  int32_t _M0L3endS2970;
  int32_t _M0L5startS2971;
  int32_t _M0L7_2abindS798;
  int32_t _M0L2__S799;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2973 = _M0L3arrS792.$2;
  _M0L5startS2974 = _M0L3arrS792.$1;
  _M0L6lengthS791 = _M0L3endS2973 - _M0L5startS2974;
  if (_M0L8capacityS794 == 4294967296ll) {
    if (_M0L6lengthS791 == 0) {
      _M0L8capacityS793 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS793 = _M0FPB21capacity__for__length(_M0L6lengthS791);
    }
  } else {
    int64_t _M0L7_2aSomeS795 = _M0L8capacityS794;
    int32_t _M0L11_2acapacityS796 = (int32_t)_M0L7_2aSomeS795;
    int32_t _M0L6_2atmpS2972;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2972 = _M0FPB21capacity__for__length(_M0L6lengthS791);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS793
    = _M0MPC13int3Int3max(_M0L11_2acapacityS796, _M0L6_2atmpS2972);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS797 = _M0FPB8new__mapGsRP19moonbitDB10RedisValueE(_M0L8capacityS793);
  _M0L3endS2970 = _M0L3arrS792.$2;
  _M0L5startS2971 = _M0L3arrS792.$1;
  _M0L7_2abindS798 = _M0L3endS2970 - _M0L5startS2971;
  _M0L2__S799 = 0;
  while (1) {
    if (_M0L2__S799 < _M0L7_2abindS798) {
      struct _M0TUsRP19moonbitDB10RedisValueE** _M0L3bufS2967 =
        _M0L3arrS792.$0;
      int32_t _M0L5startS2969 = _M0L3arrS792.$1;
      int32_t _M0L6_2atmpS2968 = _M0L5startS2969 + _M0L2__S799;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L1eS800 =
        (struct _M0TUsRP19moonbitDB10RedisValueE*)_M0L3bufS2967[
          _M0L6_2atmpS2968
        ];
      moonbit_string_t _M0L6_2atmpS2964 = _M0L1eS800->$0;
      void* _M0L6_2atmpS2965 = _M0L1eS800->$1;
      int32_t _M0L6_2atmpS2966;
      moonbit_incref(_M0L6_2atmpS2965);
      moonbit_incref(_M0L6_2atmpS2964);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L1mS797, _M0L6_2atmpS2964, _M0L6_2atmpS2965);
      moonbit_decref(_M0L6_2atmpS2964);
      moonbit_decref(_M0L6_2atmpS2965);
      _M0L6_2atmpS2966 = _M0L2__S799 + 1;
      _M0L2__S799 = _M0L6_2atmpS2966;
      continue;
    }
    break;
  }
  return _M0L1mS797;
}

struct _M0TPB3MapGsiE* _M0MPB3Map3MapGsiE(
  struct _M0TPB9ArrayViewGUsiEE _M0L3arrS803,
  int64_t _M0L8capacityS805
) {
  int32_t _M0L3endS2984;
  int32_t _M0L5startS2985;
  int32_t _M0L6lengthS802;
  int32_t _M0L8capacityS804;
  struct _M0TPB3MapGsiE* _M0L1mS808;
  int32_t _M0L3endS2981;
  int32_t _M0L5startS2982;
  int32_t _M0L7_2abindS809;
  int32_t _M0L2__S810;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2984 = _M0L3arrS803.$2;
  _M0L5startS2985 = _M0L3arrS803.$1;
  _M0L6lengthS802 = _M0L3endS2984 - _M0L5startS2985;
  if (_M0L8capacityS805 == 4294967296ll) {
    if (_M0L6lengthS802 == 0) {
      _M0L8capacityS804 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS804 = _M0FPB21capacity__for__length(_M0L6lengthS802);
    }
  } else {
    int64_t _M0L7_2aSomeS806 = _M0L8capacityS805;
    int32_t _M0L11_2acapacityS807 = (int32_t)_M0L7_2aSomeS806;
    int32_t _M0L6_2atmpS2983;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2983 = _M0FPB21capacity__for__length(_M0L6lengthS802);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS804
    = _M0MPC13int3Int3max(_M0L11_2acapacityS807, _M0L6_2atmpS2983);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS808 = _M0FPB8new__mapGsiE(_M0L8capacityS804);
  _M0L3endS2981 = _M0L3arrS803.$2;
  _M0L5startS2982 = _M0L3arrS803.$1;
  _M0L7_2abindS809 = _M0L3endS2981 - _M0L5startS2982;
  _M0L2__S810 = 0;
  while (1) {
    if (_M0L2__S810 < _M0L7_2abindS809) {
      struct _M0TUsiE** _M0L3bufS2978 = _M0L3arrS803.$0;
      int32_t _M0L5startS2980 = _M0L3arrS803.$1;
      int32_t _M0L6_2atmpS2979 = _M0L5startS2980 + _M0L2__S810;
      struct _M0TUsiE* _M0L1eS811 =
        (struct _M0TUsiE*)_M0L3bufS2978[_M0L6_2atmpS2979];
      moonbit_string_t _M0L6_2atmpS2975 = _M0L1eS811->$0;
      int32_t _M0L6_2atmpS2976 = _M0L1eS811->$1;
      int32_t _M0L6_2atmpS2977;
      moonbit_incref(_M0L6_2atmpS2975);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsiE(_M0L1mS808, _M0L6_2atmpS2975, _M0L6_2atmpS2976);
      moonbit_decref(_M0L6_2atmpS2975);
      _M0L6_2atmpS2977 = _M0L2__S810 + 1;
      _M0L2__S810 = _M0L6_2atmpS2977;
      continue;
    }
    break;
  }
  return _M0L1mS808;
}

struct _M0TPB3MapGssE* _M0MPB3Map3MapGssE(
  struct _M0TPB9ArrayViewGUssEE _M0L3arrS814,
  int64_t _M0L8capacityS816
) {
  int32_t _M0L3endS2995;
  int32_t _M0L5startS2996;
  int32_t _M0L6lengthS813;
  int32_t _M0L8capacityS815;
  struct _M0TPB3MapGssE* _M0L1mS819;
  int32_t _M0L3endS2992;
  int32_t _M0L5startS2993;
  int32_t _M0L7_2abindS820;
  int32_t _M0L2__S821;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2995 = _M0L3arrS814.$2;
  _M0L5startS2996 = _M0L3arrS814.$1;
  _M0L6lengthS813 = _M0L3endS2995 - _M0L5startS2996;
  if (_M0L8capacityS816 == 4294967296ll) {
    if (_M0L6lengthS813 == 0) {
      _M0L8capacityS815 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS815 = _M0FPB21capacity__for__length(_M0L6lengthS813);
    }
  } else {
    int64_t _M0L7_2aSomeS817 = _M0L8capacityS816;
    int32_t _M0L11_2acapacityS818 = (int32_t)_M0L7_2aSomeS817;
    int32_t _M0L6_2atmpS2994;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2994 = _M0FPB21capacity__for__length(_M0L6lengthS813);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS815
    = _M0MPC13int3Int3max(_M0L11_2acapacityS818, _M0L6_2atmpS2994);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS819 = _M0FPB8new__mapGssE(_M0L8capacityS815);
  _M0L3endS2992 = _M0L3arrS814.$2;
  _M0L5startS2993 = _M0L3arrS814.$1;
  _M0L7_2abindS820 = _M0L3endS2992 - _M0L5startS2993;
  _M0L2__S821 = 0;
  while (1) {
    if (_M0L2__S821 < _M0L7_2abindS820) {
      struct _M0TUssE** _M0L3bufS2989 = _M0L3arrS814.$0;
      int32_t _M0L5startS2991 = _M0L3arrS814.$1;
      int32_t _M0L6_2atmpS2990 = _M0L5startS2991 + _M0L2__S821;
      struct _M0TUssE* _M0L1eS822 =
        (struct _M0TUssE*)_M0L3bufS2989[_M0L6_2atmpS2990];
      moonbit_string_t _M0L6_2atmpS2986 = _M0L1eS822->$0;
      moonbit_string_t _M0L6_2atmpS2987 = _M0L1eS822->$1;
      int32_t _M0L6_2atmpS2988;
      moonbit_incref(_M0L6_2atmpS2987);
      moonbit_incref(_M0L6_2atmpS2986);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGssE(_M0L1mS819, _M0L6_2atmpS2986, _M0L6_2atmpS2987);
      moonbit_decref(_M0L6_2atmpS2986);
      moonbit_decref(_M0L6_2atmpS2987);
      _M0L6_2atmpS2988 = _M0L2__S821 + 1;
      _M0L2__S821 = _M0L6_2atmpS2988;
      continue;
    }
    break;
  }
  return _M0L1mS819;
}

struct _M0TPB3MapGsbE* _M0MPB3Map3MapGsbE(
  struct _M0TPB9ArrayViewGUsbEE _M0L3arrS825,
  int64_t _M0L8capacityS827
) {
  int32_t _M0L3endS3006;
  int32_t _M0L5startS3007;
  int32_t _M0L6lengthS824;
  int32_t _M0L8capacityS826;
  struct _M0TPB3MapGsbE* _M0L1mS830;
  int32_t _M0L3endS3003;
  int32_t _M0L5startS3004;
  int32_t _M0L7_2abindS831;
  int32_t _M0L2__S832;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS3006 = _M0L3arrS825.$2;
  _M0L5startS3007 = _M0L3arrS825.$1;
  _M0L6lengthS824 = _M0L3endS3006 - _M0L5startS3007;
  if (_M0L8capacityS827 == 4294967296ll) {
    if (_M0L6lengthS824 == 0) {
      _M0L8capacityS826 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS826 = _M0FPB21capacity__for__length(_M0L6lengthS824);
    }
  } else {
    int64_t _M0L7_2aSomeS828 = _M0L8capacityS827;
    int32_t _M0L11_2acapacityS829 = (int32_t)_M0L7_2aSomeS828;
    int32_t _M0L6_2atmpS3005;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS3005 = _M0FPB21capacity__for__length(_M0L6lengthS824);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS826
    = _M0MPC13int3Int3max(_M0L11_2acapacityS829, _M0L6_2atmpS3005);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS830 = _M0FPB8new__mapGsbE(_M0L8capacityS826);
  _M0L3endS3003 = _M0L3arrS825.$2;
  _M0L5startS3004 = _M0L3arrS825.$1;
  _M0L7_2abindS831 = _M0L3endS3003 - _M0L5startS3004;
  _M0L2__S832 = 0;
  while (1) {
    if (_M0L2__S832 < _M0L7_2abindS831) {
      struct _M0TUsbE** _M0L3bufS3000 = _M0L3arrS825.$0;
      int32_t _M0L5startS3002 = _M0L3arrS825.$1;
      int32_t _M0L6_2atmpS3001 = _M0L5startS3002 + _M0L2__S832;
      struct _M0TUsbE* _M0L1eS833 =
        (struct _M0TUsbE*)_M0L3bufS3000[_M0L6_2atmpS3001];
      moonbit_string_t _M0L6_2atmpS2997 = _M0L1eS833->$0;
      int32_t _M0L6_2atmpS2998 = _M0L1eS833->$1;
      int32_t _M0L6_2atmpS2999;
      moonbit_incref(_M0L6_2atmpS2997);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsbE(_M0L1mS830, _M0L6_2atmpS2997, _M0L6_2atmpS2998);
      moonbit_decref(_M0L6_2atmpS2997);
      _M0L6_2atmpS2999 = _M0L2__S832 + 1;
      _M0L2__S832 = _M0L6_2atmpS2999;
      continue;
    }
    break;
  }
  return _M0L1mS830;
}

struct _M0TPB3MapGsfE* _M0MPB3Map3MapGsfE(
  struct _M0TPB9ArrayViewGUsfEE _M0L3arrS836,
  int64_t _M0L8capacityS838
) {
  int32_t _M0L3endS3017;
  int32_t _M0L5startS3018;
  int32_t _M0L6lengthS835;
  int32_t _M0L8capacityS837;
  struct _M0TPB3MapGsfE* _M0L1mS841;
  int32_t _M0L3endS3014;
  int32_t _M0L5startS3015;
  int32_t _M0L7_2abindS842;
  int32_t _M0L2__S843;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS3017 = _M0L3arrS836.$2;
  _M0L5startS3018 = _M0L3arrS836.$1;
  _M0L6lengthS835 = _M0L3endS3017 - _M0L5startS3018;
  if (_M0L8capacityS838 == 4294967296ll) {
    if (_M0L6lengthS835 == 0) {
      _M0L8capacityS837 = 8;
    } else {
      #line 95 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L8capacityS837 = _M0FPB21capacity__for__length(_M0L6lengthS835);
    }
  } else {
    int64_t _M0L7_2aSomeS839 = _M0L8capacityS838;
    int32_t _M0L11_2acapacityS840 = (int32_t)_M0L7_2aSomeS839;
    int32_t _M0L6_2atmpS3016;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS3016 = _M0FPB21capacity__for__length(_M0L6lengthS835);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS837
    = _M0MPC13int3Int3max(_M0L11_2acapacityS840, _M0L6_2atmpS3016);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS841 = _M0FPB8new__mapGsfE(_M0L8capacityS837);
  _M0L3endS3014 = _M0L3arrS836.$2;
  _M0L5startS3015 = _M0L3arrS836.$1;
  _M0L7_2abindS842 = _M0L3endS3014 - _M0L5startS3015;
  _M0L2__S843 = 0;
  while (1) {
    if (_M0L2__S843 < _M0L7_2abindS842) {
      struct _M0TUsfE** _M0L3bufS3011 = _M0L3arrS836.$0;
      int32_t _M0L5startS3013 = _M0L3arrS836.$1;
      int32_t _M0L6_2atmpS3012 = _M0L5startS3013 + _M0L2__S843;
      struct _M0TUsfE* _M0L1eS844 =
        (struct _M0TUsfE*)_M0L3bufS3011[_M0L6_2atmpS3012];
      moonbit_string_t _M0L6_2atmpS3008 = _M0L1eS844->$0;
      float _M0L6_2atmpS3009 = _M0L1eS844->$1;
      int32_t _M0L6_2atmpS3010;
      moonbit_incref(_M0L6_2atmpS3008);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsfE(_M0L1mS841, _M0L6_2atmpS3008, _M0L6_2atmpS3009);
      moonbit_decref(_M0L6_2atmpS3008);
      _M0L6_2atmpS3010 = _M0L2__S843 + 1;
      _M0L2__S843 = _M0L6_2atmpS3010;
      continue;
    }
    break;
  }
  return _M0L1mS841;
}

int32_t _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS776,
  moonbit_string_t _M0L3keyS777,
  void* _M0L5valueS778
) {
  int32_t _M0L6_2atmpS2959;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2959 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS777);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRP19moonbitDB10RedisValueE(_M0L4selfS776, _M0L3keyS777, _M0L5valueS778, _M0L6_2atmpS2959);
  return 0;
}

int32_t _M0MPB3Map3setGssE(
  struct _M0TPB3MapGssE* _M0L4selfS779,
  moonbit_string_t _M0L3keyS780,
  moonbit_string_t _M0L5valueS781
) {
  int32_t _M0L6_2atmpS2960;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2960 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS780);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGssE(_M0L4selfS779, _M0L3keyS780, _M0L5valueS781, _M0L6_2atmpS2960);
  return 0;
}

int32_t _M0MPB3Map3setGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS782,
  moonbit_string_t _M0L3keyS783,
  int32_t _M0L5valueS784
) {
  int32_t _M0L6_2atmpS2961;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2961 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS783);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsbE(_M0L4selfS782, _M0L3keyS783, _M0L5valueS784, _M0L6_2atmpS2961);
  return 0;
}

int32_t _M0MPB3Map3setGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS785,
  moonbit_string_t _M0L3keyS786,
  float _M0L5valueS787
) {
  int32_t _M0L6_2atmpS2962;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2962 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS786);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsfE(_M0L4selfS785, _M0L3keyS786, _M0L5valueS787, _M0L6_2atmpS2962);
  return 0;
}

int32_t _M0MPB3Map3setGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS788,
  moonbit_string_t _M0L3keyS789,
  int32_t _M0L5valueS790
) {
  int32_t _M0L6_2atmpS2963;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2963 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS789);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsiE(_M0L4selfS788, _M0L3keyS789, _M0L5valueS790, _M0L6_2atmpS2963);
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS699,
  moonbit_string_t _M0L3keyS705,
  void* _M0L5valueS706,
  int32_t _M0L4hashS701
) {
  int32_t _M0L14capacity__maskS2886;
  int32_t _M0L6_2atmpS2885;
  int32_t _M0L3pslS696;
  int32_t _M0L3idxS697;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2886 = _M0L4selfS699->$3;
  _M0L6_2atmpS2885 = _M0L4hashS701 & _M0L14capacity__maskS2886;
  _M0L3pslS696 = 0;
  _M0L3idxS697 = _M0L6_2atmpS2885;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2884 =
      _M0L4selfS699->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS698;
    if (
      _M0L3idxS697 < 0
      || _M0L3idxS697 >= Moonbit_array_length(_M0L7entriesS2884)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS698
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2884[
        _M0L3idxS697
      ];
    if (_M0L7_2abindS698 == 0) {
      int32_t _M0L4sizeS2869 = _M0L4selfS699->$1;
      int32_t _M0L8grow__atS2870 = _M0L4selfS699->$4;
      int32_t _M0L7_2abindS702;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS703;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS704;
      if (_M0L4sizeS2869 >= _M0L8grow__atS2870) {
        int32_t _M0L14capacity__maskS2872;
        int32_t _M0L6_2atmpS2871;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRP19moonbitDB10RedisValueE(_M0L4selfS699);
        _M0L14capacity__maskS2872 = _M0L4selfS699->$3;
        _M0L6_2atmpS2871 = _M0L4hashS701 & _M0L14capacity__maskS2872;
        _M0L3pslS696 = 0;
        _M0L3idxS697 = _M0L6_2atmpS2871;
        continue;
      }
      _M0L7_2abindS702 = _M0L4selfS699->$6;
      _M0L7_2abindS703 = 0;
      moonbit_incref(_M0L3keyS705);
      moonbit_incref(_M0L5valueS706);
      _M0L5entryS704
      = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE));
      Moonbit_object_header(_M0L5entryS704)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 78, 0);
      _M0L5entryS704->$0 = _M0L7_2abindS702;
      _M0L5entryS704->$1 = _M0L7_2abindS703;
      _M0L5entryS704->$2 = _M0L3pslS696;
      _M0L5entryS704->$3 = _M0L4hashS701;
      _M0L5entryS704->$4 = _M0L3keyS705;
      _M0L5entryS704->$5 = _M0L5valueS706;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(_M0L4selfS699, _M0L3idxS697, _M0L5entryS704);
      moonbit_decref(_M0L5entryS704);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS707 =
        _M0L7_2abindS698;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L14_2acurr__entryS708 =
        _M0L7_2aSomeS707;
      int32_t _M0L4hashS2874 = _M0L14_2acurr__entryS708->$3;
      int32_t _if__result_4717;
      int32_t _M0L3pslS2875;
      int32_t _M0L6_2atmpS2880;
      int32_t _M0L6_2atmpS2882;
      int32_t _M0L14capacity__maskS2883;
      int32_t _M0L6_2atmpS2881;
      if (_M0L4hashS2874 == _M0L4hashS701) {
        moonbit_string_t _M0L3keyS2873 = _M0L14_2acurr__entryS708->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4717
        = _M0L3keyS2873 == _M0L3keyS705
          || Moonbit_array_length(_M0L3keyS2873)
             == Moonbit_array_length(_M0L3keyS705)
             && 0
                == memcmp(_M0L3keyS2873, _M0L3keyS705, Moonbit_array_length(_M0L3keyS2873) * 2);
      } else {
        _if__result_4717 = 0;
      }
      if (_if__result_4717) {
        void* _M0L6_2aoldS4210 = _M0L14_2acurr__entryS708->$5;
        moonbit_incref(_M0L5valueS706);
        moonbit_decref(_M0L6_2aoldS4210);
        _M0L14_2acurr__entryS708->$5 = _M0L5valueS706;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS708);
      }
      _M0L3pslS2875 = _M0L14_2acurr__entryS708->$2;
      if (_M0L3pslS696 > _M0L3pslS2875) {
        int32_t _M0L4sizeS2876 = _M0L4selfS699->$1;
        int32_t _M0L8grow__atS2877 = _M0L4selfS699->$4;
        int32_t _M0L7_2abindS709;
        struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS710;
        struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS711;
        if (_M0L4sizeS2876 >= _M0L8grow__atS2877) {
          int32_t _M0L14capacity__maskS2879;
          int32_t _M0L6_2atmpS2878;
          moonbit_decref(_M0L14_2acurr__entryS708);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRP19moonbitDB10RedisValueE(_M0L4selfS699);
          _M0L14capacity__maskS2879 = _M0L4selfS699->$3;
          _M0L6_2atmpS2878 = _M0L4hashS701 & _M0L14capacity__maskS2879;
          _M0L3pslS696 = 0;
          _M0L3idxS697 = _M0L6_2atmpS2878;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP19moonbitDB10RedisValueE(_M0L4selfS699, _M0L3idxS697, _M0L14_2acurr__entryS708);
        moonbit_decref(_M0L14_2acurr__entryS708);
        _M0L7_2abindS709 = _M0L4selfS699->$6;
        _M0L7_2abindS710 = 0;
        moonbit_incref(_M0L3keyS705);
        moonbit_incref(_M0L5valueS706);
        _M0L5entryS711
        = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE));
        Moonbit_object_header(_M0L5entryS711)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 78, 0);
        _M0L5entryS711->$0 = _M0L7_2abindS709;
        _M0L5entryS711->$1 = _M0L7_2abindS710;
        _M0L5entryS711->$2 = _M0L3pslS696;
        _M0L5entryS711->$3 = _M0L4hashS701;
        _M0L5entryS711->$4 = _M0L3keyS705;
        _M0L5entryS711->$5 = _M0L5valueS706;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(_M0L4selfS699, _M0L3idxS697, _M0L5entryS711);
        moonbit_decref(_M0L5entryS711);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS708);
      }
      _M0L6_2atmpS2880 = _M0L3pslS696 + 1;
      _M0L6_2atmpS2882 = _M0L3idxS697 + 1;
      _M0L14capacity__maskS2883 = _M0L4selfS699->$3;
      _M0L6_2atmpS2881 = _M0L6_2atmpS2882 & _M0L14capacity__maskS2883;
      _M0L3pslS696 = _M0L6_2atmpS2880;
      _M0L3idxS697 = _M0L6_2atmpS2881;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGssE(
  struct _M0TPB3MapGssE* _M0L4selfS715,
  moonbit_string_t _M0L3keyS721,
  moonbit_string_t _M0L5valueS722,
  int32_t _M0L4hashS717
) {
  int32_t _M0L14capacity__maskS2904;
  int32_t _M0L6_2atmpS2903;
  int32_t _M0L3pslS712;
  int32_t _M0L3idxS713;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2904 = _M0L4selfS715->$3;
  _M0L6_2atmpS2903 = _M0L4hashS717 & _M0L14capacity__maskS2904;
  _M0L3pslS712 = 0;
  _M0L3idxS713 = _M0L6_2atmpS2903;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2902 = _M0L4selfS715->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS714;
    if (
      _M0L3idxS713 < 0
      || _M0L3idxS713 >= Moonbit_array_length(_M0L7entriesS2902)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS714
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2902[_M0L3idxS713];
    if (_M0L7_2abindS714 == 0) {
      int32_t _M0L4sizeS2887 = _M0L4selfS715->$1;
      int32_t _M0L8grow__atS2888 = _M0L4selfS715->$4;
      int32_t _M0L7_2abindS718;
      struct _M0TPB5EntryGssE* _M0L7_2abindS719;
      struct _M0TPB5EntryGssE* _M0L5entryS720;
      if (_M0L4sizeS2887 >= _M0L8grow__atS2888) {
        int32_t _M0L14capacity__maskS2890;
        int32_t _M0L6_2atmpS2889;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGssE(_M0L4selfS715);
        _M0L14capacity__maskS2890 = _M0L4selfS715->$3;
        _M0L6_2atmpS2889 = _M0L4hashS717 & _M0L14capacity__maskS2890;
        _M0L3pslS712 = 0;
        _M0L3idxS713 = _M0L6_2atmpS2889;
        continue;
      }
      _M0L7_2abindS718 = _M0L4selfS715->$6;
      _M0L7_2abindS719 = 0;
      moonbit_incref(_M0L3keyS721);
      moonbit_incref(_M0L5valueS722);
      _M0L5entryS720
      = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
      Moonbit_object_header(_M0L5entryS720)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 83, 0);
      _M0L5entryS720->$0 = _M0L7_2abindS718;
      _M0L5entryS720->$1 = _M0L7_2abindS719;
      _M0L5entryS720->$2 = _M0L3pslS712;
      _M0L5entryS720->$3 = _M0L4hashS717;
      _M0L5entryS720->$4 = _M0L3keyS721;
      _M0L5entryS720->$5 = _M0L5valueS722;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS715, _M0L3idxS713, _M0L5entryS720);
      moonbit_decref(_M0L5entryS720);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS723 = _M0L7_2abindS714;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS724 = _M0L7_2aSomeS723;
      int32_t _M0L4hashS2892 = _M0L14_2acurr__entryS724->$3;
      int32_t _if__result_4719;
      int32_t _M0L3pslS2893;
      int32_t _M0L6_2atmpS2898;
      int32_t _M0L6_2atmpS2900;
      int32_t _M0L14capacity__maskS2901;
      int32_t _M0L6_2atmpS2899;
      if (_M0L4hashS2892 == _M0L4hashS717) {
        moonbit_string_t _M0L3keyS2891 = _M0L14_2acurr__entryS724->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4719
        = _M0L3keyS2891 == _M0L3keyS721
          || Moonbit_array_length(_M0L3keyS2891)
             == Moonbit_array_length(_M0L3keyS721)
             && 0
                == memcmp(_M0L3keyS2891, _M0L3keyS721, Moonbit_array_length(_M0L3keyS2891) * 2);
      } else {
        _if__result_4719 = 0;
      }
      if (_if__result_4719) {
        moonbit_string_t _M0L6_2aoldS4214 = _M0L14_2acurr__entryS724->$5;
        moonbit_incref(_M0L5valueS722);
        moonbit_decref(_M0L6_2aoldS4214);
        _M0L14_2acurr__entryS724->$5 = _M0L5valueS722;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS724);
      }
      _M0L3pslS2893 = _M0L14_2acurr__entryS724->$2;
      if (_M0L3pslS712 > _M0L3pslS2893) {
        int32_t _M0L4sizeS2894 = _M0L4selfS715->$1;
        int32_t _M0L8grow__atS2895 = _M0L4selfS715->$4;
        int32_t _M0L7_2abindS725;
        struct _M0TPB5EntryGssE* _M0L7_2abindS726;
        struct _M0TPB5EntryGssE* _M0L5entryS727;
        if (_M0L4sizeS2894 >= _M0L8grow__atS2895) {
          int32_t _M0L14capacity__maskS2897;
          int32_t _M0L6_2atmpS2896;
          moonbit_decref(_M0L14_2acurr__entryS724);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGssE(_M0L4selfS715);
          _M0L14capacity__maskS2897 = _M0L4selfS715->$3;
          _M0L6_2atmpS2896 = _M0L4hashS717 & _M0L14capacity__maskS2897;
          _M0L3pslS712 = 0;
          _M0L3idxS713 = _M0L6_2atmpS2896;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS715, _M0L3idxS713, _M0L14_2acurr__entryS724);
        moonbit_decref(_M0L14_2acurr__entryS724);
        _M0L7_2abindS725 = _M0L4selfS715->$6;
        _M0L7_2abindS726 = 0;
        moonbit_incref(_M0L3keyS721);
        moonbit_incref(_M0L5valueS722);
        _M0L5entryS727
        = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
        Moonbit_object_header(_M0L5entryS727)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 83, 0);
        _M0L5entryS727->$0 = _M0L7_2abindS725;
        _M0L5entryS727->$1 = _M0L7_2abindS726;
        _M0L5entryS727->$2 = _M0L3pslS712;
        _M0L5entryS727->$3 = _M0L4hashS717;
        _M0L5entryS727->$4 = _M0L3keyS721;
        _M0L5entryS727->$5 = _M0L5valueS722;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS715, _M0L3idxS713, _M0L5entryS727);
        moonbit_decref(_M0L5entryS727);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS724);
      }
      _M0L6_2atmpS2898 = _M0L3pslS712 + 1;
      _M0L6_2atmpS2900 = _M0L3idxS713 + 1;
      _M0L14capacity__maskS2901 = _M0L4selfS715->$3;
      _M0L6_2atmpS2899 = _M0L6_2atmpS2900 & _M0L14capacity__maskS2901;
      _M0L3pslS712 = _M0L6_2atmpS2898;
      _M0L3idxS713 = _M0L6_2atmpS2899;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS731,
  moonbit_string_t _M0L3keyS737,
  int32_t _M0L5valueS738,
  int32_t _M0L4hashS733
) {
  int32_t _M0L14capacity__maskS2922;
  int32_t _M0L6_2atmpS2921;
  int32_t _M0L3pslS728;
  int32_t _M0L3idxS729;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2922 = _M0L4selfS731->$3;
  _M0L6_2atmpS2921 = _M0L4hashS733 & _M0L14capacity__maskS2922;
  _M0L3pslS728 = 0;
  _M0L3idxS729 = _M0L6_2atmpS2921;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2920 = _M0L4selfS731->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS730;
    if (
      _M0L3idxS729 < 0
      || _M0L3idxS729 >= Moonbit_array_length(_M0L7entriesS2920)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS730
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2920[_M0L3idxS729];
    if (_M0L7_2abindS730 == 0) {
      int32_t _M0L4sizeS2905 = _M0L4selfS731->$1;
      int32_t _M0L8grow__atS2906 = _M0L4selfS731->$4;
      int32_t _M0L7_2abindS734;
      struct _M0TPB5EntryGsbE* _M0L7_2abindS735;
      struct _M0TPB5EntryGsbE* _M0L5entryS736;
      if (_M0L4sizeS2905 >= _M0L8grow__atS2906) {
        int32_t _M0L14capacity__maskS2908;
        int32_t _M0L6_2atmpS2907;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsbE(_M0L4selfS731);
        _M0L14capacity__maskS2908 = _M0L4selfS731->$3;
        _M0L6_2atmpS2907 = _M0L4hashS733 & _M0L14capacity__maskS2908;
        _M0L3pslS728 = 0;
        _M0L3idxS729 = _M0L6_2atmpS2907;
        continue;
      }
      _M0L7_2abindS734 = _M0L4selfS731->$6;
      _M0L7_2abindS735 = 0;
      moonbit_incref(_M0L3keyS737);
      _M0L5entryS736
      = (struct _M0TPB5EntryGsbE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsbE));
      Moonbit_object_header(_M0L5entryS736)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 88, 0);
      _M0L5entryS736->$0 = _M0L7_2abindS734;
      _M0L5entryS736->$1 = _M0L7_2abindS735;
      _M0L5entryS736->$2 = _M0L3pslS728;
      _M0L5entryS736->$3 = _M0L4hashS733;
      _M0L5entryS736->$4 = _M0L3keyS737;
      _M0L5entryS736->$5 = _M0L5valueS738;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS731, _M0L3idxS729, _M0L5entryS736);
      moonbit_decref(_M0L5entryS736);
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS739 = _M0L7_2abindS730;
      struct _M0TPB5EntryGsbE* _M0L14_2acurr__entryS740 = _M0L7_2aSomeS739;
      int32_t _M0L4hashS2910 = _M0L14_2acurr__entryS740->$3;
      int32_t _if__result_4721;
      int32_t _M0L3pslS2911;
      int32_t _M0L6_2atmpS2916;
      int32_t _M0L6_2atmpS2918;
      int32_t _M0L14capacity__maskS2919;
      int32_t _M0L6_2atmpS2917;
      if (_M0L4hashS2910 == _M0L4hashS733) {
        moonbit_string_t _M0L3keyS2909 = _M0L14_2acurr__entryS740->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4721
        = _M0L3keyS2909 == _M0L3keyS737
          || Moonbit_array_length(_M0L3keyS2909)
             == Moonbit_array_length(_M0L3keyS737)
             && 0
                == memcmp(_M0L3keyS2909, _M0L3keyS737, Moonbit_array_length(_M0L3keyS2909) * 2);
      } else {
        _if__result_4721 = 0;
      }
      if (_if__result_4721) {
        _M0L14_2acurr__entryS740->$5 = _M0L5valueS738;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS740);
      }
      _M0L3pslS2911 = _M0L14_2acurr__entryS740->$2;
      if (_M0L3pslS728 > _M0L3pslS2911) {
        int32_t _M0L4sizeS2912 = _M0L4selfS731->$1;
        int32_t _M0L8grow__atS2913 = _M0L4selfS731->$4;
        int32_t _M0L7_2abindS741;
        struct _M0TPB5EntryGsbE* _M0L7_2abindS742;
        struct _M0TPB5EntryGsbE* _M0L5entryS743;
        if (_M0L4sizeS2912 >= _M0L8grow__atS2913) {
          int32_t _M0L14capacity__maskS2915;
          int32_t _M0L6_2atmpS2914;
          moonbit_decref(_M0L14_2acurr__entryS740);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsbE(_M0L4selfS731);
          _M0L14capacity__maskS2915 = _M0L4selfS731->$3;
          _M0L6_2atmpS2914 = _M0L4hashS733 & _M0L14capacity__maskS2915;
          _M0L3pslS728 = 0;
          _M0L3idxS729 = _M0L6_2atmpS2914;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsbE(_M0L4selfS731, _M0L3idxS729, _M0L14_2acurr__entryS740);
        moonbit_decref(_M0L14_2acurr__entryS740);
        _M0L7_2abindS741 = _M0L4selfS731->$6;
        _M0L7_2abindS742 = 0;
        moonbit_incref(_M0L3keyS737);
        _M0L5entryS743
        = (struct _M0TPB5EntryGsbE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsbE));
        Moonbit_object_header(_M0L5entryS743)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 88, 0);
        _M0L5entryS743->$0 = _M0L7_2abindS741;
        _M0L5entryS743->$1 = _M0L7_2abindS742;
        _M0L5entryS743->$2 = _M0L3pslS728;
        _M0L5entryS743->$3 = _M0L4hashS733;
        _M0L5entryS743->$4 = _M0L3keyS737;
        _M0L5entryS743->$5 = _M0L5valueS738;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS731, _M0L3idxS729, _M0L5entryS743);
        moonbit_decref(_M0L5entryS743);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS740);
      }
      _M0L6_2atmpS2916 = _M0L3pslS728 + 1;
      _M0L6_2atmpS2918 = _M0L3idxS729 + 1;
      _M0L14capacity__maskS2919 = _M0L4selfS731->$3;
      _M0L6_2atmpS2917 = _M0L6_2atmpS2918 & _M0L14capacity__maskS2919;
      _M0L3pslS728 = _M0L6_2atmpS2916;
      _M0L3idxS729 = _M0L6_2atmpS2917;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS747,
  moonbit_string_t _M0L3keyS753,
  float _M0L5valueS754,
  int32_t _M0L4hashS749
) {
  int32_t _M0L14capacity__maskS2940;
  int32_t _M0L6_2atmpS2939;
  int32_t _M0L3pslS744;
  int32_t _M0L3idxS745;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2940 = _M0L4selfS747->$3;
  _M0L6_2atmpS2939 = _M0L4hashS749 & _M0L14capacity__maskS2940;
  _M0L3pslS744 = 0;
  _M0L3idxS745 = _M0L6_2atmpS2939;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2938 = _M0L4selfS747->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS746;
    if (
      _M0L3idxS745 < 0
      || _M0L3idxS745 >= Moonbit_array_length(_M0L7entriesS2938)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS746
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2938[_M0L3idxS745];
    if (_M0L7_2abindS746 == 0) {
      int32_t _M0L4sizeS2923 = _M0L4selfS747->$1;
      int32_t _M0L8grow__atS2924 = _M0L4selfS747->$4;
      int32_t _M0L7_2abindS750;
      struct _M0TPB5EntryGsfE* _M0L7_2abindS751;
      struct _M0TPB5EntryGsfE* _M0L5entryS752;
      if (_M0L4sizeS2923 >= _M0L8grow__atS2924) {
        int32_t _M0L14capacity__maskS2926;
        int32_t _M0L6_2atmpS2925;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsfE(_M0L4selfS747);
        _M0L14capacity__maskS2926 = _M0L4selfS747->$3;
        _M0L6_2atmpS2925 = _M0L4hashS749 & _M0L14capacity__maskS2926;
        _M0L3pslS744 = 0;
        _M0L3idxS745 = _M0L6_2atmpS2925;
        continue;
      }
      _M0L7_2abindS750 = _M0L4selfS747->$6;
      _M0L7_2abindS751 = 0;
      moonbit_incref(_M0L3keyS753);
      _M0L5entryS752
      = (struct _M0TPB5EntryGsfE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsfE));
      Moonbit_object_header(_M0L5entryS752)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 92, 0);
      _M0L5entryS752->$0 = _M0L7_2abindS750;
      _M0L5entryS752->$1 = _M0L7_2abindS751;
      _M0L5entryS752->$2 = _M0L3pslS744;
      _M0L5entryS752->$3 = _M0L4hashS749;
      _M0L5entryS752->$4 = _M0L3keyS753;
      _M0L5entryS752->$5 = _M0L5valueS754;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS747, _M0L3idxS745, _M0L5entryS752);
      moonbit_decref(_M0L5entryS752);
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS755 = _M0L7_2abindS746;
      struct _M0TPB5EntryGsfE* _M0L14_2acurr__entryS756 = _M0L7_2aSomeS755;
      int32_t _M0L4hashS2928 = _M0L14_2acurr__entryS756->$3;
      int32_t _if__result_4723;
      int32_t _M0L3pslS2929;
      int32_t _M0L6_2atmpS2934;
      int32_t _M0L6_2atmpS2936;
      int32_t _M0L14capacity__maskS2937;
      int32_t _M0L6_2atmpS2935;
      if (_M0L4hashS2928 == _M0L4hashS749) {
        moonbit_string_t _M0L3keyS2927 = _M0L14_2acurr__entryS756->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4723
        = _M0L3keyS2927 == _M0L3keyS753
          || Moonbit_array_length(_M0L3keyS2927)
             == Moonbit_array_length(_M0L3keyS753)
             && 0
                == memcmp(_M0L3keyS2927, _M0L3keyS753, Moonbit_array_length(_M0L3keyS2927) * 2);
      } else {
        _if__result_4723 = 0;
      }
      if (_if__result_4723) {
        _M0L14_2acurr__entryS756->$5 = _M0L5valueS754;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS756);
      }
      _M0L3pslS2929 = _M0L14_2acurr__entryS756->$2;
      if (_M0L3pslS744 > _M0L3pslS2929) {
        int32_t _M0L4sizeS2930 = _M0L4selfS747->$1;
        int32_t _M0L8grow__atS2931 = _M0L4selfS747->$4;
        int32_t _M0L7_2abindS757;
        struct _M0TPB5EntryGsfE* _M0L7_2abindS758;
        struct _M0TPB5EntryGsfE* _M0L5entryS759;
        if (_M0L4sizeS2930 >= _M0L8grow__atS2931) {
          int32_t _M0L14capacity__maskS2933;
          int32_t _M0L6_2atmpS2932;
          moonbit_decref(_M0L14_2acurr__entryS756);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsfE(_M0L4selfS747);
          _M0L14capacity__maskS2933 = _M0L4selfS747->$3;
          _M0L6_2atmpS2932 = _M0L4hashS749 & _M0L14capacity__maskS2933;
          _M0L3pslS744 = 0;
          _M0L3idxS745 = _M0L6_2atmpS2932;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsfE(_M0L4selfS747, _M0L3idxS745, _M0L14_2acurr__entryS756);
        moonbit_decref(_M0L14_2acurr__entryS756);
        _M0L7_2abindS757 = _M0L4selfS747->$6;
        _M0L7_2abindS758 = 0;
        moonbit_incref(_M0L3keyS753);
        _M0L5entryS759
        = (struct _M0TPB5EntryGsfE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsfE));
        Moonbit_object_header(_M0L5entryS759)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 92, 0);
        _M0L5entryS759->$0 = _M0L7_2abindS757;
        _M0L5entryS759->$1 = _M0L7_2abindS758;
        _M0L5entryS759->$2 = _M0L3pslS744;
        _M0L5entryS759->$3 = _M0L4hashS749;
        _M0L5entryS759->$4 = _M0L3keyS753;
        _M0L5entryS759->$5 = _M0L5valueS754;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS747, _M0L3idxS745, _M0L5entryS759);
        moonbit_decref(_M0L5entryS759);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS756);
      }
      _M0L6_2atmpS2934 = _M0L3pslS744 + 1;
      _M0L6_2atmpS2936 = _M0L3idxS745 + 1;
      _M0L14capacity__maskS2937 = _M0L4selfS747->$3;
      _M0L6_2atmpS2935 = _M0L6_2atmpS2936 & _M0L14capacity__maskS2937;
      _M0L3pslS744 = _M0L6_2atmpS2934;
      _M0L3idxS745 = _M0L6_2atmpS2935;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS763,
  moonbit_string_t _M0L3keyS769,
  int32_t _M0L5valueS770,
  int32_t _M0L4hashS765
) {
  int32_t _M0L14capacity__maskS2958;
  int32_t _M0L6_2atmpS2957;
  int32_t _M0L3pslS760;
  int32_t _M0L3idxS761;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2958 = _M0L4selfS763->$3;
  _M0L6_2atmpS2957 = _M0L4hashS765 & _M0L14capacity__maskS2958;
  _M0L3pslS760 = 0;
  _M0L3idxS761 = _M0L6_2atmpS2957;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2956 = _M0L4selfS763->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS762;
    if (
      _M0L3idxS761 < 0
      || _M0L3idxS761 >= Moonbit_array_length(_M0L7entriesS2956)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS762
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2956[_M0L3idxS761];
    if (_M0L7_2abindS762 == 0) {
      int32_t _M0L4sizeS2941 = _M0L4selfS763->$1;
      int32_t _M0L8grow__atS2942 = _M0L4selfS763->$4;
      int32_t _M0L7_2abindS766;
      struct _M0TPB5EntryGsiE* _M0L7_2abindS767;
      struct _M0TPB5EntryGsiE* _M0L5entryS768;
      if (_M0L4sizeS2941 >= _M0L8grow__atS2942) {
        int32_t _M0L14capacity__maskS2944;
        int32_t _M0L6_2atmpS2943;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsiE(_M0L4selfS763);
        _M0L14capacity__maskS2944 = _M0L4selfS763->$3;
        _M0L6_2atmpS2943 = _M0L4hashS765 & _M0L14capacity__maskS2944;
        _M0L3pslS760 = 0;
        _M0L3idxS761 = _M0L6_2atmpS2943;
        continue;
      }
      _M0L7_2abindS766 = _M0L4selfS763->$6;
      _M0L7_2abindS767 = 0;
      moonbit_incref(_M0L3keyS769);
      _M0L5entryS768
      = (struct _M0TPB5EntryGsiE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsiE));
      Moonbit_object_header(_M0L5entryS768)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 96, 0);
      _M0L5entryS768->$0 = _M0L7_2abindS766;
      _M0L5entryS768->$1 = _M0L7_2abindS767;
      _M0L5entryS768->$2 = _M0L3pslS760;
      _M0L5entryS768->$3 = _M0L4hashS765;
      _M0L5entryS768->$4 = _M0L3keyS769;
      _M0L5entryS768->$5 = _M0L5valueS770;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS763, _M0L3idxS761, _M0L5entryS768);
      moonbit_decref(_M0L5entryS768);
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS771 = _M0L7_2abindS762;
      struct _M0TPB5EntryGsiE* _M0L14_2acurr__entryS772 = _M0L7_2aSomeS771;
      int32_t _M0L4hashS2946 = _M0L14_2acurr__entryS772->$3;
      int32_t _if__result_4725;
      int32_t _M0L3pslS2947;
      int32_t _M0L6_2atmpS2952;
      int32_t _M0L6_2atmpS2954;
      int32_t _M0L14capacity__maskS2955;
      int32_t _M0L6_2atmpS2953;
      if (_M0L4hashS2946 == _M0L4hashS765) {
        moonbit_string_t _M0L3keyS2945 = _M0L14_2acurr__entryS772->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4725
        = _M0L3keyS2945 == _M0L3keyS769
          || Moonbit_array_length(_M0L3keyS2945)
             == Moonbit_array_length(_M0L3keyS769)
             && 0
                == memcmp(_M0L3keyS2945, _M0L3keyS769, Moonbit_array_length(_M0L3keyS2945) * 2);
      } else {
        _if__result_4725 = 0;
      }
      if (_if__result_4725) {
        _M0L14_2acurr__entryS772->$5 = _M0L5valueS770;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS772);
      }
      _M0L3pslS2947 = _M0L14_2acurr__entryS772->$2;
      if (_M0L3pslS760 > _M0L3pslS2947) {
        int32_t _M0L4sizeS2948 = _M0L4selfS763->$1;
        int32_t _M0L8grow__atS2949 = _M0L4selfS763->$4;
        int32_t _M0L7_2abindS773;
        struct _M0TPB5EntryGsiE* _M0L7_2abindS774;
        struct _M0TPB5EntryGsiE* _M0L5entryS775;
        if (_M0L4sizeS2948 >= _M0L8grow__atS2949) {
          int32_t _M0L14capacity__maskS2951;
          int32_t _M0L6_2atmpS2950;
          moonbit_decref(_M0L14_2acurr__entryS772);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsiE(_M0L4selfS763);
          _M0L14capacity__maskS2951 = _M0L4selfS763->$3;
          _M0L6_2atmpS2950 = _M0L4hashS765 & _M0L14capacity__maskS2951;
          _M0L3pslS760 = 0;
          _M0L3idxS761 = _M0L6_2atmpS2950;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsiE(_M0L4selfS763, _M0L3idxS761, _M0L14_2acurr__entryS772);
        moonbit_decref(_M0L14_2acurr__entryS772);
        _M0L7_2abindS773 = _M0L4selfS763->$6;
        _M0L7_2abindS774 = 0;
        moonbit_incref(_M0L3keyS769);
        _M0L5entryS775
        = (struct _M0TPB5EntryGsiE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsiE));
        Moonbit_object_header(_M0L5entryS775)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 96, 0);
        _M0L5entryS775->$0 = _M0L7_2abindS773;
        _M0L5entryS775->$1 = _M0L7_2abindS774;
        _M0L5entryS775->$2 = _M0L3pslS760;
        _M0L5entryS775->$3 = _M0L4hashS765;
        _M0L5entryS775->$4 = _M0L3keyS769;
        _M0L5entryS775->$5 = _M0L5valueS770;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS763, _M0L3idxS761, _M0L5entryS775);
        moonbit_decref(_M0L5entryS775);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS772);
      }
      _M0L6_2atmpS2952 = _M0L3pslS760 + 1;
      _M0L6_2atmpS2954 = _M0L3idxS761 + 1;
      _M0L14capacity__maskS2955 = _M0L4selfS763->$3;
      _M0L6_2atmpS2953 = _M0L6_2atmpS2954 & _M0L14capacity__maskS2955;
      _M0L3pslS760 = _M0L6_2atmpS2952;
      _M0L3idxS761 = _M0L6_2atmpS2953;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS657
) {
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L9old__headS656;
  int32_t _M0L8capacityS2836;
  int32_t _M0L13new__capacityS658;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2830;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L6_2atmpS2829;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L6_2aoldS4230;
  int32_t _M0L6_2atmpS2831;
  int32_t _M0L8capacityS2833;
  int32_t _M0L6_2atmpS2832;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2834;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4229;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L1xS659;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS656 = _M0L4selfS657->$5;
  _M0L8capacityS2836 = _M0L4selfS657->$2;
  _M0L13new__capacityS658 = _M0L8capacityS2836 << 1;
  _M0L6_2atmpS2830 = 0;
  _M0L6_2atmpS2829
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE**)moonbit_make_ref_array(_M0L13new__capacityS658, _M0L6_2atmpS2830);
  _M0L6_2aoldS4230 = _M0L4selfS657->$0;
  if (_M0L9old__headS656) {
    moonbit_incref(_M0L9old__headS656);
  }
  moonbit_decref(_M0L6_2aoldS4230);
  _M0L4selfS657->$0 = _M0L6_2atmpS2829;
  _M0L4selfS657->$2 = _M0L13new__capacityS658;
  _M0L6_2atmpS2831 = _M0L13new__capacityS658 - 1;
  _M0L4selfS657->$3 = _M0L6_2atmpS2831;
  _M0L8capacityS2833 = _M0L4selfS657->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2832 = _M0FPB21calc__grow__threshold(_M0L8capacityS2833);
  _M0L4selfS657->$4 = _M0L6_2atmpS2832;
  _M0L4selfS657->$1 = 0;
  _M0L6_2atmpS2834 = 0;
  _M0L6_2aoldS4229 = _M0L4selfS657->$5;
  if (_M0L6_2aoldS4229) {
    moonbit_decref(_M0L6_2aoldS4229);
  }
  _M0L4selfS657->$5 = _M0L6_2atmpS2834;
  _M0L4selfS657->$6 = -1;
  _M0L1xS659 = _M0L9old__headS656;
  while (1) {
    if (_M0L1xS659 == 0) {
      if (_M0L1xS659) {
        moonbit_decref(_M0L1xS659);
      }
      break;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS661 =
        _M0L1xS659;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4_2aeS662 =
        _M0L7_2aSomeS661;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L15next__in__chainS663 =
        _M0L4_2aeS662->$1;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2835 = 0;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4227 =
        _M0L4_2aeS662->$1;
      if (_M0L15next__in__chainS663) {
        moonbit_incref(_M0L15next__in__chainS663);
      }
      if (_M0L6_2aoldS4227) {
        moonbit_decref(_M0L6_2aoldS4227);
      }
      _M0L4_2aeS662->$1 = _M0L6_2atmpS2835;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS657, _M0L4_2aeS662);
      moonbit_decref(_M0L4_2aeS662);
      _M0L1xS659 = _M0L15next__in__chainS663;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE* _M0L4selfS665) {
  struct _M0TPB5EntryGssE* _M0L9old__headS664;
  int32_t _M0L8capacityS2844;
  int32_t _M0L13new__capacityS666;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2838;
  struct _M0TPB5EntryGssE** _M0L6_2atmpS2837;
  struct _M0TPB5EntryGssE** _M0L6_2aoldS4235;
  int32_t _M0L6_2atmpS2839;
  int32_t _M0L8capacityS2841;
  int32_t _M0L6_2atmpS2840;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2842;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS4234;
  struct _M0TPB5EntryGssE* _M0L1xS667;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS664 = _M0L4selfS665->$5;
  _M0L8capacityS2844 = _M0L4selfS665->$2;
  _M0L13new__capacityS666 = _M0L8capacityS2844 << 1;
  _M0L6_2atmpS2838 = 0;
  _M0L6_2atmpS2837
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L13new__capacityS666, _M0L6_2atmpS2838);
  _M0L6_2aoldS4235 = _M0L4selfS665->$0;
  if (_M0L9old__headS664) {
    moonbit_incref(_M0L9old__headS664);
  }
  moonbit_decref(_M0L6_2aoldS4235);
  _M0L4selfS665->$0 = _M0L6_2atmpS2837;
  _M0L4selfS665->$2 = _M0L13new__capacityS666;
  _M0L6_2atmpS2839 = _M0L13new__capacityS666 - 1;
  _M0L4selfS665->$3 = _M0L6_2atmpS2839;
  _M0L8capacityS2841 = _M0L4selfS665->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2840 = _M0FPB21calc__grow__threshold(_M0L8capacityS2841);
  _M0L4selfS665->$4 = _M0L6_2atmpS2840;
  _M0L4selfS665->$1 = 0;
  _M0L6_2atmpS2842 = 0;
  _M0L6_2aoldS4234 = _M0L4selfS665->$5;
  if (_M0L6_2aoldS4234) {
    moonbit_decref(_M0L6_2aoldS4234);
  }
  _M0L4selfS665->$5 = _M0L6_2atmpS2842;
  _M0L4selfS665->$6 = -1;
  _M0L1xS667 = _M0L9old__headS664;
  while (1) {
    if (_M0L1xS667 == 0) {
      if (_M0L1xS667) {
        moonbit_decref(_M0L1xS667);
      }
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS669 = _M0L1xS667;
      struct _M0TPB5EntryGssE* _M0L4_2aeS670 = _M0L7_2aSomeS669;
      struct _M0TPB5EntryGssE* _M0L15next__in__chainS671 = _M0L4_2aeS670->$1;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2843 = 0;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4232 = _M0L4_2aeS670->$1;
      if (_M0L15next__in__chainS671) {
        moonbit_incref(_M0L15next__in__chainS671);
      }
      if (_M0L6_2aoldS4232) {
        moonbit_decref(_M0L6_2aoldS4232);
      }
      _M0L4_2aeS670->$1 = _M0L6_2atmpS2843;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGssE(_M0L4selfS665, _M0L4_2aeS670);
      moonbit_decref(_M0L4_2aeS670);
      _M0L1xS667 = _M0L15next__in__chainS671;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsbE(struct _M0TPB3MapGsbE* _M0L4selfS673) {
  struct _M0TPB5EntryGsbE* _M0L9old__headS672;
  int32_t _M0L8capacityS2852;
  int32_t _M0L13new__capacityS674;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2846;
  struct _M0TPB5EntryGsbE** _M0L6_2atmpS2845;
  struct _M0TPB5EntryGsbE** _M0L6_2aoldS4240;
  int32_t _M0L6_2atmpS2847;
  int32_t _M0L8capacityS2849;
  int32_t _M0L6_2atmpS2848;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2850;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS4239;
  struct _M0TPB5EntryGsbE* _M0L1xS675;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS672 = _M0L4selfS673->$5;
  _M0L8capacityS2852 = _M0L4selfS673->$2;
  _M0L13new__capacityS674 = _M0L8capacityS2852 << 1;
  _M0L6_2atmpS2846 = 0;
  _M0L6_2atmpS2845
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L13new__capacityS674, _M0L6_2atmpS2846);
  _M0L6_2aoldS4240 = _M0L4selfS673->$0;
  if (_M0L9old__headS672) {
    moonbit_incref(_M0L9old__headS672);
  }
  moonbit_decref(_M0L6_2aoldS4240);
  _M0L4selfS673->$0 = _M0L6_2atmpS2845;
  _M0L4selfS673->$2 = _M0L13new__capacityS674;
  _M0L6_2atmpS2847 = _M0L13new__capacityS674 - 1;
  _M0L4selfS673->$3 = _M0L6_2atmpS2847;
  _M0L8capacityS2849 = _M0L4selfS673->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2848 = _M0FPB21calc__grow__threshold(_M0L8capacityS2849);
  _M0L4selfS673->$4 = _M0L6_2atmpS2848;
  _M0L4selfS673->$1 = 0;
  _M0L6_2atmpS2850 = 0;
  _M0L6_2aoldS4239 = _M0L4selfS673->$5;
  if (_M0L6_2aoldS4239) {
    moonbit_decref(_M0L6_2aoldS4239);
  }
  _M0L4selfS673->$5 = _M0L6_2atmpS2850;
  _M0L4selfS673->$6 = -1;
  _M0L1xS675 = _M0L9old__headS672;
  while (1) {
    if (_M0L1xS675 == 0) {
      if (_M0L1xS675) {
        moonbit_decref(_M0L1xS675);
      }
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS677 = _M0L1xS675;
      struct _M0TPB5EntryGsbE* _M0L4_2aeS678 = _M0L7_2aSomeS677;
      struct _M0TPB5EntryGsbE* _M0L15next__in__chainS679 = _M0L4_2aeS678->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2851 = 0;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4237 = _M0L4_2aeS678->$1;
      if (_M0L15next__in__chainS679) {
        moonbit_incref(_M0L15next__in__chainS679);
      }
      if (_M0L6_2aoldS4237) {
        moonbit_decref(_M0L6_2aoldS4237);
      }
      _M0L4_2aeS678->$1 = _M0L6_2atmpS2851;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsbE(_M0L4selfS673, _M0L4_2aeS678);
      moonbit_decref(_M0L4_2aeS678);
      _M0L1xS675 = _M0L15next__in__chainS679;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsfE(struct _M0TPB3MapGsfE* _M0L4selfS681) {
  struct _M0TPB5EntryGsfE* _M0L9old__headS680;
  int32_t _M0L8capacityS2860;
  int32_t _M0L13new__capacityS682;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2854;
  struct _M0TPB5EntryGsfE** _M0L6_2atmpS2853;
  struct _M0TPB5EntryGsfE** _M0L6_2aoldS4245;
  int32_t _M0L6_2atmpS2855;
  int32_t _M0L8capacityS2857;
  int32_t _M0L6_2atmpS2856;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2858;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS4244;
  struct _M0TPB5EntryGsfE* _M0L1xS683;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS680 = _M0L4selfS681->$5;
  _M0L8capacityS2860 = _M0L4selfS681->$2;
  _M0L13new__capacityS682 = _M0L8capacityS2860 << 1;
  _M0L6_2atmpS2854 = 0;
  _M0L6_2atmpS2853
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L13new__capacityS682, _M0L6_2atmpS2854);
  _M0L6_2aoldS4245 = _M0L4selfS681->$0;
  if (_M0L9old__headS680) {
    moonbit_incref(_M0L9old__headS680);
  }
  moonbit_decref(_M0L6_2aoldS4245);
  _M0L4selfS681->$0 = _M0L6_2atmpS2853;
  _M0L4selfS681->$2 = _M0L13new__capacityS682;
  _M0L6_2atmpS2855 = _M0L13new__capacityS682 - 1;
  _M0L4selfS681->$3 = _M0L6_2atmpS2855;
  _M0L8capacityS2857 = _M0L4selfS681->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2856 = _M0FPB21calc__grow__threshold(_M0L8capacityS2857);
  _M0L4selfS681->$4 = _M0L6_2atmpS2856;
  _M0L4selfS681->$1 = 0;
  _M0L6_2atmpS2858 = 0;
  _M0L6_2aoldS4244 = _M0L4selfS681->$5;
  if (_M0L6_2aoldS4244) {
    moonbit_decref(_M0L6_2aoldS4244);
  }
  _M0L4selfS681->$5 = _M0L6_2atmpS2858;
  _M0L4selfS681->$6 = -1;
  _M0L1xS683 = _M0L9old__headS680;
  while (1) {
    if (_M0L1xS683 == 0) {
      if (_M0L1xS683) {
        moonbit_decref(_M0L1xS683);
      }
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS685 = _M0L1xS683;
      struct _M0TPB5EntryGsfE* _M0L4_2aeS686 = _M0L7_2aSomeS685;
      struct _M0TPB5EntryGsfE* _M0L15next__in__chainS687 = _M0L4_2aeS686->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2859 = 0;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4242 = _M0L4_2aeS686->$1;
      if (_M0L15next__in__chainS687) {
        moonbit_incref(_M0L15next__in__chainS687);
      }
      if (_M0L6_2aoldS4242) {
        moonbit_decref(_M0L6_2aoldS4242);
      }
      _M0L4_2aeS686->$1 = _M0L6_2atmpS2859;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsfE(_M0L4selfS681, _M0L4_2aeS686);
      moonbit_decref(_M0L4_2aeS686);
      _M0L1xS683 = _M0L15next__in__chainS687;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsiE(struct _M0TPB3MapGsiE* _M0L4selfS689) {
  struct _M0TPB5EntryGsiE* _M0L9old__headS688;
  int32_t _M0L8capacityS2868;
  int32_t _M0L13new__capacityS690;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2862;
  struct _M0TPB5EntryGsiE** _M0L6_2atmpS2861;
  struct _M0TPB5EntryGsiE** _M0L6_2aoldS4250;
  int32_t _M0L6_2atmpS2863;
  int32_t _M0L8capacityS2865;
  int32_t _M0L6_2atmpS2864;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2866;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS4249;
  struct _M0TPB5EntryGsiE* _M0L1xS691;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS688 = _M0L4selfS689->$5;
  _M0L8capacityS2868 = _M0L4selfS689->$2;
  _M0L13new__capacityS690 = _M0L8capacityS2868 << 1;
  _M0L6_2atmpS2862 = 0;
  _M0L6_2atmpS2861
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L13new__capacityS690, _M0L6_2atmpS2862);
  _M0L6_2aoldS4250 = _M0L4selfS689->$0;
  if (_M0L9old__headS688) {
    moonbit_incref(_M0L9old__headS688);
  }
  moonbit_decref(_M0L6_2aoldS4250);
  _M0L4selfS689->$0 = _M0L6_2atmpS2861;
  _M0L4selfS689->$2 = _M0L13new__capacityS690;
  _M0L6_2atmpS2863 = _M0L13new__capacityS690 - 1;
  _M0L4selfS689->$3 = _M0L6_2atmpS2863;
  _M0L8capacityS2865 = _M0L4selfS689->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2864 = _M0FPB21calc__grow__threshold(_M0L8capacityS2865);
  _M0L4selfS689->$4 = _M0L6_2atmpS2864;
  _M0L4selfS689->$1 = 0;
  _M0L6_2atmpS2866 = 0;
  _M0L6_2aoldS4249 = _M0L4selfS689->$5;
  if (_M0L6_2aoldS4249) {
    moonbit_decref(_M0L6_2aoldS4249);
  }
  _M0L4selfS689->$5 = _M0L6_2atmpS2866;
  _M0L4selfS689->$6 = -1;
  _M0L1xS691 = _M0L9old__headS688;
  while (1) {
    if (_M0L1xS691 == 0) {
      if (_M0L1xS691) {
        moonbit_decref(_M0L1xS691);
      }
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS693 = _M0L1xS691;
      struct _M0TPB5EntryGsiE* _M0L4_2aeS694 = _M0L7_2aSomeS693;
      struct _M0TPB5EntryGsiE* _M0L15next__in__chainS695 = _M0L4_2aeS694->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2867 = 0;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4247 = _M0L4_2aeS694->$1;
      if (_M0L15next__in__chainS695) {
        moonbit_incref(_M0L15next__in__chainS695);
      }
      if (_M0L6_2aoldS4247) {
        moonbit_decref(_M0L6_2aoldS4247);
      }
      _M0L4_2aeS694->$1 = _M0L6_2atmpS2867;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsiE(_M0L4selfS689, _M0L4_2aeS694);
      moonbit_decref(_M0L4_2aeS694);
      _M0L1xS691 = _M0L15next__in__chainS695;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS616,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5outerS612
) {
  int32_t _M0L4hashS611;
  int32_t _M0L14capacity__maskS2788;
  int32_t _M0L6_2atmpS2787;
  int32_t _M0L3pslS613;
  int32_t _M0L3idxS614;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS611 = _M0L5outerS612->$3;
  _M0L14capacity__maskS2788 = _M0L4selfS616->$3;
  _M0L6_2atmpS2787 = _M0L4hashS611 & _M0L14capacity__maskS2788;
  _M0L3pslS613 = 0;
  _M0L3idxS614 = _M0L6_2atmpS2787;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2786 =
      _M0L4selfS616->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS615;
    if (
      _M0L3idxS614 < 0
      || _M0L3idxS614 >= Moonbit_array_length(_M0L7entriesS2786)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS615
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2786[
        _M0L3idxS614
      ];
    if (_M0L7_2abindS615 == 0) {
      int32_t _M0L4tailS2779;
      _M0L5outerS612->$2 = _M0L3pslS613;
      _M0L4tailS2779 = _M0L4selfS616->$6;
      _M0L5outerS612->$0 = _M0L4tailS2779;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(_M0L4selfS616, _M0L3idxS614, _M0L5outerS612);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS617 =
        _M0L7_2abindS615;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2acurrS618 =
        _M0L7_2aSomeS617;
      int32_t _M0L3pslS2780 = _M0L7_2acurrS618->$2;
      if (_M0L3pslS613 > _M0L3pslS2780) {
        int32_t _M0L4tailS2781;
        moonbit_incref(_M0L7_2acurrS618);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP19moonbitDB10RedisValueE(_M0L4selfS616, _M0L3idxS614, _M0L7_2acurrS618);
        moonbit_decref(_M0L7_2acurrS618);
        _M0L5outerS612->$2 = _M0L3pslS613;
        _M0L4tailS2781 = _M0L4selfS616->$6;
        _M0L5outerS612->$0 = _M0L4tailS2781;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(_M0L4selfS616, _M0L3idxS614, _M0L5outerS612);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2782 = _M0L3pslS613 + 1;
        int32_t _M0L6_2atmpS2784 = _M0L3idxS614 + 1;
        int32_t _M0L14capacity__maskS2785 = _M0L4selfS616->$3;
        int32_t _M0L6_2atmpS2783 =
          _M0L6_2atmpS2784 & _M0L14capacity__maskS2785;
        _M0L3pslS613 = _M0L6_2atmpS2782;
        _M0L3idxS614 = _M0L6_2atmpS2783;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS625,
  struct _M0TPB5EntryGssE* _M0L5outerS621
) {
  int32_t _M0L4hashS620;
  int32_t _M0L14capacity__maskS2798;
  int32_t _M0L6_2atmpS2797;
  int32_t _M0L3pslS622;
  int32_t _M0L3idxS623;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS620 = _M0L5outerS621->$3;
  _M0L14capacity__maskS2798 = _M0L4selfS625->$3;
  _M0L6_2atmpS2797 = _M0L4hashS620 & _M0L14capacity__maskS2798;
  _M0L3pslS622 = 0;
  _M0L3idxS623 = _M0L6_2atmpS2797;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2796 = _M0L4selfS625->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS624;
    if (
      _M0L3idxS623 < 0
      || _M0L3idxS623 >= Moonbit_array_length(_M0L7entriesS2796)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS624
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2796[_M0L3idxS623];
    if (_M0L7_2abindS624 == 0) {
      int32_t _M0L4tailS2789;
      _M0L5outerS621->$2 = _M0L3pslS622;
      _M0L4tailS2789 = _M0L4selfS625->$6;
      _M0L5outerS621->$0 = _M0L4tailS2789;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS625, _M0L3idxS623, _M0L5outerS621);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS626 = _M0L7_2abindS624;
      struct _M0TPB5EntryGssE* _M0L7_2acurrS627 = _M0L7_2aSomeS626;
      int32_t _M0L3pslS2790 = _M0L7_2acurrS627->$2;
      if (_M0L3pslS622 > _M0L3pslS2790) {
        int32_t _M0L4tailS2791;
        moonbit_incref(_M0L7_2acurrS627);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS625, _M0L3idxS623, _M0L7_2acurrS627);
        moonbit_decref(_M0L7_2acurrS627);
        _M0L5outerS621->$2 = _M0L3pslS622;
        _M0L4tailS2791 = _M0L4selfS625->$6;
        _M0L5outerS621->$0 = _M0L4tailS2791;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS625, _M0L3idxS623, _M0L5outerS621);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2792 = _M0L3pslS622 + 1;
        int32_t _M0L6_2atmpS2794 = _M0L3idxS623 + 1;
        int32_t _M0L14capacity__maskS2795 = _M0L4selfS625->$3;
        int32_t _M0L6_2atmpS2793 =
          _M0L6_2atmpS2794 & _M0L14capacity__maskS2795;
        _M0L3pslS622 = _M0L6_2atmpS2792;
        _M0L3idxS623 = _M0L6_2atmpS2793;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS634,
  struct _M0TPB5EntryGsbE* _M0L5outerS630
) {
  int32_t _M0L4hashS629;
  int32_t _M0L14capacity__maskS2808;
  int32_t _M0L6_2atmpS2807;
  int32_t _M0L3pslS631;
  int32_t _M0L3idxS632;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS629 = _M0L5outerS630->$3;
  _M0L14capacity__maskS2808 = _M0L4selfS634->$3;
  _M0L6_2atmpS2807 = _M0L4hashS629 & _M0L14capacity__maskS2808;
  _M0L3pslS631 = 0;
  _M0L3idxS632 = _M0L6_2atmpS2807;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2806 = _M0L4selfS634->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS633;
    if (
      _M0L3idxS632 < 0
      || _M0L3idxS632 >= Moonbit_array_length(_M0L7entriesS2806)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS633
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2806[_M0L3idxS632];
    if (_M0L7_2abindS633 == 0) {
      int32_t _M0L4tailS2799;
      _M0L5outerS630->$2 = _M0L3pslS631;
      _M0L4tailS2799 = _M0L4selfS634->$6;
      _M0L5outerS630->$0 = _M0L4tailS2799;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS634, _M0L3idxS632, _M0L5outerS630);
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS635 = _M0L7_2abindS633;
      struct _M0TPB5EntryGsbE* _M0L7_2acurrS636 = _M0L7_2aSomeS635;
      int32_t _M0L3pslS2800 = _M0L7_2acurrS636->$2;
      if (_M0L3pslS631 > _M0L3pslS2800) {
        int32_t _M0L4tailS2801;
        moonbit_incref(_M0L7_2acurrS636);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsbE(_M0L4selfS634, _M0L3idxS632, _M0L7_2acurrS636);
        moonbit_decref(_M0L7_2acurrS636);
        _M0L5outerS630->$2 = _M0L3pslS631;
        _M0L4tailS2801 = _M0L4selfS634->$6;
        _M0L5outerS630->$0 = _M0L4tailS2801;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS634, _M0L3idxS632, _M0L5outerS630);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2802 = _M0L3pslS631 + 1;
        int32_t _M0L6_2atmpS2804 = _M0L3idxS632 + 1;
        int32_t _M0L14capacity__maskS2805 = _M0L4selfS634->$3;
        int32_t _M0L6_2atmpS2803 =
          _M0L6_2atmpS2804 & _M0L14capacity__maskS2805;
        _M0L3pslS631 = _M0L6_2atmpS2802;
        _M0L3idxS632 = _M0L6_2atmpS2803;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS643,
  struct _M0TPB5EntryGsfE* _M0L5outerS639
) {
  int32_t _M0L4hashS638;
  int32_t _M0L14capacity__maskS2818;
  int32_t _M0L6_2atmpS2817;
  int32_t _M0L3pslS640;
  int32_t _M0L3idxS641;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS638 = _M0L5outerS639->$3;
  _M0L14capacity__maskS2818 = _M0L4selfS643->$3;
  _M0L6_2atmpS2817 = _M0L4hashS638 & _M0L14capacity__maskS2818;
  _M0L3pslS640 = 0;
  _M0L3idxS641 = _M0L6_2atmpS2817;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2816 = _M0L4selfS643->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS642;
    if (
      _M0L3idxS641 < 0
      || _M0L3idxS641 >= Moonbit_array_length(_M0L7entriesS2816)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS642
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2816[_M0L3idxS641];
    if (_M0L7_2abindS642 == 0) {
      int32_t _M0L4tailS2809;
      _M0L5outerS639->$2 = _M0L3pslS640;
      _M0L4tailS2809 = _M0L4selfS643->$6;
      _M0L5outerS639->$0 = _M0L4tailS2809;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS643, _M0L3idxS641, _M0L5outerS639);
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS644 = _M0L7_2abindS642;
      struct _M0TPB5EntryGsfE* _M0L7_2acurrS645 = _M0L7_2aSomeS644;
      int32_t _M0L3pslS2810 = _M0L7_2acurrS645->$2;
      if (_M0L3pslS640 > _M0L3pslS2810) {
        int32_t _M0L4tailS2811;
        moonbit_incref(_M0L7_2acurrS645);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsfE(_M0L4selfS643, _M0L3idxS641, _M0L7_2acurrS645);
        moonbit_decref(_M0L7_2acurrS645);
        _M0L5outerS639->$2 = _M0L3pslS640;
        _M0L4tailS2811 = _M0L4selfS643->$6;
        _M0L5outerS639->$0 = _M0L4tailS2811;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS643, _M0L3idxS641, _M0L5outerS639);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2812 = _M0L3pslS640 + 1;
        int32_t _M0L6_2atmpS2814 = _M0L3idxS641 + 1;
        int32_t _M0L14capacity__maskS2815 = _M0L4selfS643->$3;
        int32_t _M0L6_2atmpS2813 =
          _M0L6_2atmpS2814 & _M0L14capacity__maskS2815;
        _M0L3pslS640 = _M0L6_2atmpS2812;
        _M0L3idxS641 = _M0L6_2atmpS2813;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map20rehash__place__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS652,
  struct _M0TPB5EntryGsiE* _M0L5outerS648
) {
  int32_t _M0L4hashS647;
  int32_t _M0L14capacity__maskS2828;
  int32_t _M0L6_2atmpS2827;
  int32_t _M0L3pslS649;
  int32_t _M0L3idxS650;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS647 = _M0L5outerS648->$3;
  _M0L14capacity__maskS2828 = _M0L4selfS652->$3;
  _M0L6_2atmpS2827 = _M0L4hashS647 & _M0L14capacity__maskS2828;
  _M0L3pslS649 = 0;
  _M0L3idxS650 = _M0L6_2atmpS2827;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2826 = _M0L4selfS652->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS651;
    if (
      _M0L3idxS650 < 0
      || _M0L3idxS650 >= Moonbit_array_length(_M0L7entriesS2826)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS651
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2826[_M0L3idxS650];
    if (_M0L7_2abindS651 == 0) {
      int32_t _M0L4tailS2819;
      _M0L5outerS648->$2 = _M0L3pslS649;
      _M0L4tailS2819 = _M0L4selfS652->$6;
      _M0L5outerS648->$0 = _M0L4tailS2819;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS652, _M0L3idxS650, _M0L5outerS648);
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS653 = _M0L7_2abindS651;
      struct _M0TPB5EntryGsiE* _M0L7_2acurrS654 = _M0L7_2aSomeS653;
      int32_t _M0L3pslS2820 = _M0L7_2acurrS654->$2;
      if (_M0L3pslS649 > _M0L3pslS2820) {
        int32_t _M0L4tailS2821;
        moonbit_incref(_M0L7_2acurrS654);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsiE(_M0L4selfS652, _M0L3idxS650, _M0L7_2acurrS654);
        moonbit_decref(_M0L7_2acurrS654);
        _M0L5outerS648->$2 = _M0L3pslS649;
        _M0L4tailS2821 = _M0L4selfS652->$6;
        _M0L5outerS648->$0 = _M0L4tailS2821;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS652, _M0L3idxS650, _M0L5outerS648);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2822 = _M0L3pslS649 + 1;
        int32_t _M0L6_2atmpS2824 = _M0L3idxS650 + 1;
        int32_t _M0L14capacity__maskS2825 = _M0L4selfS652->$3;
        int32_t _M0L6_2atmpS2823 =
          _M0L6_2atmpS2824 & _M0L14capacity__maskS2825;
        _M0L3pslS649 = _M0L6_2atmpS2822;
        _M0L3idxS650 = _M0L6_2atmpS2823;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS565,
  int32_t _M0L3idxS570,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS569
) {
  int32_t _M0L3pslS2714;
  int32_t _M0L6_2atmpS2710;
  int32_t _M0L6_2atmpS2712;
  int32_t _M0L14capacity__maskS2713;
  int32_t _M0L6_2atmpS2711;
  int32_t _M0L3pslS561;
  int32_t _M0L3idxS562;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS563;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2714 = _M0L5entryS569->$2;
  _M0L6_2atmpS2710 = _M0L3pslS2714 + 1;
  _M0L6_2atmpS2712 = _M0L3idxS570 + 1;
  _M0L14capacity__maskS2713 = _M0L4selfS565->$3;
  _M0L6_2atmpS2711 = _M0L6_2atmpS2712 & _M0L14capacity__maskS2713;
  moonbit_incref(_M0L5entryS569);
  _M0L3pslS561 = _M0L6_2atmpS2710;
  _M0L3idxS562 = _M0L6_2atmpS2711;
  _M0L5entryS563 = _M0L5entryS569;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2709 =
      _M0L4selfS565->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS564;
    if (
      _M0L3idxS562 < 0
      || _M0L3idxS562 >= Moonbit_array_length(_M0L7entriesS2709)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS564
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2709[
        _M0L3idxS562
      ];
    if (_M0L7_2abindS564 == 0) {
      _M0L5entryS563->$2 = _M0L3pslS561;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS565, _M0L5entryS563, _M0L3idxS562);
      moonbit_decref(_M0L5entryS563);
      break;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS567 =
        _M0L7_2abindS564;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L14_2acurr__entryS568 =
        _M0L7_2aSomeS567;
      int32_t _M0L3pslS2699 = _M0L14_2acurr__entryS568->$2;
      if (_M0L3pslS561 > _M0L3pslS2699) {
        int32_t _M0L3pslS2704;
        int32_t _M0L6_2atmpS2700;
        int32_t _M0L6_2atmpS2702;
        int32_t _M0L14capacity__maskS2703;
        int32_t _M0L6_2atmpS2701;
        _M0L5entryS563->$2 = _M0L3pslS561;
        moonbit_incref(_M0L14_2acurr__entryS568);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS565, _M0L5entryS563, _M0L3idxS562);
        moonbit_decref(_M0L5entryS563);
        _M0L3pslS2704 = _M0L14_2acurr__entryS568->$2;
        _M0L6_2atmpS2700 = _M0L3pslS2704 + 1;
        _M0L6_2atmpS2702 = _M0L3idxS562 + 1;
        _M0L14capacity__maskS2703 = _M0L4selfS565->$3;
        _M0L6_2atmpS2701 = _M0L6_2atmpS2702 & _M0L14capacity__maskS2703;
        _M0L3pslS561 = _M0L6_2atmpS2700;
        _M0L3idxS562 = _M0L6_2atmpS2701;
        _M0L5entryS563 = _M0L14_2acurr__entryS568;
        continue;
      } else {
        int32_t _M0L6_2atmpS2705 = _M0L3pslS561 + 1;
        int32_t _M0L6_2atmpS2707 = _M0L3idxS562 + 1;
        int32_t _M0L14capacity__maskS2708 = _M0L4selfS565->$3;
        int32_t _M0L6_2atmpS2706 =
          _M0L6_2atmpS2707 & _M0L14capacity__maskS2708;
        struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _tmp_4737 =
          _M0L5entryS563;
        _M0L3pslS561 = _M0L6_2atmpS2705;
        _M0L3idxS562 = _M0L6_2atmpS2706;
        _M0L5entryS563 = _tmp_4737;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGssE(
  struct _M0TPB3MapGssE* _M0L4selfS575,
  int32_t _M0L3idxS580,
  struct _M0TPB5EntryGssE* _M0L5entryS579
) {
  int32_t _M0L3pslS2730;
  int32_t _M0L6_2atmpS2726;
  int32_t _M0L6_2atmpS2728;
  int32_t _M0L14capacity__maskS2729;
  int32_t _M0L6_2atmpS2727;
  int32_t _M0L3pslS571;
  int32_t _M0L3idxS572;
  struct _M0TPB5EntryGssE* _M0L5entryS573;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2730 = _M0L5entryS579->$2;
  _M0L6_2atmpS2726 = _M0L3pslS2730 + 1;
  _M0L6_2atmpS2728 = _M0L3idxS580 + 1;
  _M0L14capacity__maskS2729 = _M0L4selfS575->$3;
  _M0L6_2atmpS2727 = _M0L6_2atmpS2728 & _M0L14capacity__maskS2729;
  moonbit_incref(_M0L5entryS579);
  _M0L3pslS571 = _M0L6_2atmpS2726;
  _M0L3idxS572 = _M0L6_2atmpS2727;
  _M0L5entryS573 = _M0L5entryS579;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2725 = _M0L4selfS575->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS574;
    if (
      _M0L3idxS572 < 0
      || _M0L3idxS572 >= Moonbit_array_length(_M0L7entriesS2725)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS574
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2725[_M0L3idxS572];
    if (_M0L7_2abindS574 == 0) {
      _M0L5entryS573->$2 = _M0L3pslS571;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGssE(_M0L4selfS575, _M0L5entryS573, _M0L3idxS572);
      moonbit_decref(_M0L5entryS573);
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS577 = _M0L7_2abindS574;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS578 = _M0L7_2aSomeS577;
      int32_t _M0L3pslS2715 = _M0L14_2acurr__entryS578->$2;
      if (_M0L3pslS571 > _M0L3pslS2715) {
        int32_t _M0L3pslS2720;
        int32_t _M0L6_2atmpS2716;
        int32_t _M0L6_2atmpS2718;
        int32_t _M0L14capacity__maskS2719;
        int32_t _M0L6_2atmpS2717;
        _M0L5entryS573->$2 = _M0L3pslS571;
        moonbit_incref(_M0L14_2acurr__entryS578);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGssE(_M0L4selfS575, _M0L5entryS573, _M0L3idxS572);
        moonbit_decref(_M0L5entryS573);
        _M0L3pslS2720 = _M0L14_2acurr__entryS578->$2;
        _M0L6_2atmpS2716 = _M0L3pslS2720 + 1;
        _M0L6_2atmpS2718 = _M0L3idxS572 + 1;
        _M0L14capacity__maskS2719 = _M0L4selfS575->$3;
        _M0L6_2atmpS2717 = _M0L6_2atmpS2718 & _M0L14capacity__maskS2719;
        _M0L3pslS571 = _M0L6_2atmpS2716;
        _M0L3idxS572 = _M0L6_2atmpS2717;
        _M0L5entryS573 = _M0L14_2acurr__entryS578;
        continue;
      } else {
        int32_t _M0L6_2atmpS2721 = _M0L3pslS571 + 1;
        int32_t _M0L6_2atmpS2723 = _M0L3idxS572 + 1;
        int32_t _M0L14capacity__maskS2724 = _M0L4selfS575->$3;
        int32_t _M0L6_2atmpS2722 =
          _M0L6_2atmpS2723 & _M0L14capacity__maskS2724;
        struct _M0TPB5EntryGssE* _tmp_4739 = _M0L5entryS573;
        _M0L3pslS571 = _M0L6_2atmpS2721;
        _M0L3idxS572 = _M0L6_2atmpS2722;
        _M0L5entryS573 = _tmp_4739;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS585,
  int32_t _M0L3idxS590,
  struct _M0TPB5EntryGsbE* _M0L5entryS589
) {
  int32_t _M0L3pslS2746;
  int32_t _M0L6_2atmpS2742;
  int32_t _M0L6_2atmpS2744;
  int32_t _M0L14capacity__maskS2745;
  int32_t _M0L6_2atmpS2743;
  int32_t _M0L3pslS581;
  int32_t _M0L3idxS582;
  struct _M0TPB5EntryGsbE* _M0L5entryS583;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2746 = _M0L5entryS589->$2;
  _M0L6_2atmpS2742 = _M0L3pslS2746 + 1;
  _M0L6_2atmpS2744 = _M0L3idxS590 + 1;
  _M0L14capacity__maskS2745 = _M0L4selfS585->$3;
  _M0L6_2atmpS2743 = _M0L6_2atmpS2744 & _M0L14capacity__maskS2745;
  moonbit_incref(_M0L5entryS589);
  _M0L3pslS581 = _M0L6_2atmpS2742;
  _M0L3idxS582 = _M0L6_2atmpS2743;
  _M0L5entryS583 = _M0L5entryS589;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2741 = _M0L4selfS585->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS584;
    if (
      _M0L3idxS582 < 0
      || _M0L3idxS582 >= Moonbit_array_length(_M0L7entriesS2741)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS584
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2741[_M0L3idxS582];
    if (_M0L7_2abindS584 == 0) {
      _M0L5entryS583->$2 = _M0L3pslS581;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsbE(_M0L4selfS585, _M0L5entryS583, _M0L3idxS582);
      moonbit_decref(_M0L5entryS583);
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS587 = _M0L7_2abindS584;
      struct _M0TPB5EntryGsbE* _M0L14_2acurr__entryS588 = _M0L7_2aSomeS587;
      int32_t _M0L3pslS2731 = _M0L14_2acurr__entryS588->$2;
      if (_M0L3pslS581 > _M0L3pslS2731) {
        int32_t _M0L3pslS2736;
        int32_t _M0L6_2atmpS2732;
        int32_t _M0L6_2atmpS2734;
        int32_t _M0L14capacity__maskS2735;
        int32_t _M0L6_2atmpS2733;
        _M0L5entryS583->$2 = _M0L3pslS581;
        moonbit_incref(_M0L14_2acurr__entryS588);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsbE(_M0L4selfS585, _M0L5entryS583, _M0L3idxS582);
        moonbit_decref(_M0L5entryS583);
        _M0L3pslS2736 = _M0L14_2acurr__entryS588->$2;
        _M0L6_2atmpS2732 = _M0L3pslS2736 + 1;
        _M0L6_2atmpS2734 = _M0L3idxS582 + 1;
        _M0L14capacity__maskS2735 = _M0L4selfS585->$3;
        _M0L6_2atmpS2733 = _M0L6_2atmpS2734 & _M0L14capacity__maskS2735;
        _M0L3pslS581 = _M0L6_2atmpS2732;
        _M0L3idxS582 = _M0L6_2atmpS2733;
        _M0L5entryS583 = _M0L14_2acurr__entryS588;
        continue;
      } else {
        int32_t _M0L6_2atmpS2737 = _M0L3pslS581 + 1;
        int32_t _M0L6_2atmpS2739 = _M0L3idxS582 + 1;
        int32_t _M0L14capacity__maskS2740 = _M0L4selfS585->$3;
        int32_t _M0L6_2atmpS2738 =
          _M0L6_2atmpS2739 & _M0L14capacity__maskS2740;
        struct _M0TPB5EntryGsbE* _tmp_4741 = _M0L5entryS583;
        _M0L3pslS581 = _M0L6_2atmpS2737;
        _M0L3idxS582 = _M0L6_2atmpS2738;
        _M0L5entryS583 = _tmp_4741;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS595,
  int32_t _M0L3idxS600,
  struct _M0TPB5EntryGsfE* _M0L5entryS599
) {
  int32_t _M0L3pslS2762;
  int32_t _M0L6_2atmpS2758;
  int32_t _M0L6_2atmpS2760;
  int32_t _M0L14capacity__maskS2761;
  int32_t _M0L6_2atmpS2759;
  int32_t _M0L3pslS591;
  int32_t _M0L3idxS592;
  struct _M0TPB5EntryGsfE* _M0L5entryS593;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2762 = _M0L5entryS599->$2;
  _M0L6_2atmpS2758 = _M0L3pslS2762 + 1;
  _M0L6_2atmpS2760 = _M0L3idxS600 + 1;
  _M0L14capacity__maskS2761 = _M0L4selfS595->$3;
  _M0L6_2atmpS2759 = _M0L6_2atmpS2760 & _M0L14capacity__maskS2761;
  moonbit_incref(_M0L5entryS599);
  _M0L3pslS591 = _M0L6_2atmpS2758;
  _M0L3idxS592 = _M0L6_2atmpS2759;
  _M0L5entryS593 = _M0L5entryS599;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2757 = _M0L4selfS595->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS594;
    if (
      _M0L3idxS592 < 0
      || _M0L3idxS592 >= Moonbit_array_length(_M0L7entriesS2757)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS594
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2757[_M0L3idxS592];
    if (_M0L7_2abindS594 == 0) {
      _M0L5entryS593->$2 = _M0L3pslS591;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsfE(_M0L4selfS595, _M0L5entryS593, _M0L3idxS592);
      moonbit_decref(_M0L5entryS593);
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS597 = _M0L7_2abindS594;
      struct _M0TPB5EntryGsfE* _M0L14_2acurr__entryS598 = _M0L7_2aSomeS597;
      int32_t _M0L3pslS2747 = _M0L14_2acurr__entryS598->$2;
      if (_M0L3pslS591 > _M0L3pslS2747) {
        int32_t _M0L3pslS2752;
        int32_t _M0L6_2atmpS2748;
        int32_t _M0L6_2atmpS2750;
        int32_t _M0L14capacity__maskS2751;
        int32_t _M0L6_2atmpS2749;
        _M0L5entryS593->$2 = _M0L3pslS591;
        moonbit_incref(_M0L14_2acurr__entryS598);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsfE(_M0L4selfS595, _M0L5entryS593, _M0L3idxS592);
        moonbit_decref(_M0L5entryS593);
        _M0L3pslS2752 = _M0L14_2acurr__entryS598->$2;
        _M0L6_2atmpS2748 = _M0L3pslS2752 + 1;
        _M0L6_2atmpS2750 = _M0L3idxS592 + 1;
        _M0L14capacity__maskS2751 = _M0L4selfS595->$3;
        _M0L6_2atmpS2749 = _M0L6_2atmpS2750 & _M0L14capacity__maskS2751;
        _M0L3pslS591 = _M0L6_2atmpS2748;
        _M0L3idxS592 = _M0L6_2atmpS2749;
        _M0L5entryS593 = _M0L14_2acurr__entryS598;
        continue;
      } else {
        int32_t _M0L6_2atmpS2753 = _M0L3pslS591 + 1;
        int32_t _M0L6_2atmpS2755 = _M0L3idxS592 + 1;
        int32_t _M0L14capacity__maskS2756 = _M0L4selfS595->$3;
        int32_t _M0L6_2atmpS2754 =
          _M0L6_2atmpS2755 & _M0L14capacity__maskS2756;
        struct _M0TPB5EntryGsfE* _tmp_4743 = _M0L5entryS593;
        _M0L3pslS591 = _M0L6_2atmpS2753;
        _M0L3idxS592 = _M0L6_2atmpS2754;
        _M0L5entryS593 = _tmp_4743;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS605,
  int32_t _M0L3idxS610,
  struct _M0TPB5EntryGsiE* _M0L5entryS609
) {
  int32_t _M0L3pslS2778;
  int32_t _M0L6_2atmpS2774;
  int32_t _M0L6_2atmpS2776;
  int32_t _M0L14capacity__maskS2777;
  int32_t _M0L6_2atmpS2775;
  int32_t _M0L3pslS601;
  int32_t _M0L3idxS602;
  struct _M0TPB5EntryGsiE* _M0L5entryS603;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2778 = _M0L5entryS609->$2;
  _M0L6_2atmpS2774 = _M0L3pslS2778 + 1;
  _M0L6_2atmpS2776 = _M0L3idxS610 + 1;
  _M0L14capacity__maskS2777 = _M0L4selfS605->$3;
  _M0L6_2atmpS2775 = _M0L6_2atmpS2776 & _M0L14capacity__maskS2777;
  moonbit_incref(_M0L5entryS609);
  _M0L3pslS601 = _M0L6_2atmpS2774;
  _M0L3idxS602 = _M0L6_2atmpS2775;
  _M0L5entryS603 = _M0L5entryS609;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2773 = _M0L4selfS605->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS604;
    if (
      _M0L3idxS602 < 0
      || _M0L3idxS602 >= Moonbit_array_length(_M0L7entriesS2773)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS604
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2773[_M0L3idxS602];
    if (_M0L7_2abindS604 == 0) {
      _M0L5entryS603->$2 = _M0L3pslS601;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsiE(_M0L4selfS605, _M0L5entryS603, _M0L3idxS602);
      moonbit_decref(_M0L5entryS603);
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS607 = _M0L7_2abindS604;
      struct _M0TPB5EntryGsiE* _M0L14_2acurr__entryS608 = _M0L7_2aSomeS607;
      int32_t _M0L3pslS2763 = _M0L14_2acurr__entryS608->$2;
      if (_M0L3pslS601 > _M0L3pslS2763) {
        int32_t _M0L3pslS2768;
        int32_t _M0L6_2atmpS2764;
        int32_t _M0L6_2atmpS2766;
        int32_t _M0L14capacity__maskS2767;
        int32_t _M0L6_2atmpS2765;
        _M0L5entryS603->$2 = _M0L3pslS601;
        moonbit_incref(_M0L14_2acurr__entryS608);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsiE(_M0L4selfS605, _M0L5entryS603, _M0L3idxS602);
        moonbit_decref(_M0L5entryS603);
        _M0L3pslS2768 = _M0L14_2acurr__entryS608->$2;
        _M0L6_2atmpS2764 = _M0L3pslS2768 + 1;
        _M0L6_2atmpS2766 = _M0L3idxS602 + 1;
        _M0L14capacity__maskS2767 = _M0L4selfS605->$3;
        _M0L6_2atmpS2765 = _M0L6_2atmpS2766 & _M0L14capacity__maskS2767;
        _M0L3pslS601 = _M0L6_2atmpS2764;
        _M0L3idxS602 = _M0L6_2atmpS2765;
        _M0L5entryS603 = _M0L14_2acurr__entryS608;
        continue;
      } else {
        int32_t _M0L6_2atmpS2769 = _M0L3pslS601 + 1;
        int32_t _M0L6_2atmpS2771 = _M0L3idxS602 + 1;
        int32_t _M0L14capacity__maskS2772 = _M0L4selfS605->$3;
        int32_t _M0L6_2atmpS2770 =
          _M0L6_2atmpS2771 & _M0L14capacity__maskS2772;
        struct _M0TPB5EntryGsiE* _tmp_4745 = _M0L5entryS603;
        _M0L3pslS601 = _M0L6_2atmpS2769;
        _M0L3idxS602 = _M0L6_2atmpS2770;
        _M0L5entryS603 = _tmp_4745;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS531,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS533,
  int32_t _M0L8new__idxS532
) {
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2689;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2690;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4273;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS534;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2689 = _M0L4selfS531->$0;
  _M0L6_2atmpS2690 = _M0L5entryS533;
  if (
    _M0L8new__idxS532 < 0
    || _M0L8new__idxS532 >= Moonbit_array_length(_M0L7entriesS2689)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4273
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2689[
      _M0L8new__idxS532
    ];
  if (_M0L6_2atmpS2690) {
    moonbit_incref(_M0L6_2atmpS2690);
  }
  if (_M0L6_2aoldS4273) {
    moonbit_decref(_M0L6_2aoldS4273);
  }
  _M0L7entriesS2689[_M0L8new__idxS532] = _M0L6_2atmpS2690;
  _M0L7_2abindS534 = _M0L5entryS533->$1;
  if (_M0L7_2abindS534 == 0) {
    _M0L4selfS531->$6 = _M0L8new__idxS532;
  } else {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS535 =
      _M0L7_2abindS534;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2anextS536 =
      _M0L7_2aSomeS535;
    _M0L7_2anextS536->$0 = _M0L8new__idxS532;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS537,
  struct _M0TPB5EntryGsiE* _M0L5entryS539,
  int32_t _M0L8new__idxS538
) {
  struct _M0TPB5EntryGsiE** _M0L7entriesS2691;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2692;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS4276;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS540;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2691 = _M0L4selfS537->$0;
  _M0L6_2atmpS2692 = _M0L5entryS539;
  if (
    _M0L8new__idxS538 < 0
    || _M0L8new__idxS538 >= Moonbit_array_length(_M0L7entriesS2691)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4276
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2691[_M0L8new__idxS538];
  if (_M0L6_2atmpS2692) {
    moonbit_incref(_M0L6_2atmpS2692);
  }
  if (_M0L6_2aoldS4276) {
    moonbit_decref(_M0L6_2aoldS4276);
  }
  _M0L7entriesS2691[_M0L8new__idxS538] = _M0L6_2atmpS2692;
  _M0L7_2abindS540 = _M0L5entryS539->$1;
  if (_M0L7_2abindS540 == 0) {
    _M0L4selfS537->$6 = _M0L8new__idxS538;
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS541 = _M0L7_2abindS540;
    struct _M0TPB5EntryGsiE* _M0L7_2anextS542 = _M0L7_2aSomeS541;
    _M0L7_2anextS542->$0 = _M0L8new__idxS538;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS543,
  struct _M0TPB5EntryGssE* _M0L5entryS545,
  int32_t _M0L8new__idxS544
) {
  struct _M0TPB5EntryGssE** _M0L7entriesS2693;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2694;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS4279;
  struct _M0TPB5EntryGssE* _M0L7_2abindS546;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2693 = _M0L4selfS543->$0;
  _M0L6_2atmpS2694 = _M0L5entryS545;
  if (
    _M0L8new__idxS544 < 0
    || _M0L8new__idxS544 >= Moonbit_array_length(_M0L7entriesS2693)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4279
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2693[_M0L8new__idxS544];
  if (_M0L6_2atmpS2694) {
    moonbit_incref(_M0L6_2atmpS2694);
  }
  if (_M0L6_2aoldS4279) {
    moonbit_decref(_M0L6_2aoldS4279);
  }
  _M0L7entriesS2693[_M0L8new__idxS544] = _M0L6_2atmpS2694;
  _M0L7_2abindS546 = _M0L5entryS545->$1;
  if (_M0L7_2abindS546 == 0) {
    _M0L4selfS543->$6 = _M0L8new__idxS544;
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS547 = _M0L7_2abindS546;
    struct _M0TPB5EntryGssE* _M0L7_2anextS548 = _M0L7_2aSomeS547;
    _M0L7_2anextS548->$0 = _M0L8new__idxS544;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS549,
  struct _M0TPB5EntryGsbE* _M0L5entryS551,
  int32_t _M0L8new__idxS550
) {
  struct _M0TPB5EntryGsbE** _M0L7entriesS2695;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2696;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS4282;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS552;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2695 = _M0L4selfS549->$0;
  _M0L6_2atmpS2696 = _M0L5entryS551;
  if (
    _M0L8new__idxS550 < 0
    || _M0L8new__idxS550 >= Moonbit_array_length(_M0L7entriesS2695)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4282
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2695[_M0L8new__idxS550];
  if (_M0L6_2atmpS2696) {
    moonbit_incref(_M0L6_2atmpS2696);
  }
  if (_M0L6_2aoldS4282) {
    moonbit_decref(_M0L6_2aoldS4282);
  }
  _M0L7entriesS2695[_M0L8new__idxS550] = _M0L6_2atmpS2696;
  _M0L7_2abindS552 = _M0L5entryS551->$1;
  if (_M0L7_2abindS552 == 0) {
    _M0L4selfS549->$6 = _M0L8new__idxS550;
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS553 = _M0L7_2abindS552;
    struct _M0TPB5EntryGsbE* _M0L7_2anextS554 = _M0L7_2aSomeS553;
    _M0L7_2anextS554->$0 = _M0L8new__idxS550;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS555,
  struct _M0TPB5EntryGsfE* _M0L5entryS557,
  int32_t _M0L8new__idxS556
) {
  struct _M0TPB5EntryGsfE** _M0L7entriesS2697;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2698;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS4285;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS558;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2697 = _M0L4selfS555->$0;
  _M0L6_2atmpS2698 = _M0L5entryS557;
  if (
    _M0L8new__idxS556 < 0
    || _M0L8new__idxS556 >= Moonbit_array_length(_M0L7entriesS2697)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4285
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2697[_M0L8new__idxS556];
  if (_M0L6_2atmpS2698) {
    moonbit_incref(_M0L6_2atmpS2698);
  }
  if (_M0L6_2aoldS4285) {
    moonbit_decref(_M0L6_2aoldS4285);
  }
  _M0L7entriesS2697[_M0L8new__idxS556] = _M0L6_2atmpS2698;
  _M0L7_2abindS558 = _M0L5entryS557->$1;
  if (_M0L7_2abindS558 == 0) {
    _M0L4selfS555->$6 = _M0L8new__idxS556;
  } else {
    struct _M0TPB5EntryGsfE* _M0L7_2aSomeS559 = _M0L7_2abindS558;
    struct _M0TPB5EntryGsfE* _M0L7_2anextS560 = _M0L7_2aSomeS559;
    _M0L7_2anextS560->$0 = _M0L8new__idxS556;
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS512,
  int32_t _M0L3idxS514,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS513
) {
  int32_t _M0L7_2abindS511;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2649;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2650;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4287;
  int32_t _M0L4sizeS2652;
  int32_t _M0L6_2atmpS2651;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS511 = _M0L4selfS512->$6;
  switch (_M0L7_2abindS511) {
    case -1: {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2644 =
        _M0L5entryS513;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4289 =
        _M0L4selfS512->$5;
      if (_M0L6_2atmpS2644) {
        moonbit_incref(_M0L6_2atmpS2644);
      }
      if (_M0L6_2aoldS4289) {
        moonbit_decref(_M0L6_2aoldS4289);
      }
      _M0L4selfS512->$5 = _M0L6_2atmpS2644;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2648 =
        _M0L4selfS512->$0;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2647;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2645;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2646;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4290;
      if (
        _M0L7_2abindS511 < 0
        || _M0L7_2abindS511 >= Moonbit_array_length(_M0L7entriesS2648)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2647
      = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2648[
          _M0L7_2abindS511
        ];
      if (_M0L6_2atmpS2647) {
        moonbit_incref(_M0L6_2atmpS2647);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2645
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP19moonbitDB10RedisValueEE(_M0L6_2atmpS2647);
      if (_M0L6_2atmpS2647) {
        moonbit_decref(_M0L6_2atmpS2647);
      }
      _M0L6_2atmpS2646 = _M0L5entryS513;
      _M0L6_2aoldS4290 = _M0L6_2atmpS2645->$1;
      if (_M0L6_2atmpS2646) {
        moonbit_incref(_M0L6_2atmpS2646);
      }
      if (_M0L6_2aoldS4290) {
        moonbit_decref(_M0L6_2aoldS4290);
      }
      _M0L6_2atmpS2645->$1 = _M0L6_2atmpS2646;
      moonbit_decref(_M0L6_2atmpS2645);
      break;
    }
  }
  _M0L4selfS512->$6 = _M0L3idxS514;
  _M0L7entriesS2649 = _M0L4selfS512->$0;
  _M0L6_2atmpS2650 = _M0L5entryS513;
  if (
    _M0L3idxS514 < 0
    || _M0L3idxS514 >= Moonbit_array_length(_M0L7entriesS2649)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4287
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2649[
      _M0L3idxS514
    ];
  if (_M0L6_2atmpS2650) {
    moonbit_incref(_M0L6_2atmpS2650);
  }
  if (_M0L6_2aoldS4287) {
    moonbit_decref(_M0L6_2aoldS4287);
  }
  _M0L7entriesS2649[_M0L3idxS514] = _M0L6_2atmpS2650;
  _M0L4sizeS2652 = _M0L4selfS512->$1;
  _M0L6_2atmpS2651 = _M0L4sizeS2652 + 1;
  _M0L4selfS512->$1 = _M0L6_2atmpS2651;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGssE(
  struct _M0TPB3MapGssE* _M0L4selfS516,
  int32_t _M0L3idxS518,
  struct _M0TPB5EntryGssE* _M0L5entryS517
) {
  int32_t _M0L7_2abindS515;
  struct _M0TPB5EntryGssE** _M0L7entriesS2658;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2659;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS4293;
  int32_t _M0L4sizeS2661;
  int32_t _M0L6_2atmpS2660;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS515 = _M0L4selfS516->$6;
  switch (_M0L7_2abindS515) {
    case -1: {
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2653 = _M0L5entryS517;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4295 = _M0L4selfS516->$5;
      if (_M0L6_2atmpS2653) {
        moonbit_incref(_M0L6_2atmpS2653);
      }
      if (_M0L6_2aoldS4295) {
        moonbit_decref(_M0L6_2aoldS4295);
      }
      _M0L4selfS516->$5 = _M0L6_2atmpS2653;
      break;
    }
    default: {
      struct _M0TPB5EntryGssE** _M0L7entriesS2657 = _M0L4selfS516->$0;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2656;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2654;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2655;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4296;
      if (
        _M0L7_2abindS515 < 0
        || _M0L7_2abindS515 >= Moonbit_array_length(_M0L7entriesS2657)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2656
      = (struct _M0TPB5EntryGssE*)_M0L7entriesS2657[_M0L7_2abindS515];
      if (_M0L6_2atmpS2656) {
        moonbit_incref(_M0L6_2atmpS2656);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2654
      = _M0MPC16option6Option6unwrapGRPB5EntryGssEE(_M0L6_2atmpS2656);
      if (_M0L6_2atmpS2656) {
        moonbit_decref(_M0L6_2atmpS2656);
      }
      _M0L6_2atmpS2655 = _M0L5entryS517;
      _M0L6_2aoldS4296 = _M0L6_2atmpS2654->$1;
      if (_M0L6_2atmpS2655) {
        moonbit_incref(_M0L6_2atmpS2655);
      }
      if (_M0L6_2aoldS4296) {
        moonbit_decref(_M0L6_2aoldS4296);
      }
      _M0L6_2atmpS2654->$1 = _M0L6_2atmpS2655;
      moonbit_decref(_M0L6_2atmpS2654);
      break;
    }
  }
  _M0L4selfS516->$6 = _M0L3idxS518;
  _M0L7entriesS2658 = _M0L4selfS516->$0;
  _M0L6_2atmpS2659 = _M0L5entryS517;
  if (
    _M0L3idxS518 < 0
    || _M0L3idxS518 >= Moonbit_array_length(_M0L7entriesS2658)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4293
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2658[_M0L3idxS518];
  if (_M0L6_2atmpS2659) {
    moonbit_incref(_M0L6_2atmpS2659);
  }
  if (_M0L6_2aoldS4293) {
    moonbit_decref(_M0L6_2aoldS4293);
  }
  _M0L7entriesS2658[_M0L3idxS518] = _M0L6_2atmpS2659;
  _M0L4sizeS2661 = _M0L4selfS516->$1;
  _M0L6_2atmpS2660 = _M0L4sizeS2661 + 1;
  _M0L4selfS516->$1 = _M0L6_2atmpS2660;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS520,
  int32_t _M0L3idxS522,
  struct _M0TPB5EntryGsbE* _M0L5entryS521
) {
  int32_t _M0L7_2abindS519;
  struct _M0TPB5EntryGsbE** _M0L7entriesS2667;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2668;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS4299;
  int32_t _M0L4sizeS2670;
  int32_t _M0L6_2atmpS2669;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS519 = _M0L4selfS520->$6;
  switch (_M0L7_2abindS519) {
    case -1: {
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2662 = _M0L5entryS521;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4301 = _M0L4selfS520->$5;
      if (_M0L6_2atmpS2662) {
        moonbit_incref(_M0L6_2atmpS2662);
      }
      if (_M0L6_2aoldS4301) {
        moonbit_decref(_M0L6_2aoldS4301);
      }
      _M0L4selfS520->$5 = _M0L6_2atmpS2662;
      break;
    }
    default: {
      struct _M0TPB5EntryGsbE** _M0L7entriesS2666 = _M0L4selfS520->$0;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2665;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2663;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2664;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4302;
      if (
        _M0L7_2abindS519 < 0
        || _M0L7_2abindS519 >= Moonbit_array_length(_M0L7entriesS2666)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2665
      = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2666[_M0L7_2abindS519];
      if (_M0L6_2atmpS2665) {
        moonbit_incref(_M0L6_2atmpS2665);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2663
      = _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(_M0L6_2atmpS2665);
      if (_M0L6_2atmpS2665) {
        moonbit_decref(_M0L6_2atmpS2665);
      }
      _M0L6_2atmpS2664 = _M0L5entryS521;
      _M0L6_2aoldS4302 = _M0L6_2atmpS2663->$1;
      if (_M0L6_2atmpS2664) {
        moonbit_incref(_M0L6_2atmpS2664);
      }
      if (_M0L6_2aoldS4302) {
        moonbit_decref(_M0L6_2aoldS4302);
      }
      _M0L6_2atmpS2663->$1 = _M0L6_2atmpS2664;
      moonbit_decref(_M0L6_2atmpS2663);
      break;
    }
  }
  _M0L4selfS520->$6 = _M0L3idxS522;
  _M0L7entriesS2667 = _M0L4selfS520->$0;
  _M0L6_2atmpS2668 = _M0L5entryS521;
  if (
    _M0L3idxS522 < 0
    || _M0L3idxS522 >= Moonbit_array_length(_M0L7entriesS2667)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4299
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2667[_M0L3idxS522];
  if (_M0L6_2atmpS2668) {
    moonbit_incref(_M0L6_2atmpS2668);
  }
  if (_M0L6_2aoldS4299) {
    moonbit_decref(_M0L6_2aoldS4299);
  }
  _M0L7entriesS2667[_M0L3idxS522] = _M0L6_2atmpS2668;
  _M0L4sizeS2670 = _M0L4selfS520->$1;
  _M0L6_2atmpS2669 = _M0L4sizeS2670 + 1;
  _M0L4selfS520->$1 = _M0L6_2atmpS2669;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS524,
  int32_t _M0L3idxS526,
  struct _M0TPB5EntryGsfE* _M0L5entryS525
) {
  int32_t _M0L7_2abindS523;
  struct _M0TPB5EntryGsfE** _M0L7entriesS2676;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2677;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS4305;
  int32_t _M0L4sizeS2679;
  int32_t _M0L6_2atmpS2678;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS523 = _M0L4selfS524->$6;
  switch (_M0L7_2abindS523) {
    case -1: {
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2671 = _M0L5entryS525;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4307 = _M0L4selfS524->$5;
      if (_M0L6_2atmpS2671) {
        moonbit_incref(_M0L6_2atmpS2671);
      }
      if (_M0L6_2aoldS4307) {
        moonbit_decref(_M0L6_2aoldS4307);
      }
      _M0L4selfS524->$5 = _M0L6_2atmpS2671;
      break;
    }
    default: {
      struct _M0TPB5EntryGsfE** _M0L7entriesS2675 = _M0L4selfS524->$0;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2674;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2672;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2673;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4308;
      if (
        _M0L7_2abindS523 < 0
        || _M0L7_2abindS523 >= Moonbit_array_length(_M0L7entriesS2675)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2674
      = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2675[_M0L7_2abindS523];
      if (_M0L6_2atmpS2674) {
        moonbit_incref(_M0L6_2atmpS2674);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2672
      = _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(_M0L6_2atmpS2674);
      if (_M0L6_2atmpS2674) {
        moonbit_decref(_M0L6_2atmpS2674);
      }
      _M0L6_2atmpS2673 = _M0L5entryS525;
      _M0L6_2aoldS4308 = _M0L6_2atmpS2672->$1;
      if (_M0L6_2atmpS2673) {
        moonbit_incref(_M0L6_2atmpS2673);
      }
      if (_M0L6_2aoldS4308) {
        moonbit_decref(_M0L6_2aoldS4308);
      }
      _M0L6_2atmpS2672->$1 = _M0L6_2atmpS2673;
      moonbit_decref(_M0L6_2atmpS2672);
      break;
    }
  }
  _M0L4selfS524->$6 = _M0L3idxS526;
  _M0L7entriesS2676 = _M0L4selfS524->$0;
  _M0L6_2atmpS2677 = _M0L5entryS525;
  if (
    _M0L3idxS526 < 0
    || _M0L3idxS526 >= Moonbit_array_length(_M0L7entriesS2676)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4305
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2676[_M0L3idxS526];
  if (_M0L6_2atmpS2677) {
    moonbit_incref(_M0L6_2atmpS2677);
  }
  if (_M0L6_2aoldS4305) {
    moonbit_decref(_M0L6_2aoldS4305);
  }
  _M0L7entriesS2676[_M0L3idxS526] = _M0L6_2atmpS2677;
  _M0L4sizeS2679 = _M0L4selfS524->$1;
  _M0L6_2atmpS2678 = _M0L4sizeS2679 + 1;
  _M0L4selfS524->$1 = _M0L6_2atmpS2678;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS528,
  int32_t _M0L3idxS530,
  struct _M0TPB5EntryGsiE* _M0L5entryS529
) {
  int32_t _M0L7_2abindS527;
  struct _M0TPB5EntryGsiE** _M0L7entriesS2685;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2686;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS4311;
  int32_t _M0L4sizeS2688;
  int32_t _M0L6_2atmpS2687;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS527 = _M0L4selfS528->$6;
  switch (_M0L7_2abindS527) {
    case -1: {
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2680 = _M0L5entryS529;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4313 = _M0L4selfS528->$5;
      if (_M0L6_2atmpS2680) {
        moonbit_incref(_M0L6_2atmpS2680);
      }
      if (_M0L6_2aoldS4313) {
        moonbit_decref(_M0L6_2aoldS4313);
      }
      _M0L4selfS528->$5 = _M0L6_2atmpS2680;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS2684 = _M0L4selfS528->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2683;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2681;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2682;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4314;
      if (
        _M0L7_2abindS527 < 0
        || _M0L7_2abindS527 >= Moonbit_array_length(_M0L7entriesS2684)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2683
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2684[_M0L7_2abindS527];
      if (_M0L6_2atmpS2683) {
        moonbit_incref(_M0L6_2atmpS2683);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2681
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS2683);
      if (_M0L6_2atmpS2683) {
        moonbit_decref(_M0L6_2atmpS2683);
      }
      _M0L6_2atmpS2682 = _M0L5entryS529;
      _M0L6_2aoldS4314 = _M0L6_2atmpS2681->$1;
      if (_M0L6_2atmpS2682) {
        moonbit_incref(_M0L6_2atmpS2682);
      }
      if (_M0L6_2aoldS4314) {
        moonbit_decref(_M0L6_2aoldS4314);
      }
      _M0L6_2atmpS2681->$1 = _M0L6_2atmpS2682;
      moonbit_decref(_M0L6_2atmpS2681);
      break;
    }
  }
  _M0L4selfS528->$6 = _M0L3idxS530;
  _M0L7entriesS2685 = _M0L4selfS528->$0;
  _M0L6_2atmpS2686 = _M0L5entryS529;
  if (
    _M0L3idxS530 < 0
    || _M0L3idxS530 >= Moonbit_array_length(_M0L7entriesS2685)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4311
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2685[_M0L3idxS530];
  if (_M0L6_2atmpS2686) {
    moonbit_incref(_M0L6_2atmpS2686);
  }
  if (_M0L6_2aoldS4311) {
    moonbit_decref(_M0L6_2aoldS4311);
  }
  _M0L7entriesS2685[_M0L3idxS530] = _M0L6_2atmpS2686;
  _M0L4sizeS2688 = _M0L4selfS528->$1;
  _M0L6_2atmpS2687 = _M0L4sizeS2688 + 1;
  _M0L4selfS528->$1 = _M0L6_2atmpS2687;
  return 0;
}

int32_t _M0MPC13int3Int3max(int32_t _M0L4selfS509, int32_t _M0L5otherS510) {
  #line 75 "/home/developer/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS509 > _M0L5otherS510) {
    return _M0L4selfS509;
  } else {
    return _M0L5otherS510;
  }
}

int32_t _M0FPB21capacity__for__length(int32_t _M0L6lengthS508) {
  int32_t _M0Lm8capacityS507;
  int32_t _M0L6_2atmpS2642;
  int32_t _M0L6_2atmpS2641;
  #line 71 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 72 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS507 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS508);
  _M0L6_2atmpS2642 = _M0Lm8capacityS507;
  #line 73 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2641 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2642);
  if (_M0L6lengthS508 > _M0L6_2atmpS2641) {
    int32_t _M0L6_2atmpS2643 = _M0Lm8capacityS507;
    _M0Lm8capacityS507 = _M0L6_2atmpS2643 * 2;
  }
  return _M0Lm8capacityS507;
}

struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0FPB8new__mapGsRP19moonbitDB10RedisValueE(
  int32_t _M0L8capacityS478
) {
  int32_t _M0L8capacityS477;
  int32_t _M0L7_2abindS479;
  int32_t _M0L7_2abindS480;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2636;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7_2abindS481;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS482;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _block_4746;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS477
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS478);
  _M0L7_2abindS479 = _M0L8capacityS477 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS480 = _M0FPB21calc__grow__threshold(_M0L8capacityS477);
  _M0L6_2atmpS2636 = 0;
  _M0L7_2abindS481
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE**)moonbit_make_ref_array(_M0L8capacityS477, _M0L6_2atmpS2636);
  _M0L7_2abindS482 = 0;
  _block_4746
  = (struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRP19moonbitDB10RedisValueE));
  Moonbit_object_header(_block_4746)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 100, 0);
  _block_4746->$0 = _M0L7_2abindS481;
  _block_4746->$1 = 0;
  _block_4746->$2 = _M0L8capacityS477;
  _block_4746->$3 = _M0L7_2abindS479;
  _block_4746->$4 = _M0L7_2abindS480;
  _block_4746->$5 = _M0L7_2abindS482;
  _block_4746->$6 = -1;
  return _block_4746;
}

struct _M0TPB3MapGsiE* _M0FPB8new__mapGsiE(int32_t _M0L8capacityS484) {
  int32_t _M0L8capacityS483;
  int32_t _M0L7_2abindS485;
  int32_t _M0L7_2abindS486;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2637;
  struct _M0TPB5EntryGsiE** _M0L7_2abindS487;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS488;
  struct _M0TPB3MapGsiE* _block_4747;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS483
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS484);
  _M0L7_2abindS485 = _M0L8capacityS483 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS486 = _M0FPB21calc__grow__threshold(_M0L8capacityS483);
  _M0L6_2atmpS2637 = 0;
  _M0L7_2abindS487
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L8capacityS483, _M0L6_2atmpS2637);
  _M0L7_2abindS488 = 0;
  _block_4747
  = (struct _M0TPB3MapGsiE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsiE));
  Moonbit_object_header(_block_4747)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 104, 0);
  _block_4747->$0 = _M0L7_2abindS487;
  _block_4747->$1 = 0;
  _block_4747->$2 = _M0L8capacityS483;
  _block_4747->$3 = _M0L7_2abindS485;
  _block_4747->$4 = _M0L7_2abindS486;
  _block_4747->$5 = _M0L7_2abindS488;
  _block_4747->$6 = -1;
  return _block_4747;
}

struct _M0TPB3MapGssE* _M0FPB8new__mapGssE(int32_t _M0L8capacityS490) {
  int32_t _M0L8capacityS489;
  int32_t _M0L7_2abindS491;
  int32_t _M0L7_2abindS492;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2638;
  struct _M0TPB5EntryGssE** _M0L7_2abindS493;
  struct _M0TPB5EntryGssE* _M0L7_2abindS494;
  struct _M0TPB3MapGssE* _block_4748;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS489
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS490);
  _M0L7_2abindS491 = _M0L8capacityS489 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS492 = _M0FPB21calc__grow__threshold(_M0L8capacityS489);
  _M0L6_2atmpS2638 = 0;
  _M0L7_2abindS493
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L8capacityS489, _M0L6_2atmpS2638);
  _M0L7_2abindS494 = 0;
  _block_4748
  = (struct _M0TPB3MapGssE*)moonbit_malloc(sizeof(struct _M0TPB3MapGssE));
  Moonbit_object_header(_block_4748)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 108, 0);
  _block_4748->$0 = _M0L7_2abindS493;
  _block_4748->$1 = 0;
  _block_4748->$2 = _M0L8capacityS489;
  _block_4748->$3 = _M0L7_2abindS491;
  _block_4748->$4 = _M0L7_2abindS492;
  _block_4748->$5 = _M0L7_2abindS494;
  _block_4748->$6 = -1;
  return _block_4748;
}

struct _M0TPB3MapGsbE* _M0FPB8new__mapGsbE(int32_t _M0L8capacityS496) {
  int32_t _M0L8capacityS495;
  int32_t _M0L7_2abindS497;
  int32_t _M0L7_2abindS498;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2639;
  struct _M0TPB5EntryGsbE** _M0L7_2abindS499;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS500;
  struct _M0TPB3MapGsbE* _block_4749;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS495
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS496);
  _M0L7_2abindS497 = _M0L8capacityS495 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS498 = _M0FPB21calc__grow__threshold(_M0L8capacityS495);
  _M0L6_2atmpS2639 = 0;
  _M0L7_2abindS499
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L8capacityS495, _M0L6_2atmpS2639);
  _M0L7_2abindS500 = 0;
  _block_4749
  = (struct _M0TPB3MapGsbE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsbE));
  Moonbit_object_header(_block_4749)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 112, 0);
  _block_4749->$0 = _M0L7_2abindS499;
  _block_4749->$1 = 0;
  _block_4749->$2 = _M0L8capacityS495;
  _block_4749->$3 = _M0L7_2abindS497;
  _block_4749->$4 = _M0L7_2abindS498;
  _block_4749->$5 = _M0L7_2abindS500;
  _block_4749->$6 = -1;
  return _block_4749;
}

struct _M0TPB3MapGsfE* _M0FPB8new__mapGsfE(int32_t _M0L8capacityS502) {
  int32_t _M0L8capacityS501;
  int32_t _M0L7_2abindS503;
  int32_t _M0L7_2abindS504;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2640;
  struct _M0TPB5EntryGsfE** _M0L7_2abindS505;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS506;
  struct _M0TPB3MapGsfE* _block_4750;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS501
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS502);
  _M0L7_2abindS503 = _M0L8capacityS501 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS504 = _M0FPB21calc__grow__threshold(_M0L8capacityS501);
  _M0L6_2atmpS2640 = 0;
  _M0L7_2abindS505
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L8capacityS501, _M0L6_2atmpS2640);
  _M0L7_2abindS506 = 0;
  _block_4750
  = (struct _M0TPB3MapGsfE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsfE));
  Moonbit_object_header(_block_4750)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 116, 0);
  _block_4750->$0 = _M0L7_2abindS505;
  _block_4750->$1 = 0;
  _block_4750->$2 = _M0L8capacityS501;
  _block_4750->$3 = _M0L7_2abindS503;
  _block_4750->$4 = _M0L7_2abindS504;
  _block_4750->$5 = _M0L7_2abindS506;
  _block_4750->$6 = -1;
  return _block_4750;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS476) {
  #line 33 "/home/developer/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS476 >= 0) {
    int32_t _M0L6_2atmpS2635;
    int32_t _M0L6_2atmpS2634;
    int32_t _M0L6_2atmpS2633;
    int32_t _M0L6_2atmpS2632;
    if (_M0L4selfS476 <= 1) {
      return 1;
    }
    if (_M0L4selfS476 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2635 = _M0L4selfS476 - 1;
    #line 44 "/home/developer/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS2634 = moonbit_clz32(_M0L6_2atmpS2635);
    _M0L6_2atmpS2633 = _M0L6_2atmpS2634 - 1;
    _M0L6_2atmpS2632 = 2147483647 >> (_M0L6_2atmpS2633 & 31);
    return _M0L6_2atmpS2632 + 1;
  } else {
    #line 34 "/home/developer/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS475) {
  int32_t _M0L6_2atmpS2631;
  #line 610 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2631 = _M0L8capacityS475 * 13;
  return _M0L6_2atmpS2631 / 16;
}

int32_t _M0MPC16option6Option6unwrapGiE(int64_t _M0L4selfS463) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS463 == 4294967296ll) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    int64_t _M0L7_2aSomeS464 = _M0L4selfS463;
    return (int32_t)_M0L7_2aSomeS464;
  }
}

struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP19moonbitDB10RedisValueEE(
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4selfS465
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS465 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS466 =
      _M0L4selfS465;
    if (_M0L7_2aSomeS466) {
      moonbit_incref(_M0L7_2aSomeS466);
    }
    return _M0L7_2aSomeS466;
  }
}

struct _M0TPB5EntryGsiE* _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(
  struct _M0TPB5EntryGsiE* _M0L4selfS467
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS467 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS468 = _M0L4selfS467;
    if (_M0L7_2aSomeS468) {
      moonbit_incref(_M0L7_2aSomeS468);
    }
    return _M0L7_2aSomeS468;
  }
}

struct _M0TPB5EntryGssE* _M0MPC16option6Option6unwrapGRPB5EntryGssEE(
  struct _M0TPB5EntryGssE* _M0L4selfS469
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS469 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS470 = _M0L4selfS469;
    if (_M0L7_2aSomeS470) {
      moonbit_incref(_M0L7_2aSomeS470);
    }
    return _M0L7_2aSomeS470;
  }
}

struct _M0TPB5EntryGsbE* _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(
  struct _M0TPB5EntryGsbE* _M0L4selfS471
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS471 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS472 = _M0L4selfS471;
    if (_M0L7_2aSomeS472) {
      moonbit_incref(_M0L7_2aSomeS472);
    }
    return _M0L7_2aSomeS472;
  }
}

struct _M0TPB5EntryGsfE* _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(
  struct _M0TPB5EntryGsfE* _M0L4selfS473
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS473 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsfE* _M0L7_2aSomeS474 = _M0L4selfS473;
    if (_M0L7_2aSomeS474) {
      moonbit_incref(_M0L7_2aSomeS474);
    }
    return _M0L7_2aSomeS474;
  }
}

moonbit_string_t _M0MPC15array9ArrayView4joinGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS437,
  struct _M0TPC16string10StringView _M0L9separatorS450
) {
  int32_t _M0L3endS2606;
  int32_t _M0L5startS2607;
  int32_t _M0L6_2atmpS2605;
  #line 1497 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS2606 = _M0L4selfS437.$2;
  _M0L5startS2607 = _M0L4selfS437.$1;
  _M0L6_2atmpS2605 = _M0L3endS2606 - _M0L5startS2607;
  if (_M0L6_2atmpS2605 == 0) {
    return (moonbit_string_t)moonbit_string_literal_96.data;
  } else {
    moonbit_string_t* _M0L3bufS2629 = _M0L4selfS437.$0;
    int32_t _M0L5startS2630 = _M0L4selfS437.$1;
    moonbit_string_t _M0L5_2ahdS438 =
      (moonbit_string_t)_M0L3bufS2629[_M0L5startS2630];
    moonbit_string_t* _M0L9_2ax__bufS439 = _M0L4selfS437.$0;
    int32_t _M0L5startS2628 = _M0L4selfS437.$1;
    int32_t _M0L11_2ax__startS440 = 1 + _M0L5startS2628;
    int32_t _M0L9_2ax__endS441 = _M0L4selfS437.$2;
    struct _M0TPC16string10StringView _M0L2hdS442;
    int32_t _M0L7_2abindS443;
    int32_t _M0L3endS2626;
    int32_t _M0L5startS2627;
    int32_t _M0L6_2atmpS2625;
    int32_t _M0L10size__hintS444;
    int32_t _M0L2__S445;
    int32_t _M0L10size__hintS446;
    int32_t _M0L10size__hintS451;
    struct _M0TPB13StringBuilder* _M0L3bufS452;
    int32_t _M0L3endS2609;
    int32_t _M0L5startS2610;
    int32_t _M0L6_2atmpS2608;
    moonbit_string_t _result_4754;
    moonbit_incref(_M0L9_2ax__bufS439);
    moonbit_incref(_M0L5_2ahdS438);
    #line 1504 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0L2hdS442
    = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L5_2ahdS438);
    moonbit_decref(_M0L5_2ahdS438);
    _M0L7_2abindS443 = _M0L9_2ax__endS441 - _M0L11_2ax__startS440;
    _M0L3endS2626 = _M0L2hdS442.$2;
    _M0L5startS2627 = _M0L2hdS442.$1;
    _M0L6_2atmpS2625 = _M0L3endS2626 - _M0L5startS2627;
    _M0L2__S445 = 0;
    _M0L10size__hintS446 = _M0L6_2atmpS2625;
    while (1) {
      if (_M0L2__S445 < _M0L7_2abindS443) {
        int32_t _M0L6_2atmpS2624 = _M0L11_2ax__startS440 + _M0L2__S445;
        moonbit_string_t _M0L1sS447 =
          (moonbit_string_t)_M0L9_2ax__bufS439[_M0L6_2atmpS2624];
        int32_t _M0L6_2atmpS2615 = _M0L2__S445 + 1;
        struct _M0TPC16string10StringView _M0L7_2abindS449;
        int32_t _M0L3endS2622;
        int32_t _M0L5startS2623;
        int32_t _M0L6_2atmpS2621;
        int32_t _M0L6_2atmpS2617;
        int32_t _M0L3endS2619;
        int32_t _M0L5startS2620;
        int32_t _M0L6_2atmpS2618;
        int32_t _M0L6_2atmpS2616;
        moonbit_incref(_M0L1sS447);
        #line 1506 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
        _M0L7_2abindS449
        = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS447);
        moonbit_decref(_M0L1sS447);
        _M0L3endS2622 = _M0L7_2abindS449.$2;
        _M0L5startS2623 = _M0L7_2abindS449.$1;
        moonbit_decref(_M0L7_2abindS449.$0);
        _M0L6_2atmpS2621 = _M0L3endS2622 - _M0L5startS2623;
        _M0L6_2atmpS2617 = _M0L10size__hintS446 + _M0L6_2atmpS2621;
        _M0L3endS2619 = _M0L9separatorS450.$2;
        _M0L5startS2620 = _M0L9separatorS450.$1;
        _M0L6_2atmpS2618 = _M0L3endS2619 - _M0L5startS2620;
        _M0L6_2atmpS2616 = _M0L6_2atmpS2617 + _M0L6_2atmpS2618;
        _M0L2__S445 = _M0L6_2atmpS2615;
        _M0L10size__hintS446 = _M0L6_2atmpS2616;
        continue;
      } else {
        _M0L10size__hintS444 = _M0L10size__hintS446;
      }
      break;
    }
    _M0L10size__hintS451 = _M0L10size__hintS444 << 1;
    #line 1511 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0L3bufS452
    = _M0MPB13StringBuilder21StringBuilder_2einner(_M0L10size__hintS451);
    #line 1513 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS452, _M0L2hdS442);
    moonbit_decref(_M0L2hdS442.$0);
    _M0L3endS2609 = _M0L9separatorS450.$2;
    _M0L5startS2610 = _M0L9separatorS450.$1;
    _M0L6_2atmpS2608 = _M0L3endS2609 - _M0L5startS2610;
    if (_M0L6_2atmpS2608 == 0) {
      int32_t _M0L7_2abindS453 = _M0L9_2ax__endS441 - _M0L11_2ax__startS440;
      int32_t _M0L2__S454 = 0;
      while (1) {
        if (_M0L2__S454 < _M0L7_2abindS453) {
          int32_t _M0L6_2atmpS2612 = _M0L11_2ax__startS440 + _M0L2__S454;
          moonbit_string_t _M0L1sS455 =
            (moonbit_string_t)_M0L9_2ax__bufS439[_M0L6_2atmpS2612];
          struct _M0TPC16string10StringView _M0L1sS456;
          int32_t _M0L6_2atmpS2611;
          moonbit_incref(_M0L1sS455);
          #line 1517 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS456
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS455);
          moonbit_decref(_M0L1sS455);
          #line 1518 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS452, _M0L1sS456);
          moonbit_decref(_M0L1sS456.$0);
          _M0L6_2atmpS2611 = _M0L2__S454 + 1;
          _M0L2__S454 = _M0L6_2atmpS2611;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS439);
        }
        break;
      }
    } else {
      int32_t _M0L7_2abindS458 = _M0L9_2ax__endS441 - _M0L11_2ax__startS440;
      int32_t _M0L2__S459 = 0;
      while (1) {
        if (_M0L2__S459 < _M0L7_2abindS458) {
          int32_t _M0L6_2atmpS2614 = _M0L11_2ax__startS440 + _M0L2__S459;
          moonbit_string_t _M0L1sS460 =
            (moonbit_string_t)_M0L9_2ax__bufS439[_M0L6_2atmpS2614];
          struct _M0TPC16string10StringView _M0L1sS461;
          int32_t _M0L6_2atmpS2613;
          moonbit_incref(_M0L1sS460);
          #line 1522 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS461
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS460);
          moonbit_decref(_M0L1sS460);
          #line 1523 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS452, _M0L9separatorS450);
          #line 1525 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS452, _M0L1sS461);
          moonbit_decref(_M0L1sS461.$0);
          _M0L6_2atmpS2613 = _M0L2__S459 + 1;
          _M0L2__S459 = _M0L6_2atmpS2613;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS439);
        }
        break;
      }
    }
    #line 1528 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _result_4754 = _M0MPB13StringBuilder10to__string(_M0L3bufS452);
    moonbit_decref(_M0L3bufS452);
    return _result_4754;
  }
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS433,
  int32_t _M0L5indexS434
) {
  uint64_t* _M0L6_2atmpS2603;
  uint64_t _result_4755;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS433);
  _M0L6_2atmpS2603 = _M0L4selfS433;
  if (
    _M0L5indexS434 < 0
    || _M0L5indexS434 >= Moonbit_array_length(_M0L6_2atmpS2603)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_4755 = (uint64_t)_M0L6_2atmpS2603[_M0L5indexS434];
  moonbit_decref(_M0L6_2atmpS2603);
  return _result_4755;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS435,
  int32_t _M0L5indexS436
) {
  uint32_t* _M0L6_2atmpS2604;
  uint32_t _result_4756;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS435);
  _M0L6_2atmpS2604 = _M0L4selfS435;
  if (
    _M0L5indexS436 < 0
    || _M0L5indexS436 >= Moonbit_array_length(_M0L6_2atmpS2604)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_4756 = (uint32_t)_M0L6_2atmpS2604[_M0L5indexS436];
  moonbit_decref(_M0L6_2atmpS2604);
  return _result_4756;
}

moonbit_string_t _M0IPC16uint646UInt64PB4Show10to__string(
  uint64_t _M0L4selfS432
) {
  #line 50 "/home/developer/.moon/lib/core/builtin/show.mbt"
  #line 51 "/home/developer/.moon/lib/core/builtin/show.mbt"
  return _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS432, 10);
}

moonbit_string_t _M0IPC13int3IntPB4Show10to__string(int32_t _M0L4selfS431) {
  #line 35 "/home/developer/.moon/lib/core/builtin/show.mbt"
  #line 36 "/home/developer/.moon/lib/core/builtin/show.mbt"
  return _M0MPC13int3Int18to__string_2einner(_M0L4selfS431, 10);
}

moonbit_string_t _M0IPC14bool4BoolPB4Show10to__string(int32_t _M0L4selfS430) {
  #line 26 "/home/developer/.moon/lib/core/builtin/show.mbt"
  if (_M0L4selfS430) {
    return (moonbit_string_t)moonbit_string_literal_101.data;
  } else {
    return (moonbit_string_t)moonbit_string_literal_102.data;
  }
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS429) {
  #line 2494 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  return (uint64_t)_M0L4selfS429;
}

struct _M0TPC16string10StringView _M0IPC16string6StringPB12ToStringView16to__string__view(
  moonbit_string_t _M0L4selfS428
) {
  int32_t _M0L6_2atmpS2602;
  #line 24 "/home/developer/.moon/lib/core/builtin/string_like.mbt"
  _M0L6_2atmpS2602 = Moonbit_array_length(_M0L4selfS428);
  moonbit_incref(_M0L4selfS428);
  return (struct _M0TPC16string10StringView){.$0 = _M0L4selfS428,
                                               .$1 = 0,
                                               .$2 = _M0L6_2atmpS2602};
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS419,
  moonbit_string_t _M0L5valueS421
) {
  int32_t _M0L3lenS2587;
  moonbit_string_t* _M0L6_2atmpS2589;
  int32_t _M0L6_2atmpS2588;
  int32_t _M0L6lengthS420;
  moonbit_string_t* _M0L3bufS2590;
  moonbit_string_t _M0L6_2aoldS4323;
  int32_t _M0L6_2atmpS2591;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2587 = _M0L4selfS419->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2589 = _M0MPC15array5Array6bufferGsE(_M0L4selfS419);
  _M0L6_2atmpS2588 = Moonbit_array_length(_M0L6_2atmpS2589);
  moonbit_decref(_M0L6_2atmpS2589);
  if (_M0L3lenS2587 == _M0L6_2atmpS2588) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS419);
  }
  _M0L6lengthS420 = _M0L4selfS419->$1;
  _M0L3bufS2590 = _M0L4selfS419->$0;
  _M0L6_2aoldS4323 = (moonbit_string_t)_M0L3bufS2590[_M0L6lengthS420];
  moonbit_incref(_M0L5valueS421);
  moonbit_decref(_M0L6_2aoldS4323);
  _M0L3bufS2590[_M0L6lengthS420] = _M0L5valueS421;
  _M0L6_2atmpS2591 = _M0L6lengthS420 + 1;
  _M0L4selfS419->$1 = _M0L6_2atmpS2591;
  return 0;
}

int32_t _M0MPC15array5Array4pushGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS422,
  moonbit_string_t _M0L5valueS424
) {
  int32_t _M0L3lenS2592;
  moonbit_string_t* _M0L6_2atmpS2594;
  int32_t _M0L6_2atmpS2593;
  int32_t _M0L6lengthS423;
  moonbit_string_t* _M0L3bufS2595;
  moonbit_string_t _M0L6_2aoldS4325;
  int32_t _M0L6_2atmpS2596;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2592 = _M0L4selfS422->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2594 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS422);
  _M0L6_2atmpS2593 = Moonbit_array_length(_M0L6_2atmpS2594);
  moonbit_decref(_M0L6_2atmpS2594);
  if (_M0L3lenS2592 == _M0L6_2atmpS2593) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGOsE(_M0L4selfS422);
  }
  _M0L6lengthS423 = _M0L4selfS422->$1;
  _M0L3bufS2595 = _M0L4selfS422->$0;
  _M0L6_2aoldS4325 = (moonbit_string_t)_M0L3bufS2595[_M0L6lengthS423];
  if (_M0L5valueS424) {
    moonbit_incref(_M0L5valueS424);
  }
  if (_M0L6_2aoldS4325) {
    moonbit_decref(_M0L6_2aoldS4325);
  }
  _M0L3bufS2595[_M0L6lengthS423] = _M0L5valueS424;
  _M0L6_2atmpS2596 = _M0L6lengthS423 + 1;
  _M0L4selfS422->$1 = _M0L6_2atmpS2596;
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS425,
  struct _M0TUsfE* _M0L5valueS427
) {
  int32_t _M0L3lenS2597;
  struct _M0TUsfE** _M0L6_2atmpS2599;
  int32_t _M0L6_2atmpS2598;
  int32_t _M0L6lengthS426;
  struct _M0TUsfE** _M0L3bufS2600;
  struct _M0TUsfE* _M0L6_2aoldS4327;
  int32_t _M0L6_2atmpS2601;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2597 = _M0L4selfS425->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2599 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS425);
  _M0L6_2atmpS2598 = Moonbit_array_length(_M0L6_2atmpS2599);
  moonbit_decref(_M0L6_2atmpS2599);
  if (_M0L3lenS2597 == _M0L6_2atmpS2598) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsfEE(_M0L4selfS425);
  }
  _M0L6lengthS426 = _M0L4selfS425->$1;
  _M0L3bufS2600 = _M0L4selfS425->$0;
  _M0L6_2aoldS4327 = (struct _M0TUsfE*)_M0L3bufS2600[_M0L6lengthS426];
  moonbit_incref(_M0L5valueS427);
  if (_M0L6_2aoldS4327) {
    moonbit_decref(_M0L6_2aoldS4327);
  }
  _M0L3bufS2600[_M0L6lengthS426] = _M0L5valueS427;
  _M0L6_2atmpS2601 = _M0L6lengthS426 + 1;
  _M0L4selfS425->$1 = _M0L6_2atmpS2601;
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS411) {
  int32_t _M0L8old__capS410;
  int32_t _M0L8new__capS412;
  #line 245 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS410 = _M0L4selfS411->$1;
  if (_M0L8old__capS410 == 0) {
    _M0L8new__capS412 = 8;
  } else {
    _M0L8new__capS412 = _M0L8old__capS410 * 2;
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS411, _M0L8new__capS412);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS414
) {
  int32_t _M0L8old__capS413;
  int32_t _M0L8new__capS415;
  #line 245 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS413 = _M0L4selfS414->$1;
  if (_M0L8old__capS413 == 0) {
    _M0L8new__capS415 = 8;
  } else {
    _M0L8new__capS415 = _M0L8old__capS413 * 2;
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGOsE(_M0L4selfS414, _M0L8new__capS415);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS417
) {
  int32_t _M0L8old__capS416;
  int32_t _M0L8new__capS418;
  #line 245 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__capS416 = _M0L4selfS417->$1;
  if (_M0L8old__capS416 == 0) {
    _M0L8new__capS418 = 8;
  } else {
    _M0L8new__capS418 = _M0L8old__capS416 * 2;
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsfEE(_M0L4selfS417, _M0L8new__capS418);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS393,
  int32_t _M0L13new__capacityS396
) {
  moonbit_string_t* _M0L8old__bufS392;
  int32_t _M0L8old__capS394;
  int32_t _M0L9copy__lenS395;
  moonbit_string_t* _M0L8new__bufS397;
  moonbit_string_t* _M0L6_2aoldS4329;
  #line 189 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__bufS392 = _M0L4selfS393->$0;
  _M0L8old__capS394 = Moonbit_array_length(_M0L8old__bufS392);
  if (_M0L8old__capS394 < _M0L13new__capacityS396) {
    _M0L9copy__lenS395 = _M0L8old__capS394;
  } else {
    _M0L9copy__lenS395 = _M0L13new__capacityS396;
  }
  moonbit_incref(_M0L8old__bufS392);
  #line 193 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS397
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(_M0L8old__bufS392, _M0L13new__capacityS396, _M0L9copy__lenS395, 0, 0);
  moonbit_decref(_M0L8old__bufS392);
  _M0L6_2aoldS4329 = _M0L4selfS393->$0;
  moonbit_decref(_M0L6_2aoldS4329);
  _M0L4selfS393->$0 = _M0L8new__bufS397;
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS399,
  int32_t _M0L13new__capacityS402
) {
  moonbit_string_t* _M0L8old__bufS398;
  int32_t _M0L8old__capS400;
  int32_t _M0L9copy__lenS401;
  moonbit_string_t* _M0L8new__bufS403;
  moonbit_string_t* _M0L6_2aoldS4331;
  #line 189 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__bufS398 = _M0L4selfS399->$0;
  _M0L8old__capS400 = Moonbit_array_length(_M0L8old__bufS398);
  if (_M0L8old__capS400 < _M0L13new__capacityS402) {
    _M0L9copy__lenS401 = _M0L8old__capS400;
  } else {
    _M0L9copy__lenS401 = _M0L13new__capacityS402;
  }
  moonbit_incref(_M0L8old__bufS398);
  #line 193 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS403
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGOsE(_M0L8old__bufS398, _M0L13new__capacityS402, _M0L9copy__lenS401, 0, 0);
  moonbit_decref(_M0L8old__bufS398);
  _M0L6_2aoldS4331 = _M0L4selfS399->$0;
  moonbit_decref(_M0L6_2aoldS4331);
  _M0L4selfS399->$0 = _M0L8new__bufS403;
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS405,
  int32_t _M0L13new__capacityS408
) {
  struct _M0TUsfE** _M0L8old__bufS404;
  int32_t _M0L8old__capS406;
  int32_t _M0L9copy__lenS407;
  struct _M0TUsfE** _M0L8new__bufS409;
  struct _M0TUsfE** _M0L6_2aoldS4333;
  #line 189 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8old__bufS404 = _M0L4selfS405->$0;
  _M0L8old__capS406 = Moonbit_array_length(_M0L8old__bufS404);
  if (_M0L8old__capS406 < _M0L13new__capacityS408) {
    _M0L9copy__lenS407 = _M0L8old__capS406;
  } else {
    _M0L9copy__lenS407 = _M0L13new__capacityS408;
  }
  moonbit_incref(_M0L8old__bufS404);
  #line 193 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8new__bufS409
  = _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(_M0L8old__bufS404, _M0L13new__capacityS408, _M0L9copy__lenS407, 0, 0);
  moonbit_decref(_M0L8old__bufS404);
  _M0L6_2aoldS4333 = _M0L4selfS405->$0;
  moonbit_decref(_M0L6_2aoldS4333);
  _M0L4selfS405->$0 = _M0L8new__bufS409;
  return 0;
}

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE* _M0L4selfS388) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS388->$1;
}

int32_t _M0MPC15array5Array6lengthGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS389
) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS389->$1;
}

int32_t _M0MPC15array5Array6lengthGRP39moonbitDB8examples11leaderboard6PlayerE(
  struct _M0TPB5ArrayGRP39moonbitDB8examples11leaderboard6PlayerE* _M0L4selfS390
) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS390->$1;
}

int32_t _M0MPC15array5Array6lengthGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS391
) {
  #line 140 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  return _M0L4selfS391->$1;
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS387
) {
  #line 222 "/home/developer/.moon/lib/core/builtin/show.mbt"
  moonbit_incref(_M0L4selfS387);
  return _M0L4selfS387;
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS386,
  struct _M0TPC16string10StringView _M0L3strS385
) {
  int32_t _M0L3endS2585;
  int32_t _M0L5startS2586;
  int32_t _M0L8str__lenS384;
  int32_t _M0L3lenS2578;
  int32_t _M0L6_2atmpS2577;
  uint16_t* _M0L4dataS2579;
  int32_t _M0L3lenS2580;
  moonbit_string_t _M0L6_2atmpS2581;
  int32_t _M0L6_2atmpS2582;
  int32_t _M0L3lenS2584;
  int32_t _M0L6_2atmpS2583;
  #line 131 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3endS2585 = _M0L3strS385.$2;
  _M0L5startS2586 = _M0L3strS385.$1;
  _M0L8str__lenS384 = _M0L3endS2585 - _M0L5startS2586;
  _M0L3lenS2578 = _M0L4selfS386->$1;
  _M0L6_2atmpS2577 = _M0L3lenS2578 + _M0L8str__lenS384;
  #line 136 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS386, _M0L6_2atmpS2577);
  _M0L4dataS2579 = _M0L4selfS386->$0;
  _M0L3lenS2580 = _M0L4selfS386->$1;
  moonbit_incref(_M0L4dataS2579);
  #line 139 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2581 = _M0MPC16string10StringView4data(_M0L3strS385);
  #line 140 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2582 = _M0MPC16string10StringView13start__offset(_M0L3strS385);
  #line 137 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2579, _M0L3lenS2580, _M0L6_2atmpS2581, _M0L6_2atmpS2582, _M0L8str__lenS384);
  moonbit_decref(_M0L4dataS2579);
  moonbit_decref(_M0L6_2atmpS2581);
  _M0L3lenS2584 = _M0L4selfS386->$1;
  _M0L6_2atmpS2583 = _M0L3lenS2584 + _M0L8str__lenS384;
  _M0L4selfS386->$1 = _M0L6_2atmpS2583;
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/home/developer/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS380
) {
  moonbit_string_t* _M0L8_2afieldS4336;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS4336 = _M0L4selfS380->$0;
  moonbit_incref(_M0L8_2afieldS4336);
  return _M0L8_2afieldS4336;
}

moonbit_string_t* _M0MPC15array5Array6bufferGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS381
) {
  moonbit_string_t* _M0L8_2afieldS4337;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS4337 = _M0L4selfS381->$0;
  moonbit_incref(_M0L8_2afieldS4337);
  return _M0L8_2afieldS4337;
}

struct _M0TP39moonbitDB8examples11leaderboard6Player** _M0MPC15array5Array6bufferGRP39moonbitDB8examples11leaderboard6PlayerE(
  struct _M0TPB5ArrayGRP39moonbitDB8examples11leaderboard6PlayerE* _M0L4selfS382
) {
  struct _M0TP39moonbitDB8examples11leaderboard6Player** _M0L8_2afieldS4338;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS4338 = _M0L4selfS382->$0;
  moonbit_incref(_M0L8_2afieldS4338);
  return _M0L8_2afieldS4338;
}

struct _M0TUsfE** _M0MPC15array5Array6bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS383
) {
  struct _M0TUsfE** _M0L8_2afieldS4339;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS4339 = _M0L4selfS383->$0;
  moonbit_incref(_M0L8_2afieldS4339);
  return _M0L8_2afieldS4339;
}

struct _M0TPB4IterGUssEE* _M0MPB4Iter3newGUssEE(
  struct _M0TWEOUssE* _M0L1fS364,
  int64_t _M0L10size__hintS361
) {
  int64_t _M0L10size__hintS360;
  struct _M0TPB4IterGUssEE* _block_4757;
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
  _block_4757
  = (struct _M0TPB4IterGUssEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUssEE));
  Moonbit_object_header(_block_4757)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 120, 0);
  _block_4757->$0 = _M0L1fS364;
  _block_4757->$1 = _M0L10size__hintS360;
  return _block_4757;
}

struct _M0TPB4IterGUsbEE* _M0MPB4Iter3newGUsbEE(
  struct _M0TWEOUsbE* _M0L1fS369,
  int64_t _M0L10size__hintS366
) {
  int64_t _M0L10size__hintS365;
  struct _M0TPB4IterGUsbEE* _block_4758;
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
  _block_4758
  = (struct _M0TPB4IterGUsbEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsbEE));
  Moonbit_object_header(_block_4758)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 123, 0);
  _block_4758->$0 = _M0L1fS369;
  _block_4758->$1 = _M0L10size__hintS365;
  return _block_4758;
}

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB4Iter3newGUsRP19moonbitDB10RedisValueEE(
  struct _M0TWEOUsRP19moonbitDB10RedisValueE* _M0L1fS374,
  int64_t _M0L10size__hintS371
) {
  int64_t _M0L10size__hintS370;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _block_4759;
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
      = _M0MPB4Iter3newN6constrS9988GUsRP19moonbitDB10RedisValueEE;
    }
  }
  moonbit_incref(_M0L1fS374);
  _block_4759
  = (struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE));
  Moonbit_object_header(_block_4759)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 126, 0);
  _block_4759->$0 = _M0L1fS374;
  _block_4759->$1 = _M0L10size__hintS370;
  return _block_4759;
}

struct _M0TPB4IterGUsfEE* _M0MPB4Iter3newGUsfEE(
  struct _M0TWEOUsfE* _M0L1fS379,
  int64_t _M0L10size__hintS376
) {
  int64_t _M0L10size__hintS375;
  struct _M0TPB4IterGUsfEE* _block_4760;
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
  _block_4760
  = (struct _M0TPB4IterGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsfEE));
  Moonbit_object_header(_block_4760)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 129, 0);
  _block_4760->$0 = _M0L1fS379;
  _block_4760->$1 = _M0L10size__hintS375;
  return _block_4760;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS352,
  int32_t _M0L5radixS351
) {
  int32_t _if__result_4761;
  uint16_t* _M0L6bufferS353;
  #line 607 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS351 < 2) {
    _if__result_4761 = 1;
  } else {
    _if__result_4761 = _M0L5radixS351 > 36;
  }
  if (_if__result_4761) {
    #line 611 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_103.data);
  }
  if (_M0L4selfS352 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_85.data;
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
  int32_t _M0L6_2atmpS2576;
  uint64_t _M0L3numS327;
  int32_t _M0L6offsetS328;
  #line 493 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2576 = _M0L10total__lenS350 - _M0L12digit__startS338;
  _M0L3numS327 = _M0L3numS349;
  _M0L6offsetS328 = _M0L6_2atmpS2576;
  while (1) {
    if (_M0L3numS327 >= 10000ull) {
      uint64_t _M0L1tS329 = _M0L3numS327 / 10000ull;
      uint64_t _M0L6_2atmpS2553 = _M0L3numS327 % 10000ull;
      int32_t _M0L1rS330 = (int32_t)_M0L6_2atmpS2553;
      int32_t _M0L2d1S331 = _M0L1rS330 / 100;
      int32_t _M0L2d2S332 = _M0L1rS330 % 100;
      int32_t _M0L6_2atmpS2552 = _M0L2d1S331 / 10;
      int32_t _M0L6_2atmpS2551 = 48 + _M0L6_2atmpS2552;
      int32_t _M0L6d1__hiS333 = (uint16_t)_M0L6_2atmpS2551;
      int32_t _M0L6_2atmpS2550 = _M0L2d1S331 % 10;
      int32_t _M0L6_2atmpS2549 = 48 + _M0L6_2atmpS2550;
      int32_t _M0L6d1__loS334 = (uint16_t)_M0L6_2atmpS2549;
      int32_t _M0L6_2atmpS2548 = _M0L2d2S332 / 10;
      int32_t _M0L6_2atmpS2547 = 48 + _M0L6_2atmpS2548;
      int32_t _M0L6d2__hiS335 = (uint16_t)_M0L6_2atmpS2547;
      int32_t _M0L6_2atmpS2546 = _M0L2d2S332 % 10;
      int32_t _M0L6_2atmpS2545 = 48 + _M0L6_2atmpS2546;
      int32_t _M0L6d2__loS336 = (uint16_t)_M0L6_2atmpS2545;
      int32_t _M0L6_2atmpS2537 = _M0L12digit__startS338 + _M0L6offsetS328;
      int32_t _M0L6_2atmpS2536 = _M0L6_2atmpS2537 - 4;
      int32_t _M0L6_2atmpS2539;
      int32_t _M0L6_2atmpS2538;
      int32_t _M0L6_2atmpS2541;
      int32_t _M0L6_2atmpS2540;
      int32_t _M0L6_2atmpS2543;
      int32_t _M0L6_2atmpS2542;
      int32_t _M0L6_2atmpS2544;
      _M0L6bufferS337[_M0L6_2atmpS2536] = _M0L6d1__hiS333;
      _M0L6_2atmpS2539 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2538 = _M0L6_2atmpS2539 - 3;
      _M0L6bufferS337[_M0L6_2atmpS2538] = _M0L6d1__loS334;
      _M0L6_2atmpS2541 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2540 = _M0L6_2atmpS2541 - 2;
      _M0L6bufferS337[_M0L6_2atmpS2540] = _M0L6d2__hiS335;
      _M0L6_2atmpS2543 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2542 = _M0L6_2atmpS2543 - 1;
      _M0L6bufferS337[_M0L6_2atmpS2542] = _M0L6d2__loS336;
      _M0L6_2atmpS2544 = _M0L6offsetS328 - 4;
      _M0L3numS327 = _M0L1tS329;
      _M0L6offsetS328 = _M0L6_2atmpS2544;
      continue;
    } else {
      int32_t _M0L6_2atmpS2575 = (int32_t)_M0L3numS327;
      int32_t _M0L9remainingS340 = _M0L6_2atmpS2575;
      int32_t _M0L6offsetS341 = _M0L6offsetS328;
      while (1) {
        if (_M0L9remainingS340 >= 100) {
          int32_t _M0L1tS342 = _M0L9remainingS340 / 100;
          int32_t _M0L1dS343 = _M0L9remainingS340 % 100;
          int32_t _M0L6_2atmpS2562 = _M0L1dS343 / 10;
          int32_t _M0L6_2atmpS2561 = 48 + _M0L6_2atmpS2562;
          int32_t _M0L5d__hiS344 = (uint16_t)_M0L6_2atmpS2561;
          int32_t _M0L6_2atmpS2560 = _M0L1dS343 % 10;
          int32_t _M0L6_2atmpS2559 = 48 + _M0L6_2atmpS2560;
          int32_t _M0L5d__loS345 = (uint16_t)_M0L6_2atmpS2559;
          int32_t _M0L6_2atmpS2555 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2554 = _M0L6_2atmpS2555 - 2;
          int32_t _M0L6_2atmpS2557;
          int32_t _M0L6_2atmpS2556;
          int32_t _M0L6_2atmpS2558;
          _M0L6bufferS337[_M0L6_2atmpS2554] = _M0L5d__hiS344;
          _M0L6_2atmpS2557 = _M0L12digit__startS338 + _M0L6offsetS341;
          _M0L6_2atmpS2556 = _M0L6_2atmpS2557 - 1;
          _M0L6bufferS337[_M0L6_2atmpS2556] = _M0L5d__loS345;
          _M0L6_2atmpS2558 = _M0L6offsetS341 - 2;
          _M0L9remainingS340 = _M0L1tS342;
          _M0L6offsetS341 = _M0L6_2atmpS2558;
          continue;
        } else if (_M0L9remainingS340 >= 10) {
          int32_t _M0L6_2atmpS2570 = _M0L9remainingS340 / 10;
          int32_t _M0L6_2atmpS2569 = 48 + _M0L6_2atmpS2570;
          int32_t _M0L5d__hiS347 = (uint16_t)_M0L6_2atmpS2569;
          int32_t _M0L6_2atmpS2568 = _M0L9remainingS340 % 10;
          int32_t _M0L6_2atmpS2567 = 48 + _M0L6_2atmpS2568;
          int32_t _M0L5d__loS348 = (uint16_t)_M0L6_2atmpS2567;
          int32_t _M0L6_2atmpS2564 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2563 = _M0L6_2atmpS2564 - 2;
          int32_t _M0L6_2atmpS2566;
          int32_t _M0L6_2atmpS2565;
          _M0L6bufferS337[_M0L6_2atmpS2563] = _M0L5d__hiS347;
          _M0L6_2atmpS2566 = _M0L12digit__startS338 + _M0L6offsetS341;
          _M0L6_2atmpS2565 = _M0L6_2atmpS2566 - 1;
          _M0L6bufferS337[_M0L6_2atmpS2565] = _M0L5d__loS348;
        } else {
          int32_t _M0L6_2atmpS2574 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2571 = _M0L6_2atmpS2574 - 1;
          int32_t _M0L6_2atmpS2573 = 48 + _M0L9remainingS340;
          int32_t _M0L6_2atmpS2572 = (uint16_t)_M0L6_2atmpS2573;
          _M0L6bufferS337[_M0L6_2atmpS2571] = _M0L6_2atmpS2572;
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
  int32_t _M0L6_2atmpS2521;
  int32_t _M0L6_2atmpS2520;
  #line 462 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  #line 470 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS310 = _M0MPC13int3Int10to__uint64(_M0L5radixS311);
  _M0L6_2atmpS2521 = _M0L5radixS311 - 1;
  _M0L6_2atmpS2520 = _M0L5radixS311 & _M0L6_2atmpS2521;
  if (_M0L6_2atmpS2520 == 0) {
    int32_t _M0L5shiftS312;
    uint64_t _M0L4maskS313;
    int32_t _M0L6_2atmpS2528;
    int32_t _M0L6offsetS314;
    uint64_t _M0L1nS315;
    #line 473 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS312 = moonbit_ctz32(_M0L5radixS311);
    _M0L4maskS313 = _M0L4baseS310 - 1ull;
    _M0L6_2atmpS2528 = _M0L10total__lenS320 - _M0L12digit__startS318;
    _M0L6offsetS314 = _M0L6_2atmpS2528;
    _M0L1nS315 = _M0L3numS321;
    while (1) {
      if (_M0L1nS315 > 0ull) {
        uint64_t _M0L6_2atmpS2527 = _M0L1nS315 & _M0L4maskS313;
        int32_t _M0L5digitS316 = (int32_t)_M0L6_2atmpS2527;
        int32_t _M0L6_2atmpS2524 = _M0L12digit__startS318 + _M0L6offsetS314;
        int32_t _M0L6_2atmpS2522 = _M0L6_2atmpS2524 - 1;
        int32_t _M0L6_2atmpS2523 =
          ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L5digitS316];
        int32_t _M0L6_2atmpS2525;
        uint64_t _M0L6_2atmpS2526;
        _M0L6bufferS317[_M0L6_2atmpS2522] = _M0L6_2atmpS2523;
        _M0L6_2atmpS2525 = _M0L6offsetS314 - 1;
        _M0L6_2atmpS2526 = _M0L1nS315 >> (_M0L5shiftS312 & 63);
        _M0L6offsetS314 = _M0L6_2atmpS2525;
        _M0L1nS315 = _M0L6_2atmpS2526;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2535 = _M0L10total__lenS320 - _M0L12digit__startS318;
    int32_t _M0L6offsetS322 = _M0L6_2atmpS2535;
    uint64_t _M0L1nS323 = _M0L3numS321;
    while (1) {
      if (_M0L1nS323 > 0ull) {
        uint64_t _M0L1qS324 = _M0L1nS323 / _M0L4baseS310;
        uint64_t _M0L6_2atmpS2534 = _M0L1qS324 * _M0L4baseS310;
        uint64_t _M0L6_2atmpS2533 = _M0L1nS323 - _M0L6_2atmpS2534;
        int32_t _M0L5digitS325 = (int32_t)_M0L6_2atmpS2533;
        int32_t _M0L6_2atmpS2531 = _M0L12digit__startS318 + _M0L6offsetS322;
        int32_t _M0L6_2atmpS2529 = _M0L6_2atmpS2531 - 1;
        int32_t _M0L6_2atmpS2530 =
          ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L5digitS325];
        int32_t _M0L6_2atmpS2532;
        _M0L6bufferS317[_M0L6_2atmpS2529] = _M0L6_2atmpS2530;
        _M0L6_2atmpS2532 = _M0L6offsetS322 - 1;
        _M0L6offsetS322 = _M0L6_2atmpS2532;
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
  int32_t _M0L6_2atmpS2519;
  int32_t _M0L6offsetS299;
  uint64_t _M0L1nS300;
  #line 434 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2519 = _M0L10total__lenS308 - _M0L12digit__startS305;
  _M0L6offsetS299 = _M0L6_2atmpS2519;
  _M0L1nS300 = _M0L3numS309;
  while (1) {
    if (_M0L6offsetS299 >= 2) {
      uint64_t _M0L6_2atmpS2516 = _M0L1nS300 & 255ull;
      int32_t _M0L9byte__valS301 = (int32_t)_M0L6_2atmpS2516;
      int32_t _M0L2hiS302 = _M0L9byte__valS301 / 16;
      int32_t _M0L2loS303 = _M0L9byte__valS301 % 16;
      int32_t _M0L6_2atmpS2510 = _M0L12digit__startS305 + _M0L6offsetS299;
      int32_t _M0L6_2atmpS2508 = _M0L6_2atmpS2510 - 2;
      int32_t _M0L6_2atmpS2509 =
        ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L2hiS302];
      int32_t _M0L6_2atmpS2513;
      int32_t _M0L6_2atmpS2511;
      int32_t _M0L6_2atmpS2512;
      int32_t _M0L6_2atmpS2514;
      uint64_t _M0L6_2atmpS2515;
      _M0L6bufferS304[_M0L6_2atmpS2508] = _M0L6_2atmpS2509;
      _M0L6_2atmpS2513 = _M0L12digit__startS305 + _M0L6offsetS299;
      _M0L6_2atmpS2511 = _M0L6_2atmpS2513 - 1;
      _M0L6_2atmpS2512
      = ((moonbit_string_t)moonbit_string_literal_104.data)[
        _M0L2loS303
      ];
      _M0L6bufferS304[_M0L6_2atmpS2511] = _M0L6_2atmpS2512;
      _M0L6_2atmpS2514 = _M0L6offsetS299 - 2;
      _M0L6_2atmpS2515 = _M0L1nS300 >> 8;
      _M0L6offsetS299 = _M0L6_2atmpS2514;
      _M0L1nS300 = _M0L6_2atmpS2515;
      continue;
    } else if (_M0L6offsetS299 == 1) {
      uint64_t _M0L6_2atmpS2518 = _M0L1nS300 & 15ull;
      int32_t _M0L6nibbleS307 = (int32_t)_M0L6_2atmpS2518;
      int32_t _M0L6_2atmpS2517 =
        ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L6nibbleS307];
      _M0L6bufferS304[_M0L12digit__startS305] = _M0L6_2atmpS2517;
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
      uint64_t _M0L6_2atmpS2506 = _M0L3numS296 / _M0L4baseS294;
      int32_t _M0L6_2atmpS2507 = _M0L5countS297 + 1;
      _M0L3numS296 = _M0L6_2atmpS2506;
      _M0L5countS297 = _M0L6_2atmpS2507;
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
    int32_t _M0L6_2atmpS2505;
    int32_t _M0L6_2atmpS2504;
    #line 412 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS292 = moonbit_clz64(_M0L5valueS291);
    _M0L6_2atmpS2505 = 63 - _M0L14leading__zerosS292;
    _M0L6_2atmpS2504 = _M0L6_2atmpS2505 / 4;
    return _M0L6_2atmpS2504 + 1;
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
  int32_t _if__result_4768;
  int32_t _M0L12is__negativeS275;
  uint32_t _M0L3numS276;
  uint16_t* _M0L6bufferS277;
  #line 209 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS273 < 2) {
    _if__result_4768 = 1;
  } else {
    _if__result_4768 = _M0L5radixS273 > 36;
  }
  if (_if__result_4768) {
    #line 213 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_103.data);
  }
  if (_M0L4selfS274 == 0) {
    return (moonbit_string_t)moonbit_string_literal_85.data;
  }
  _M0L12is__negativeS275 = _M0L4selfS274 < 0;
  if (_M0L12is__negativeS275) {
    int32_t _M0L6_2atmpS2503 = -_M0L4selfS274;
    _M0L3numS276 = *(uint32_t*)&_M0L6_2atmpS2503;
  } else {
    _M0L3numS276 = *(uint32_t*)&_M0L4selfS274;
  }
  switch (_M0L5radixS273) {
    case 10: {
      int32_t _M0L10digit__lenS278;
      int32_t _M0L6_2atmpS2500;
      int32_t _M0L10total__lenS279;
      uint16_t* _M0L6bufferS280;
      int32_t _M0L12digit__startS281;
      #line 235 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS278 = _M0FPB12dec__count32(_M0L3numS276);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2500 = 1;
      } else {
        _M0L6_2atmpS2500 = 0;
      }
      _M0L10total__lenS279 = _M0L10digit__lenS278 + _M0L6_2atmpS2500;
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
      int32_t _M0L6_2atmpS2501;
      int32_t _M0L10total__lenS283;
      uint16_t* _M0L6bufferS284;
      int32_t _M0L12digit__startS285;
      #line 243 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS282 = _M0FPB12hex__count32(_M0L3numS276);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2501 = 1;
      } else {
        _M0L6_2atmpS2501 = 0;
      }
      _M0L10total__lenS283 = _M0L10digit__lenS282 + _M0L6_2atmpS2501;
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
      int32_t _M0L6_2atmpS2502;
      int32_t _M0L10total__lenS287;
      uint16_t* _M0L6bufferS288;
      int32_t _M0L12digit__startS289;
      #line 251 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS286
      = _M0FPB14radix__count32(_M0L3numS276, _M0L5radixS273);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2502 = 1;
      } else {
        _M0L6_2atmpS2502 = 0;
      }
      _M0L10total__lenS287 = _M0L10digit__lenS286 + _M0L6_2atmpS2502;
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
      uint32_t _M0L6_2atmpS2498 = _M0L3numS270 / _M0L4baseS268;
      int32_t _M0L6_2atmpS2499 = _M0L5countS271 + 1;
      _M0L3numS270 = _M0L6_2atmpS2498;
      _M0L5countS271 = _M0L6_2atmpS2499;
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
    int32_t _M0L6_2atmpS2497;
    int32_t _M0L6_2atmpS2496;
    #line 182 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS266 = moonbit_clz32(_M0L5valueS265);
    _M0L6_2atmpS2497 = 31 - _M0L14leading__zerosS266;
    _M0L6_2atmpS2496 = _M0L6_2atmpS2497 / 4;
    return _M0L6_2atmpS2496 + 1;
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
  int32_t _M0L6_2atmpS2495;
  uint32_t _M0L3numS240;
  int32_t _M0L6offsetS241;
  #line 88 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2495 = _M0L10total__lenS263 - _M0L12digit__startS251;
  _M0L3numS240 = _M0L3numS262;
  _M0L6offsetS241 = _M0L6_2atmpS2495;
  while (1) {
    if (_M0L3numS240 >= 10000u) {
      uint32_t _M0L1tS242 = _M0L3numS240 / 10000u;
      uint32_t _M0L6_2atmpS2472 = _M0L3numS240 % 10000u;
      int32_t _M0L1rS243 = *(int32_t*)&_M0L6_2atmpS2472;
      int32_t _M0L2d1S244 = _M0L1rS243 / 100;
      int32_t _M0L2d2S245 = _M0L1rS243 % 100;
      int32_t _M0L6_2atmpS2471 = _M0L2d1S244 / 10;
      int32_t _M0L6_2atmpS2470 = 48 + _M0L6_2atmpS2471;
      int32_t _M0L6d1__hiS246 = (uint16_t)_M0L6_2atmpS2470;
      int32_t _M0L6_2atmpS2469 = _M0L2d1S244 % 10;
      int32_t _M0L6_2atmpS2468 = 48 + _M0L6_2atmpS2469;
      int32_t _M0L6d1__loS247 = (uint16_t)_M0L6_2atmpS2468;
      int32_t _M0L6_2atmpS2467 = _M0L2d2S245 / 10;
      int32_t _M0L6_2atmpS2466 = 48 + _M0L6_2atmpS2467;
      int32_t _M0L6d2__hiS248 = (uint16_t)_M0L6_2atmpS2466;
      int32_t _M0L6_2atmpS2465 = _M0L2d2S245 % 10;
      int32_t _M0L6_2atmpS2464 = 48 + _M0L6_2atmpS2465;
      int32_t _M0L6d2__loS249 = (uint16_t)_M0L6_2atmpS2464;
      int32_t _M0L6_2atmpS2456 = _M0L12digit__startS251 + _M0L6offsetS241;
      int32_t _M0L6_2atmpS2455 = _M0L6_2atmpS2456 - 4;
      int32_t _M0L6_2atmpS2458;
      int32_t _M0L6_2atmpS2457;
      int32_t _M0L6_2atmpS2460;
      int32_t _M0L6_2atmpS2459;
      int32_t _M0L6_2atmpS2462;
      int32_t _M0L6_2atmpS2461;
      int32_t _M0L6_2atmpS2463;
      _M0L6bufferS250[_M0L6_2atmpS2455] = _M0L6d1__hiS246;
      _M0L6_2atmpS2458 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2457 = _M0L6_2atmpS2458 - 3;
      _M0L6bufferS250[_M0L6_2atmpS2457] = _M0L6d1__loS247;
      _M0L6_2atmpS2460 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2459 = _M0L6_2atmpS2460 - 2;
      _M0L6bufferS250[_M0L6_2atmpS2459] = _M0L6d2__hiS248;
      _M0L6_2atmpS2462 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2461 = _M0L6_2atmpS2462 - 1;
      _M0L6bufferS250[_M0L6_2atmpS2461] = _M0L6d2__loS249;
      _M0L6_2atmpS2463 = _M0L6offsetS241 - 4;
      _M0L3numS240 = _M0L1tS242;
      _M0L6offsetS241 = _M0L6_2atmpS2463;
      continue;
    } else {
      int32_t _M0L6_2atmpS2494 = *(int32_t*)&_M0L3numS240;
      int32_t _M0L9remainingS253 = _M0L6_2atmpS2494;
      int32_t _M0L6offsetS254 = _M0L6offsetS241;
      while (1) {
        if (_M0L9remainingS253 >= 100) {
          int32_t _M0L1tS255 = _M0L9remainingS253 / 100;
          int32_t _M0L1dS256 = _M0L9remainingS253 % 100;
          int32_t _M0L6_2atmpS2481 = _M0L1dS256 / 10;
          int32_t _M0L6_2atmpS2480 = 48 + _M0L6_2atmpS2481;
          int32_t _M0L5d__hiS257 = (uint16_t)_M0L6_2atmpS2480;
          int32_t _M0L6_2atmpS2479 = _M0L1dS256 % 10;
          int32_t _M0L6_2atmpS2478 = 48 + _M0L6_2atmpS2479;
          int32_t _M0L5d__loS258 = (uint16_t)_M0L6_2atmpS2478;
          int32_t _M0L6_2atmpS2474 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2473 = _M0L6_2atmpS2474 - 2;
          int32_t _M0L6_2atmpS2476;
          int32_t _M0L6_2atmpS2475;
          int32_t _M0L6_2atmpS2477;
          _M0L6bufferS250[_M0L6_2atmpS2473] = _M0L5d__hiS257;
          _M0L6_2atmpS2476 = _M0L12digit__startS251 + _M0L6offsetS254;
          _M0L6_2atmpS2475 = _M0L6_2atmpS2476 - 1;
          _M0L6bufferS250[_M0L6_2atmpS2475] = _M0L5d__loS258;
          _M0L6_2atmpS2477 = _M0L6offsetS254 - 2;
          _M0L9remainingS253 = _M0L1tS255;
          _M0L6offsetS254 = _M0L6_2atmpS2477;
          continue;
        } else if (_M0L9remainingS253 >= 10) {
          int32_t _M0L6_2atmpS2489 = _M0L9remainingS253 / 10;
          int32_t _M0L6_2atmpS2488 = 48 + _M0L6_2atmpS2489;
          int32_t _M0L5d__hiS260 = (uint16_t)_M0L6_2atmpS2488;
          int32_t _M0L6_2atmpS2487 = _M0L9remainingS253 % 10;
          int32_t _M0L6_2atmpS2486 = 48 + _M0L6_2atmpS2487;
          int32_t _M0L5d__loS261 = (uint16_t)_M0L6_2atmpS2486;
          int32_t _M0L6_2atmpS2483 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2482 = _M0L6_2atmpS2483 - 2;
          int32_t _M0L6_2atmpS2485;
          int32_t _M0L6_2atmpS2484;
          _M0L6bufferS250[_M0L6_2atmpS2482] = _M0L5d__hiS260;
          _M0L6_2atmpS2485 = _M0L12digit__startS251 + _M0L6offsetS254;
          _M0L6_2atmpS2484 = _M0L6_2atmpS2485 - 1;
          _M0L6bufferS250[_M0L6_2atmpS2484] = _M0L5d__loS261;
        } else {
          int32_t _M0L6_2atmpS2493 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2490 = _M0L6_2atmpS2493 - 1;
          int32_t _M0L6_2atmpS2492 = 48 + _M0L9remainingS253;
          int32_t _M0L6_2atmpS2491 = (uint16_t)_M0L6_2atmpS2492;
          _M0L6bufferS250[_M0L6_2atmpS2490] = _M0L6_2atmpS2491;
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
  int32_t _M0L6_2atmpS2440;
  int32_t _M0L6_2atmpS2439;
  #line 57 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS223 = *(uint32_t*)&_M0L5radixS224;
  _M0L6_2atmpS2440 = _M0L5radixS224 - 1;
  _M0L6_2atmpS2439 = _M0L5radixS224 & _M0L6_2atmpS2440;
  if (_M0L6_2atmpS2439 == 0) {
    int32_t _M0L5shiftS225;
    uint32_t _M0L4maskS226;
    int32_t _M0L6_2atmpS2447;
    int32_t _M0L6offsetS227;
    uint32_t _M0L1nS228;
    #line 68 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS225 = moonbit_ctz32(_M0L5radixS224);
    _M0L4maskS226 = _M0L4baseS223 - 1u;
    _M0L6_2atmpS2447 = _M0L10total__lenS233 - _M0L12digit__startS231;
    _M0L6offsetS227 = _M0L6_2atmpS2447;
    _M0L1nS228 = _M0L3numS234;
    while (1) {
      if (_M0L1nS228 > 0u) {
        uint32_t _M0L6_2atmpS2446 = _M0L1nS228 & _M0L4maskS226;
        int32_t _M0L5digitS229 = *(int32_t*)&_M0L6_2atmpS2446;
        int32_t _M0L6_2atmpS2443 = _M0L12digit__startS231 + _M0L6offsetS227;
        int32_t _M0L6_2atmpS2441 = _M0L6_2atmpS2443 - 1;
        int32_t _M0L6_2atmpS2442 =
          ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L5digitS229];
        int32_t _M0L6_2atmpS2444;
        uint32_t _M0L6_2atmpS2445;
        _M0L6bufferS230[_M0L6_2atmpS2441] = _M0L6_2atmpS2442;
        _M0L6_2atmpS2444 = _M0L6offsetS227 - 1;
        _M0L6_2atmpS2445 = _M0L1nS228 >> (_M0L5shiftS225 & 31);
        _M0L6offsetS227 = _M0L6_2atmpS2444;
        _M0L1nS228 = _M0L6_2atmpS2445;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2454 = _M0L10total__lenS233 - _M0L12digit__startS231;
    int32_t _M0L6offsetS235 = _M0L6_2atmpS2454;
    uint32_t _M0L1nS236 = _M0L3numS234;
    while (1) {
      if (_M0L1nS236 > 0u) {
        uint32_t _M0L1qS237 = _M0L1nS236 / _M0L4baseS223;
        uint32_t _M0L6_2atmpS2453 = _M0L1qS237 * _M0L4baseS223;
        uint32_t _M0L6_2atmpS2452 = _M0L1nS236 - _M0L6_2atmpS2453;
        int32_t _M0L5digitS238 = *(int32_t*)&_M0L6_2atmpS2452;
        int32_t _M0L6_2atmpS2450 = _M0L12digit__startS231 + _M0L6offsetS235;
        int32_t _M0L6_2atmpS2448 = _M0L6_2atmpS2450 - 1;
        int32_t _M0L6_2atmpS2449 =
          ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L5digitS238];
        int32_t _M0L6_2atmpS2451;
        _M0L6bufferS230[_M0L6_2atmpS2448] = _M0L6_2atmpS2449;
        _M0L6_2atmpS2451 = _M0L6offsetS235 - 1;
        _M0L6offsetS235 = _M0L6_2atmpS2451;
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
  int32_t _M0L6_2atmpS2438;
  int32_t _M0L6offsetS212;
  uint32_t _M0L1nS213;
  #line 29 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2438 = _M0L10total__lenS221 - _M0L12digit__startS218;
  _M0L6offsetS212 = _M0L6_2atmpS2438;
  _M0L1nS213 = _M0L3numS222;
  while (1) {
    if (_M0L6offsetS212 >= 2) {
      uint32_t _M0L6_2atmpS2435 = _M0L1nS213 & 255u;
      int32_t _M0L9byte__valS214 = *(int32_t*)&_M0L6_2atmpS2435;
      int32_t _M0L2hiS215 = _M0L9byte__valS214 / 16;
      int32_t _M0L2loS216 = _M0L9byte__valS214 % 16;
      int32_t _M0L6_2atmpS2429 = _M0L12digit__startS218 + _M0L6offsetS212;
      int32_t _M0L6_2atmpS2427 = _M0L6_2atmpS2429 - 2;
      int32_t _M0L6_2atmpS2428 =
        ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L2hiS215];
      int32_t _M0L6_2atmpS2432;
      int32_t _M0L6_2atmpS2430;
      int32_t _M0L6_2atmpS2431;
      int32_t _M0L6_2atmpS2433;
      uint32_t _M0L6_2atmpS2434;
      _M0L6bufferS217[_M0L6_2atmpS2427] = _M0L6_2atmpS2428;
      _M0L6_2atmpS2432 = _M0L12digit__startS218 + _M0L6offsetS212;
      _M0L6_2atmpS2430 = _M0L6_2atmpS2432 - 1;
      _M0L6_2atmpS2431
      = ((moonbit_string_t)moonbit_string_literal_104.data)[
        _M0L2loS216
      ];
      _M0L6bufferS217[_M0L6_2atmpS2430] = _M0L6_2atmpS2431;
      _M0L6_2atmpS2433 = _M0L6offsetS212 - 2;
      _M0L6_2atmpS2434 = _M0L1nS213 >> 8;
      _M0L6offsetS212 = _M0L6_2atmpS2433;
      _M0L1nS213 = _M0L6_2atmpS2434;
      continue;
    } else if (_M0L6offsetS212 == 1) {
      uint32_t _M0L6_2atmpS2437 = _M0L1nS213 & 15u;
      int32_t _M0L6nibbleS220 = *(int32_t*)&_M0L6_2atmpS2437;
      int32_t _M0L6_2atmpS2436 =
        ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L6nibbleS220];
      _M0L6bufferS217[_M0L12digit__startS218] = _M0L6_2atmpS2436;
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
    int64_t _M0L6_2atmpS2419;
    if (_M0L4_2anS193 > 0) {
      int32_t _M0L6_2atmpS2420 = _M0L4_2anS193 - 1;
      _M0L6_2atmpS2419 = (int64_t)_M0L6_2atmpS2420;
    } else {
      _M0L6_2atmpS2419 = _M0MPB4Iter4nextN6constrS9980GUssEE;
    }
    _M0L4selfS189->$1 = _M0L6_2atmpS2419;
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
    int64_t _M0L6_2atmpS2421;
    if (_M0L4_2anS199 > 0) {
      int32_t _M0L6_2atmpS2422 = _M0L4_2anS199 - 1;
      _M0L6_2atmpS2421 = (int64_t)_M0L6_2atmpS2422;
    } else {
      _M0L6_2atmpS2421 = _M0MPB4Iter4nextN6constrS9980GUsbEE;
    }
    _M0L4selfS195->$1 = _M0L6_2atmpS2421;
  }
  return _M0L6resultS196;
}

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB4Iter4nextGUsRP19moonbitDB10RedisValueEE(
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L4selfS201
) {
  struct _M0TWEOUsRP19moonbitDB10RedisValueE* _M0L7_2afuncS200;
  struct _M0TUsRP19moonbitDB10RedisValueE* _M0L6resultS202;
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
    = _M0MPB4Iter4nextN6constrS9981GUsRP19moonbitDB10RedisValueEE;
  } else if (_M0L7_2abindS203 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS204 = _M0L7_2abindS203;
    int32_t _M0L4_2anS205 = (int32_t)_M0L7_2aSomeS204;
    int64_t _M0L6_2atmpS2423;
    if (_M0L4_2anS205 > 0) {
      int32_t _M0L6_2atmpS2424 = _M0L4_2anS205 - 1;
      _M0L6_2atmpS2423 = (int64_t)_M0L6_2atmpS2424;
    } else {
      _M0L6_2atmpS2423
      = _M0MPB4Iter4nextN6constrS9980GUsRP19moonbitDB10RedisValueEE;
    }
    _M0L4selfS201->$1 = _M0L6_2atmpS2423;
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
    int64_t _M0L6_2atmpS2425;
    if (_M0L4_2anS211 > 0) {
      int32_t _M0L6_2atmpS2426 = _M0L4_2anS211 - 1;
      _M0L6_2atmpS2425 = (int64_t)_M0L6_2atmpS2426;
    } else {
      _M0L6_2atmpS2425 = _M0MPB4Iter4nextN6constrS9980GUsfEE;
    }
    _M0L4selfS207->$1 = _M0L6_2atmpS2425;
  }
  return _M0L6resultS208;
}

int32_t _M0IP016_24default__implPB4Show6outputGsE(
  moonbit_string_t _M0L4selfS179,
  struct _M0TPB6Logger _M0L6loggerS178
) {
  moonbit_string_t _M0L6_2atmpS2414;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2414 = _M0IPC16string6StringPB4Show10to__string(_M0L4selfS179);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS178.$0->$method_0(_M0L6loggerS178.$1, _M0L6_2atmpS2414);
  moonbit_decref(_M0L6_2atmpS2414);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGiE(
  int32_t _M0L4selfS181,
  struct _M0TPB6Logger _M0L6loggerS180
) {
  moonbit_string_t _M0L6_2atmpS2415;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2415 = _M0IPC13int3IntPB4Show10to__string(_M0L4selfS181);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS180.$0->$method_0(_M0L6loggerS180.$1, _M0L6_2atmpS2415);
  moonbit_decref(_M0L6_2atmpS2415);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGbE(
  int32_t _M0L4selfS183,
  struct _M0TPB6Logger _M0L6loggerS182
) {
  moonbit_string_t _M0L6_2atmpS2416;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2416 = _M0IPC14bool4BoolPB4Show10to__string(_M0L4selfS183);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS182.$0->$method_0(_M0L6loggerS182.$1, _M0L6_2atmpS2416);
  moonbit_decref(_M0L6_2atmpS2416);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGfE(
  float _M0L4selfS185,
  struct _M0TPB6Logger _M0L6loggerS184
) {
  moonbit_string_t _M0L6_2atmpS2417;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2417 = _M0IPC15float5FloatPB4Show10to__string(_M0L4selfS185);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS184.$0->$method_0(_M0L6loggerS184.$1, _M0L6_2atmpS2417);
  moonbit_decref(_M0L6_2atmpS2417);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGmE(
  uint64_t _M0L4selfS187,
  struct _M0TPB6Logger _M0L6loggerS186
) {
  moonbit_string_t _M0L6_2atmpS2418;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2418 = _M0IPC16uint646UInt64PB4Show10to__string(_M0L4selfS187);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS186.$0->$method_0(_M0L6loggerS186.$1, _M0L6_2atmpS2418);
  moonbit_decref(_M0L6_2atmpS2418);
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
  moonbit_string_t _M0L8_2afieldS4344;
  #line 92 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L8_2afieldS4344 = _M0L4selfS176.$0;
  moonbit_incref(_M0L8_2afieldS4344);
  return _M0L8_2afieldS4344;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS172,
  moonbit_string_t _M0L5valueS173,
  int32_t _M0L5startS174,
  int32_t _M0L3lenS175
) {
  int32_t _M0L6_2atmpS2413;
  int64_t _M0L6_2atmpS2412;
  struct _M0TPC16string10StringView _M0L6_2atmpS2411;
  #line 122 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2413 = _M0L5startS174 + _M0L3lenS175;
  _M0L6_2atmpS2412 = (int64_t)_M0L6_2atmpS2413;
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2411
  = _M0MPC16string6String11sub_2einner(_M0L5valueS173, _M0L5startS174, _M0L6_2atmpS2412);
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS172, _M0L6_2atmpS2411);
  moonbit_decref(_M0L6_2atmpS2411.$0);
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
  int32_t _if__result_4775;
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
      _if__result_4775 = _M0L3endS166 <= _M0L3lenS164;
    } else {
      _if__result_4775 = 0;
    }
  } else {
    _if__result_4775 = 0;
  }
  if (_if__result_4775) {
    if (_M0L5startS170 < _M0L3lenS164) {
      int32_t _M0L6_2atmpS2408 = _M0L4selfS165[_M0L5startS170];
      int32_t _M0L6_2atmpS2407;
      #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2407
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2408);
      if (!_M0L6_2atmpS2407) {
        
      } else {
        #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS166 < _M0L3lenS164) {
      int32_t _M0L6_2atmpS2410 = _M0L4selfS165[_M0L3endS166];
      int32_t _M0L6_2atmpS2409;
      #line 768 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2409
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2410);
      if (!_M0L6_2atmpS2409) {
        
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
  struct _M0TPB6Logger _M0L6_2atmpS2406;
  #line 116 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS163);
  _M0L6_2atmpS2406
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS163
  };
  #line 117 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS162.$0->$method_0(_M0L4showS162.$1, _M0L6_2atmpS2406);
  if (_M0L6_2atmpS2406.$1) {
    moonbit_decref(_M0L6_2atmpS2406.$1);
  }
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS161,
  struct _M0TPB4Show _M0L4showS160
) {
  struct _M0TPB6Logger _M0L6_2atmpS2405;
  #line 111 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS161);
  _M0L6_2atmpS2405
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS161
  };
  #line 112 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS160.$0->$method_0(_M0L4showS160.$1, _M0L6_2atmpS2405);
  if (_M0L6_2atmpS2405.$1) {
    moonbit_decref(_M0L6_2atmpS2405.$1);
  }
  return 0;
}

int32_t _M0FPB13finalize__acc(uint32_t _M0L3accS159) {
  uint32_t _M0L6_2atmpS2404;
  #line 444 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  #line 445 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2404 = _M0FPB14avalanche__acc(_M0L3accS159);
  return *(int32_t*)&_M0L6_2atmpS2404;
}

uint32_t _M0FPB14avalanche__acc(uint32_t _M0L3accS158) {
  uint32_t _M0Lm3accS157;
  uint32_t _M0L6_2atmpS2393;
  uint32_t _M0L6_2atmpS2395;
  uint32_t _M0L6_2atmpS2394;
  uint32_t _M0L6_2atmpS2396;
  uint32_t _M0L6_2atmpS2397;
  uint32_t _M0L6_2atmpS2399;
  uint32_t _M0L6_2atmpS2398;
  uint32_t _M0L6_2atmpS2400;
  uint32_t _M0L6_2atmpS2401;
  uint32_t _M0L6_2atmpS2403;
  uint32_t _M0L6_2atmpS2402;
  #line 449 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0Lm3accS157 = _M0L3accS158;
  _M0L6_2atmpS2393 = _M0Lm3accS157;
  _M0L6_2atmpS2395 = _M0Lm3accS157;
  _M0L6_2atmpS2394 = _M0L6_2atmpS2395 >> 15;
  _M0Lm3accS157 = _M0L6_2atmpS2393 ^ _M0L6_2atmpS2394;
  _M0L6_2atmpS2396 = _M0Lm3accS157;
  _M0Lm3accS157 = _M0L6_2atmpS2396 * 2246822519u;
  _M0L6_2atmpS2397 = _M0Lm3accS157;
  _M0L6_2atmpS2399 = _M0Lm3accS157;
  _M0L6_2atmpS2398 = _M0L6_2atmpS2399 >> 13;
  _M0Lm3accS157 = _M0L6_2atmpS2397 ^ _M0L6_2atmpS2398;
  _M0L6_2atmpS2400 = _M0Lm3accS157;
  _M0Lm3accS157 = _M0L6_2atmpS2400 * 3266489917u;
  _M0L6_2atmpS2401 = _M0Lm3accS157;
  _M0L6_2atmpS2403 = _M0Lm3accS157;
  _M0L6_2atmpS2402 = _M0L6_2atmpS2403 >> 16;
  _M0Lm3accS157 = _M0L6_2atmpS2401 ^ _M0L6_2atmpS2402;
  return _M0Lm3accS157;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS156) {
  int64_t _M0L6_2atmpS2392;
  #line 907 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2392 = (int64_t)_M0L4selfS156;
  return *(uint64_t*)&_M0L6_2atmpS2392;
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS155,
  moonbit_string_t _M0L3strS154
) {
  int32_t _M0L8str__lenS153;
  int32_t _M0L3lenS2387;
  int32_t _M0L6_2atmpS2386;
  uint16_t* _M0L4dataS2388;
  int32_t _M0L3lenS2389;
  int32_t _M0L3lenS2391;
  int32_t _M0L6_2atmpS2390;
  #line 86 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS153 = Moonbit_array_length(_M0L3strS154);
  _M0L3lenS2387 = _M0L4selfS155->$1;
  _M0L6_2atmpS2386 = _M0L3lenS2387 + _M0L8str__lenS153;
  #line 88 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS155, _M0L6_2atmpS2386);
  _M0L4dataS2388 = _M0L4selfS155->$0;
  _M0L3lenS2389 = _M0L4selfS155->$1;
  moonbit_incref(_M0L4dataS2388);
  #line 89 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2388, _M0L3lenS2389, _M0L3strS154, 0, _M0L8str__lenS153);
  moonbit_decref(_M0L4dataS2388);
  _M0L3lenS2391 = _M0L4selfS155->$1;
  _M0L6_2atmpS2390 = _M0L3lenS2391 + _M0L8str__lenS153;
  _M0L4selfS155->$1 = _M0L6_2atmpS2390;
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
      int32_t _M0L6_2atmpS2383 = _M0L3strS150[_M0L1iS147];
      int32_t _M0L6_2atmpS2384;
      int32_t _M0L6_2atmpS2385;
      if (
        _M0L1jS148 < 0 || _M0L1jS148 >= Moonbit_array_length(_M0L4selfS149)
      ) {
        #line 80 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
        moonbit_panic();
      }
      _M0L4selfS149[_M0L1jS148] = _M0L6_2atmpS2383;
      _M0L6_2atmpS2384 = _M0L1iS147 + 1;
      _M0L6_2atmpS2385 = _M0L1jS148 + 1;
      _M0L1iS147 = _M0L6_2atmpS2384;
      _M0L1jS148 = _M0L6_2atmpS2385;
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
    int32_t _M0L3lenS2362 = _M0L4selfS141->$1;
    int32_t _M0L6_2atmpS2361 = _M0L3lenS2362 + 1;
    uint16_t* _M0L4dataS2363;
    int32_t _M0L3lenS2364;
    int32_t _M0L6_2atmpS2365;
    int32_t _M0L3lenS2367;
    int32_t _M0L6_2atmpS2366;
    #line 98 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS141, _M0L6_2atmpS2361);
    _M0L4dataS2363 = _M0L4selfS141->$0;
    _M0L3lenS2364 = _M0L4selfS141->$1;
    moonbit_incref(_M0L4dataS2363);
    #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2365 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS139);
    if (
      _M0L3lenS2364 < 0
      || _M0L3lenS2364 >= Moonbit_array_length(_M0L4dataS2363)
    ) {
      #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2363[_M0L3lenS2364] = _M0L6_2atmpS2365;
    moonbit_decref(_M0L4dataS2363);
    _M0L3lenS2367 = _M0L4selfS141->$1;
    _M0L6_2atmpS2366 = _M0L3lenS2367 + 1;
    _M0L4selfS141->$1 = _M0L6_2atmpS2366;
  } else if (_M0L4codeS139 <= 1114111u) {
    int32_t _M0L3lenS2369 = _M0L4selfS141->$1;
    int32_t _M0L6_2atmpS2368 = _M0L3lenS2369 + 2;
    uint32_t _M0L4codeS142;
    uint16_t* _M0L4dataS2370;
    int32_t _M0L3lenS2371;
    uint32_t _M0L6_2atmpS2374;
    uint32_t _M0L6_2atmpS2373;
    int32_t _M0L6_2atmpS2372;
    uint16_t* _M0L4dataS2375;
    int32_t _M0L3lenS2380;
    int32_t _M0L6_2atmpS2376;
    uint32_t _M0L6_2atmpS2379;
    uint32_t _M0L6_2atmpS2378;
    int32_t _M0L6_2atmpS2377;
    int32_t _M0L3lenS2382;
    int32_t _M0L6_2atmpS2381;
    #line 102 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS141, _M0L6_2atmpS2368);
    _M0L4codeS142 = _M0L4codeS139 - 65536u;
    _M0L4dataS2370 = _M0L4selfS141->$0;
    _M0L3lenS2371 = _M0L4selfS141->$1;
    _M0L6_2atmpS2374 = _M0L4codeS142 >> 10;
    _M0L6_2atmpS2373 = 55296u + _M0L6_2atmpS2374;
    moonbit_incref(_M0L4dataS2370);
    #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2372 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2373);
    if (
      _M0L3lenS2371 < 0
      || _M0L3lenS2371 >= Moonbit_array_length(_M0L4dataS2370)
    ) {
      #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2370[_M0L3lenS2371] = _M0L6_2atmpS2372;
    moonbit_decref(_M0L4dataS2370);
    _M0L4dataS2375 = _M0L4selfS141->$0;
    _M0L3lenS2380 = _M0L4selfS141->$1;
    _M0L6_2atmpS2376 = _M0L3lenS2380 + 1;
    _M0L6_2atmpS2379 = _M0L4codeS142 & 1023u;
    _M0L6_2atmpS2378 = 56320u + _M0L6_2atmpS2379;
    moonbit_incref(_M0L4dataS2375);
    #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2377 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2378);
    if (
      _M0L6_2atmpS2376 < 0
      || _M0L6_2atmpS2376 >= Moonbit_array_length(_M0L4dataS2375)
    ) {
      #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2375[_M0L6_2atmpS2376] = _M0L6_2atmpS2377;
    moonbit_decref(_M0L4dataS2375);
    _M0L3lenS2382 = _M0L4selfS141->$1;
    _M0L6_2atmpS2381 = _M0L3lenS2382 + 2;
    _M0L4selfS141->$1 = _M0L6_2atmpS2381;
  } else {
    #line 108 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_105.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS133,
  int32_t _M0L8requiredS134
) {
  uint16_t* _M0L4dataS2360;
  int32_t _M0L12current__lenS132;
  int32_t _M0L13enough__spaceS135;
  int32_t _M0L13enough__spaceS136;
  uint16_t* _M0L4dataS2356;
  int32_t _M0L6_2atmpS2357;
  int32_t _M0L3lenS2358;
  uint16_t* _M0L9new__dataS138;
  uint16_t* _M0L6_2aoldS4349;
  #line 46 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS2360 = _M0L4selfS133->$0;
  _M0L12current__lenS132 = Moonbit_array_length(_M0L4dataS2360);
  if (_M0L8requiredS134 <= _M0L12current__lenS132) {
    return 0;
  }
  _M0L13enough__spaceS136 = _M0L12current__lenS132;
  while (1) {
    if (_M0L13enough__spaceS136 < _M0L8requiredS134) {
      int32_t _M0L6_2atmpS2359 = _M0L13enough__spaceS136 * 2;
      _M0L13enough__spaceS136 = _M0L6_2atmpS2359;
      continue;
    } else {
      _M0L13enough__spaceS135 = _M0L13enough__spaceS136;
    }
    break;
  }
  _M0L4dataS2356 = _M0L4selfS133->$0;
  moonbit_incref(_M0L4dataS2356);
  #line 64 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2357 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L3lenS2358 = _M0L4selfS133->$1;
  #line 61 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L9new__dataS138
  = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS2356, _M0L13enough__spaceS135, _M0L6_2atmpS2357, _M0L3lenS2358, 0, 0);
  moonbit_decref(_M0L4dataS2356);
  _M0L6_2aoldS4349 = _M0L4selfS133->$0;
  moonbit_decref(_M0L6_2aoldS4349);
  _M0L4selfS133->$0 = _M0L9new__dataS138;
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS131) {
  int32_t _M0L6_2atmpS2355;
  #line 2676 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2355 = *(int32_t*)&_M0L4selfS131;
  return (uint16_t)_M0L6_2atmpS2355;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS130) {
  int32_t _M0L6_2atmpS2354;
  #line 1254 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2354 = _M0L4selfS130;
  return *(uint32_t*)&_M0L6_2atmpS2354;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS128
) {
  int32_t _M0L3lenS2346;
  uint16_t* _M0L4dataS2348;
  int32_t _M0L6_2atmpS2347;
  #line 148 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS2346 = _M0L4selfS128->$1;
  _M0L4dataS2348 = _M0L4selfS128->$0;
  _M0L6_2atmpS2347 = Moonbit_array_length(_M0L4dataS2348);
  if (_M0L3lenS2346 == _M0L6_2atmpS2347) {
    uint16_t* _M0L4dataS2349 = _M0L4selfS128->$0;
    moonbit_incref(_M0L4dataS2349);
    return _M0L4dataS2349;
  } else {
    uint16_t* _M0L4dataS2350 = _M0L4selfS128->$0;
    int32_t _M0L3lenS2351 = _M0L4selfS128->$1;
    int32_t _M0L6_2atmpS2352;
    int32_t _M0L3lenS2353;
    uint16_t* _M0L4dataS129;
    moonbit_incref(_M0L4dataS2350);
    #line 155 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2352 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L3lenS2353 = _M0L4selfS128->$1;
    #line 152 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L4dataS129
    = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS2350, _M0L3lenS2351, _M0L6_2atmpS2352, _M0L3lenS2353, 0, 0);
    moonbit_decref(_M0L4dataS2350);
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
  int32_t _if__result_4778;
  #line 97 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L13allocate__lenS121 >= 0) {
    if (_M0L3lenS122 >= 0) {
      if (_M0L11src__offsetS123 >= 0) {
        if (_M0L11dst__offsetS124 >= 0) {
          int32_t _M0L6_2atmpS2342 = _M0L11src__offsetS123 + _M0L3lenS122;
          int32_t _M0L6_2atmpS2343 = Moonbit_array_length(_M0L3srcS125);
          if (_M0L6_2atmpS2342 <= _M0L6_2atmpS2343) {
            int32_t _M0L6_2atmpS2341 = _M0L11dst__offsetS124 + _M0L3lenS122;
            _if__result_4778 = _M0L6_2atmpS2341 <= _M0L13allocate__lenS121;
          } else {
            _if__result_4778 = 0;
          }
        } else {
          _if__result_4778 = 0;
        }
      } else {
        _if__result_4778 = 0;
      }
    } else {
      _if__result_4778 = 0;
    }
  } else {
    _if__result_4778 = 0;
  }
  if (_if__result_4778) {
    moonbit_incref(_M0L3srcS125);
    #line 115 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    return _M0MPC15array10FixedArray23unsafe__make__and__blitGkE(_M0L3srcS125, _M0L13allocate__lenS121, _M0L4initS126, _M0L11src__offsetS123, _M0L11dst__offsetS124, _M0L3lenS122);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS127;
    int32_t _M0L6_2atmpS2345;
    moonbit_string_t _M0L6_2atmpS2344;
    uint16_t* _result_4779;
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L18_2astring__builderS127
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_106.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L13allocate__lenS121);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_107.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L11src__offsetS123);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_108.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L11dst__offsetS124);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_109.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L3lenS122);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_110.data);
    _M0L6_2atmpS2345 = Moonbit_array_length(_M0L3srcS125);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L6_2atmpS2345);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L6_2atmpS2344
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS127);
    moonbit_decref(_M0L18_2astring__builderS127);
    #line 111 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _result_4779 = _M0FPC15abort5abortGAkE(_M0L6_2atmpS2344);
    moonbit_decref(_M0L6_2atmpS2344);
    return _result_4779;
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
  struct _M0TPB13StringBuilder* _block_4780;
  #line 32 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS112 < 1) {
    _M0L7initialS111 = 1;
  } else {
    int32_t _M0L6_2atmpS2340 = _M0L10size__hintS112 + 1;
    _M0L7initialS111 = _M0L6_2atmpS2340 / 2;
  }
  _M0L4dataS113 = (uint16_t*)moonbit_make_string(_M0L7initialS111, 0);
  _block_4780
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4780)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 132, 0);
  _block_4780->$0 = _M0L4dataS113;
  _block_4780->$1 = 0;
  return _block_4780;
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(
  moonbit_string_t* _M0L3srcS97,
  int32_t _M0L13allocate__lenS93,
  int32_t _M0L3lenS94,
  int32_t _M0L11src__offsetS95,
  int32_t _M0L11dst__offsetS96
) {
  int32_t _if__result_4781;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS93 >= 0) {
    if (_M0L3lenS94 >= 0) {
      if (_M0L11src__offsetS95 >= 0) {
        if (_M0L11dst__offsetS96 >= 0) {
          int32_t _M0L6_2atmpS2326 = _M0L11src__offsetS95 + _M0L3lenS94;
          int32_t _M0L6_2atmpS2327;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2327
          = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS97);
          if (_M0L6_2atmpS2326 <= _M0L6_2atmpS2327) {
            int32_t _M0L6_2atmpS2325 = _M0L11dst__offsetS96 + _M0L3lenS94;
            _if__result_4781 = _M0L6_2atmpS2325 <= _M0L13allocate__lenS93;
          } else {
            _if__result_4781 = 0;
          }
        } else {
          _if__result_4781 = 0;
        }
      } else {
        _if__result_4781 = 0;
      }
    } else {
      _if__result_4781 = 0;
    }
  } else {
    _if__result_4781 = 0;
  }
  if (_if__result_4781) {
    moonbit_incref(_M0L3srcS97);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS93, (moonbit_string_t)moonbit_string_literal_96.data, _M0L3srcS97, _M0L11src__offsetS95, _M0L11dst__offsetS96, _M0L3lenS94);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS98;
    int32_t _M0L6_2atmpS2329;
    moonbit_string_t _M0L6_2atmpS2328;
    moonbit_string_t* _result_4782;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS98
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_106.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L13allocate__lenS93);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_107.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L11src__offsetS95);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_108.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L11dst__offsetS96);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_109.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L3lenS94);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_110.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2329 = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS97);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L6_2atmpS2329);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2328
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS98);
    moonbit_decref(_M0L18_2astring__builderS98);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4782
    = _M0FPC15abort5abortGRPB18UninitializedArrayGsEE(_M0L6_2atmpS2328);
    moonbit_decref(_M0L6_2atmpS2328);
    return _result_4782;
  }
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGOsE(
  moonbit_string_t* _M0L3srcS103,
  int32_t _M0L13allocate__lenS99,
  int32_t _M0L3lenS100,
  int32_t _M0L11src__offsetS101,
  int32_t _M0L11dst__offsetS102
) {
  int32_t _if__result_4783;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS99 >= 0) {
    if (_M0L3lenS100 >= 0) {
      if (_M0L11src__offsetS101 >= 0) {
        if (_M0L11dst__offsetS102 >= 0) {
          int32_t _M0L6_2atmpS2331 = _M0L11src__offsetS101 + _M0L3lenS100;
          int32_t _M0L6_2atmpS2332;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2332
          = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS103);
          if (_M0L6_2atmpS2331 <= _M0L6_2atmpS2332) {
            int32_t _M0L6_2atmpS2330 = _M0L11dst__offsetS102 + _M0L3lenS100;
            _if__result_4783 = _M0L6_2atmpS2330 <= _M0L13allocate__lenS99;
          } else {
            _if__result_4783 = 0;
          }
        } else {
          _if__result_4783 = 0;
        }
      } else {
        _if__result_4783 = 0;
      }
    } else {
      _if__result_4783 = 0;
    }
  } else {
    _if__result_4783 = 0;
  }
  if (_if__result_4783) {
    moonbit_incref(_M0L3srcS103);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS99, 0, _M0L3srcS103, _M0L11src__offsetS101, _M0L11dst__offsetS102, _M0L3lenS100);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS104;
    int32_t _M0L6_2atmpS2334;
    moonbit_string_t _M0L6_2atmpS2333;
    moonbit_string_t* _result_4784;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS104
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_106.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L13allocate__lenS99);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_107.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L11src__offsetS101);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_108.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L11dst__offsetS102);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_109.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L3lenS100);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_110.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2334 = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS103);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L6_2atmpS2334);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2333
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS104);
    moonbit_decref(_M0L18_2astring__builderS104);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4784
    = _M0FPC15abort5abortGRPB18UninitializedArrayGOsEE(_M0L6_2atmpS2333);
    moonbit_decref(_M0L6_2atmpS2333);
    return _result_4784;
  }
}

struct _M0TUsfE** _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(
  struct _M0TUsfE** _M0L3srcS109,
  int32_t _M0L13allocate__lenS105,
  int32_t _M0L3lenS106,
  int32_t _M0L11src__offsetS107,
  int32_t _M0L11dst__offsetS108
) {
  int32_t _if__result_4785;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS105 >= 0) {
    if (_M0L3lenS106 >= 0) {
      if (_M0L11src__offsetS107 >= 0) {
        if (_M0L11dst__offsetS108 >= 0) {
          int32_t _M0L6_2atmpS2336 = _M0L11src__offsetS107 + _M0L3lenS106;
          int32_t _M0L6_2atmpS2337;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2337
          = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS109);
          if (_M0L6_2atmpS2336 <= _M0L6_2atmpS2337) {
            int32_t _M0L6_2atmpS2335 = _M0L11dst__offsetS108 + _M0L3lenS106;
            _if__result_4785 = _M0L6_2atmpS2335 <= _M0L13allocate__lenS105;
          } else {
            _if__result_4785 = 0;
          }
        } else {
          _if__result_4785 = 0;
        }
      } else {
        _if__result_4785 = 0;
      }
    } else {
      _if__result_4785 = 0;
    }
  } else {
    _if__result_4785 = 0;
  }
  if (_if__result_4785) {
    moonbit_incref(_M0L3srcS109);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (struct _M0TUsfE**)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS105, 0, _M0L3srcS109, _M0L11src__offsetS107, _M0L11dst__offsetS108, _M0L3lenS106);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS110;
    int32_t _M0L6_2atmpS2339;
    moonbit_string_t _M0L6_2atmpS2338;
    struct _M0TUsfE** _result_4786;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS110
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_106.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L13allocate__lenS105);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_107.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L11src__offsetS107);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_108.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L11dst__offsetS108);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_109.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L3lenS106);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_110.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2339 = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS109);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L6_2atmpS2339);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2338
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS110);
    moonbit_decref(_M0L18_2astring__builderS110);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4786
    = _M0FPC15abort5abortGRPB18UninitializedArrayGUsfEEE(_M0L6_2atmpS2338);
    moonbit_decref(_M0L6_2atmpS2338);
    return _result_4786;
  }
}

int32_t _M0MPB13StringBuilder13write__objectGsE(
  struct _M0TPB13StringBuilder* _M0L4selfS84,
  moonbit_string_t _M0L3objS83
) {
  struct _M0TPB6Logger _M0L6_2atmpS2320;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS84);
  _M0L6_2atmpS2320
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS84
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGsE(_M0L3objS83, _M0L6_2atmpS2320);
  if (_M0L6_2atmpS2320.$1) {
    moonbit_decref(_M0L6_2atmpS2320.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGiE(
  struct _M0TPB13StringBuilder* _M0L4selfS86,
  int32_t _M0L3objS85
) {
  struct _M0TPB6Logger _M0L6_2atmpS2321;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS86);
  _M0L6_2atmpS2321
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS86
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGiE(_M0L3objS85, _M0L6_2atmpS2321);
  if (_M0L6_2atmpS2321.$1) {
    moonbit_decref(_M0L6_2atmpS2321.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGbE(
  struct _M0TPB13StringBuilder* _M0L4selfS88,
  int32_t _M0L3objS87
) {
  struct _M0TPB6Logger _M0L6_2atmpS2322;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS88);
  _M0L6_2atmpS2322
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS88
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGbE(_M0L3objS87, _M0L6_2atmpS2322);
  if (_M0L6_2atmpS2322.$1) {
    moonbit_decref(_M0L6_2atmpS2322.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGfE(
  struct _M0TPB13StringBuilder* _M0L4selfS90,
  float _M0L3objS89
) {
  struct _M0TPB6Logger _M0L6_2atmpS2323;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS90);
  _M0L6_2atmpS2323
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS90
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGfE(_M0L3objS89, _M0L6_2atmpS2323);
  if (_M0L6_2atmpS2323.$1) {
    moonbit_decref(_M0L6_2atmpS2323.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGmE(
  struct _M0TPB13StringBuilder* _M0L4selfS92,
  uint64_t _M0L3objS91
) {
  struct _M0TPB6Logger _M0L6_2atmpS2324;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS92);
  _M0L6_2atmpS2324
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS92
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGmE(_M0L3objS91, _M0L6_2atmpS2324);
  if (_M0L6_2atmpS2324.$1) {
    moonbit_decref(_M0L6_2atmpS2324.$1);
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
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13allocate__lenS66, (moonbit_string_t)moonbit_string_literal_96.data);
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
  int32_t _if__result_4787;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS14 == _M0L3srcS15) {
    _if__result_4787 = _M0L11dst__offsetS16 < _M0L11src__offsetS17;
  } else {
    _if__result_4787 = 0;
  }
  if (_if__result_4787) {
    int32_t _M0L1iS18 = 0;
    while (1) {
      if (_M0L1iS18 < _M0L3lenS19) {
        int32_t _M0L6_2atmpS2284 = _M0L11dst__offsetS16 + _M0L1iS18;
        int32_t _M0L6_2atmpS2286 = _M0L11src__offsetS17 + _M0L1iS18;
        int32_t _M0L6_2atmpS2285;
        int32_t _M0L6_2atmpS2287;
        if (
          _M0L6_2atmpS2286 < 0
          || _M0L6_2atmpS2286 >= Moonbit_array_length(_M0L3srcS15)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2285 = (int32_t)_M0L3srcS15[_M0L6_2atmpS2286];
        if (
          _M0L6_2atmpS2284 < 0
          || _M0L6_2atmpS2284 >= Moonbit_array_length(_M0L3dstS14)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS14[_M0L6_2atmpS2284] = _M0L6_2atmpS2285;
        _M0L6_2atmpS2287 = _M0L1iS18 + 1;
        _M0L1iS18 = _M0L6_2atmpS2287;
        continue;
      } else {
        moonbit_decref(_M0L3srcS15);
        moonbit_decref(_M0L3dstS14);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2292 = _M0L3lenS19 - 1;
    int32_t _M0L1iS21 = _M0L6_2atmpS2292;
    while (1) {
      if (_M0L1iS21 >= 0) {
        int32_t _M0L6_2atmpS2288 = _M0L11dst__offsetS16 + _M0L1iS21;
        int32_t _M0L6_2atmpS2290 = _M0L11src__offsetS17 + _M0L1iS21;
        int32_t _M0L6_2atmpS2289;
        int32_t _M0L6_2atmpS2291;
        if (
          _M0L6_2atmpS2290 < 0
          || _M0L6_2atmpS2290 >= Moonbit_array_length(_M0L3srcS15)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2289 = (int32_t)_M0L3srcS15[_M0L6_2atmpS2290];
        if (
          _M0L6_2atmpS2288 < 0
          || _M0L6_2atmpS2288 >= Moonbit_array_length(_M0L3dstS14)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS14[_M0L6_2atmpS2288] = _M0L6_2atmpS2289;
        _M0L6_2atmpS2291 = _M0L1iS21 - 1;
        _M0L1iS21 = _M0L6_2atmpS2291;
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
  int32_t _if__result_4790;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS23 == _M0L3srcS24) {
    _if__result_4790 = _M0L11dst__offsetS25 < _M0L11src__offsetS26;
  } else {
    _if__result_4790 = 0;
  }
  if (_if__result_4790) {
    int32_t _M0L1iS27 = 0;
    while (1) {
      if (_M0L1iS27 < _M0L3lenS28) {
        int32_t _M0L6_2atmpS2293 = _M0L11dst__offsetS25 + _M0L1iS27;
        int32_t _M0L6_2atmpS2295 = _M0L11src__offsetS26 + _M0L1iS27;
        moonbit_string_t _M0L6_2atmpS2294;
        moonbit_string_t _M0L6_2aoldS4355;
        int32_t _M0L6_2atmpS2296;
        if (
          _M0L6_2atmpS2295 < 0
          || _M0L6_2atmpS2295 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2294 = (moonbit_string_t)_M0L3srcS24[_M0L6_2atmpS2295];
        if (
          _M0L6_2atmpS2293 < 0
          || _M0L6_2atmpS2293 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4355 = (moonbit_string_t)_M0L3dstS23[_M0L6_2atmpS2293];
        moonbit_incref(_M0L6_2atmpS2294);
        moonbit_decref(_M0L6_2aoldS4355);
        _M0L3dstS23[_M0L6_2atmpS2293] = _M0L6_2atmpS2294;
        _M0L6_2atmpS2296 = _M0L1iS27 + 1;
        _M0L1iS27 = _M0L6_2atmpS2296;
        continue;
      } else {
        moonbit_decref(_M0L3srcS24);
        moonbit_decref(_M0L3dstS23);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2301 = _M0L3lenS28 - 1;
    int32_t _M0L1iS30 = _M0L6_2atmpS2301;
    while (1) {
      if (_M0L1iS30 >= 0) {
        int32_t _M0L6_2atmpS2297 = _M0L11dst__offsetS25 + _M0L1iS30;
        int32_t _M0L6_2atmpS2299 = _M0L11src__offsetS26 + _M0L1iS30;
        moonbit_string_t _M0L6_2atmpS2298;
        moonbit_string_t _M0L6_2aoldS4357;
        int32_t _M0L6_2atmpS2300;
        if (
          _M0L6_2atmpS2299 < 0
          || _M0L6_2atmpS2299 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2298 = (moonbit_string_t)_M0L3srcS24[_M0L6_2atmpS2299];
        if (
          _M0L6_2atmpS2297 < 0
          || _M0L6_2atmpS2297 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4357 = (moonbit_string_t)_M0L3dstS23[_M0L6_2atmpS2297];
        moonbit_incref(_M0L6_2atmpS2298);
        moonbit_decref(_M0L6_2aoldS4357);
        _M0L3dstS23[_M0L6_2atmpS2297] = _M0L6_2atmpS2298;
        _M0L6_2atmpS2300 = _M0L1iS30 - 1;
        _M0L1iS30 = _M0L6_2atmpS2300;
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
  int32_t _if__result_4793;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS32 == _M0L3srcS33) {
    _if__result_4793 = _M0L11dst__offsetS34 < _M0L11src__offsetS35;
  } else {
    _if__result_4793 = 0;
  }
  if (_if__result_4793) {
    int32_t _M0L1iS36 = 0;
    while (1) {
      if (_M0L1iS36 < _M0L3lenS37) {
        int32_t _M0L6_2atmpS2302 = _M0L11dst__offsetS34 + _M0L1iS36;
        int32_t _M0L6_2atmpS2304 = _M0L11src__offsetS35 + _M0L1iS36;
        moonbit_string_t _M0L6_2atmpS2303;
        moonbit_string_t _M0L6_2aoldS4359;
        int32_t _M0L6_2atmpS2305;
        if (
          _M0L6_2atmpS2304 < 0
          || _M0L6_2atmpS2304 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2303 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS2304];
        if (
          _M0L6_2atmpS2302 < 0
          || _M0L6_2atmpS2302 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4359 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS2302];
        if (_M0L6_2atmpS2303) {
          moonbit_incref(_M0L6_2atmpS2303);
        }
        if (_M0L6_2aoldS4359) {
          moonbit_decref(_M0L6_2aoldS4359);
        }
        _M0L3dstS32[_M0L6_2atmpS2302] = _M0L6_2atmpS2303;
        _M0L6_2atmpS2305 = _M0L1iS36 + 1;
        _M0L1iS36 = _M0L6_2atmpS2305;
        continue;
      } else {
        moonbit_decref(_M0L3srcS33);
        moonbit_decref(_M0L3dstS32);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2310 = _M0L3lenS37 - 1;
    int32_t _M0L1iS39 = _M0L6_2atmpS2310;
    while (1) {
      if (_M0L1iS39 >= 0) {
        int32_t _M0L6_2atmpS2306 = _M0L11dst__offsetS34 + _M0L1iS39;
        int32_t _M0L6_2atmpS2308 = _M0L11src__offsetS35 + _M0L1iS39;
        moonbit_string_t _M0L6_2atmpS2307;
        moonbit_string_t _M0L6_2aoldS4361;
        int32_t _M0L6_2atmpS2309;
        if (
          _M0L6_2atmpS2308 < 0
          || _M0L6_2atmpS2308 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2307 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS2308];
        if (
          _M0L6_2atmpS2306 < 0
          || _M0L6_2atmpS2306 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4361 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS2306];
        if (_M0L6_2atmpS2307) {
          moonbit_incref(_M0L6_2atmpS2307);
        }
        if (_M0L6_2aoldS4361) {
          moonbit_decref(_M0L6_2aoldS4361);
        }
        _M0L3dstS32[_M0L6_2atmpS2306] = _M0L6_2atmpS2307;
        _M0L6_2atmpS2309 = _M0L1iS39 - 1;
        _M0L1iS39 = _M0L6_2atmpS2309;
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
  int32_t _if__result_4796;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS41 == _M0L3srcS42) {
    _if__result_4796 = _M0L11dst__offsetS43 < _M0L11src__offsetS44;
  } else {
    _if__result_4796 = 0;
  }
  if (_if__result_4796) {
    int32_t _M0L1iS45 = 0;
    while (1) {
      if (_M0L1iS45 < _M0L3lenS46) {
        int32_t _M0L6_2atmpS2311 = _M0L11dst__offsetS43 + _M0L1iS45;
        int32_t _M0L6_2atmpS2313 = _M0L11src__offsetS44 + _M0L1iS45;
        struct _M0TUsfE* _M0L6_2atmpS2312;
        struct _M0TUsfE* _M0L6_2aoldS4363;
        int32_t _M0L6_2atmpS2314;
        if (
          _M0L6_2atmpS2313 < 0
          || _M0L6_2atmpS2313 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2312 = (struct _M0TUsfE*)_M0L3srcS42[_M0L6_2atmpS2313];
        if (
          _M0L6_2atmpS2311 < 0
          || _M0L6_2atmpS2311 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4363 = (struct _M0TUsfE*)_M0L3dstS41[_M0L6_2atmpS2311];
        if (_M0L6_2atmpS2312) {
          moonbit_incref(_M0L6_2atmpS2312);
        }
        if (_M0L6_2aoldS4363) {
          moonbit_decref(_M0L6_2aoldS4363);
        }
        _M0L3dstS41[_M0L6_2atmpS2311] = _M0L6_2atmpS2312;
        _M0L6_2atmpS2314 = _M0L1iS45 + 1;
        _M0L1iS45 = _M0L6_2atmpS2314;
        continue;
      } else {
        moonbit_decref(_M0L3srcS42);
        moonbit_decref(_M0L3dstS41);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2319 = _M0L3lenS46 - 1;
    int32_t _M0L1iS48 = _M0L6_2atmpS2319;
    while (1) {
      if (_M0L1iS48 >= 0) {
        int32_t _M0L6_2atmpS2315 = _M0L11dst__offsetS43 + _M0L1iS48;
        int32_t _M0L6_2atmpS2317 = _M0L11src__offsetS44 + _M0L1iS48;
        struct _M0TUsfE* _M0L6_2atmpS2316;
        struct _M0TUsfE* _M0L6_2aoldS4365;
        int32_t _M0L6_2atmpS2318;
        if (
          _M0L6_2atmpS2317 < 0
          || _M0L6_2atmpS2317 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2316 = (struct _M0TUsfE*)_M0L3srcS42[_M0L6_2atmpS2317];
        if (
          _M0L6_2atmpS2315 < 0
          || _M0L6_2atmpS2315 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4365 = (struct _M0TUsfE*)_M0L3dstS41[_M0L6_2atmpS2315];
        if (_M0L6_2atmpS2316) {
          moonbit_incref(_M0L6_2atmpS2316);
        }
        if (_M0L6_2aoldS4365) {
          moonbit_decref(_M0L6_2aoldS4365);
        }
        _M0L3dstS41[_M0L6_2atmpS2315] = _M0L6_2atmpS2316;
        _M0L6_2atmpS2318 = _M0L1iS48 - 1;
        _M0L1iS48 = _M0L6_2atmpS2318;
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
  uint32_t _M0L6_2atmpS2283;
  uint32_t _M0L6_2atmpS2282;
  uint32_t _M0L6_2atmpS2281;
  #line 465 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2283 = _M0L5inputS10 * 3266489917u;
  _M0L6_2atmpS2282 = _M0L3accS9 + _M0L6_2atmpS2283;
  #line 466 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2281 = _M0FPB4rotl(_M0L6_2atmpS2282, 17);
  return _M0L6_2atmpS2281 * 668265263u;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS7, int32_t _M0L1rS8) {
  uint32_t _M0L6_2atmpS2278;
  int32_t _M0L6_2atmpS2280;
  uint32_t _M0L6_2atmpS2279;
  #line 475 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2278 = _M0L1xS7 << (_M0L1rS8 & 31);
  _M0L6_2atmpS2280 = 32 - _M0L1rS8;
  _M0L6_2atmpS2279 = _M0L1xS7 >> (_M0L6_2atmpS2280 & 31);
  return _M0L6_2atmpS2278 | _M0L6_2atmpS2279;
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
  void* _M0L11_2aobj__ptrS2169,
  struct _M0TPB4Show _M0L8_2aparamS2168
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2167 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2169;
  _M0IP016_24default__implPB6Logger5writeGRPB13StringBuilderE(_M0L7_2aselfS2167, _M0L8_2aparamS2168);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger84write__string__interpolation_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS2166,
  struct _M0TPB4Show _M0L8_2aparamS2165
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2164 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2166;
  _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(_M0L7_2aselfS2164, _M0L8_2aparamS2165);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS2163,
  int32_t _M0L8_2aparamS2162
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2161 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2163;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS2161, _M0L8_2aparamS2162);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS2160,
  struct _M0TPC16string10StringView _M0L8_2aparamS2159
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2158 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2160;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS2158, _M0L8_2aparamS2159);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS2157,
  moonbit_string_t _M0L8_2aparamS2154,
  int32_t _M0L8_2aparamS2155,
  int32_t _M0L8_2aparamS2156
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2153 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2157;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS2153, _M0L8_2aparamS2154, _M0L8_2aparamS2155, _M0L8_2aparamS2156);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS2152,
  moonbit_string_t _M0L8_2aparamS2151
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2150 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2152;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS2150, _M0L8_2aparamS2151);
  return 0;
}

void moonbit_init() {
  moonbit_layout_table = moonbit_layout_table_data;
}

int main(int argc, char** argv) {
  struct _M0TP19moonbitDB8Database* _M0L2dbS2034;
  struct _M0TP39moonbitDB8examples11leaderboard6Player* _M0L6_2atmpS2270;
  struct _M0TP39moonbitDB8examples11leaderboard6Player* _M0L6_2atmpS2271;
  struct _M0TP39moonbitDB8examples11leaderboard6Player* _M0L6_2atmpS2272;
  struct _M0TP39moonbitDB8examples11leaderboard6Player* _M0L6_2atmpS2273;
  struct _M0TP39moonbitDB8examples11leaderboard6Player* _M0L6_2atmpS2274;
  struct _M0TP39moonbitDB8examples11leaderboard6Player* _M0L6_2atmpS2275;
  struct _M0TP39moonbitDB8examples11leaderboard6Player* _M0L6_2atmpS2276;
  struct _M0TP39moonbitDB8examples11leaderboard6Player* _M0L6_2atmpS2277;
  struct _M0TP39moonbitDB8examples11leaderboard6Player** _M0L6_2atmpS2269;
  struct _M0TPB5ArrayGRP39moonbitDB8examples11leaderboard6PlayerE* _M0L7playersS2035;
  int32_t _M0L1iS2036;
  struct _M0TPB5ArrayGsE* _M0L9top5__idsS2044;
  int32_t _M0L1iS2045;
  struct _M0TUsfE* _M0L8_2atupleS2265;
  struct _M0TUsfE* _M0L8_2atupleS2266;
  struct _M0TUsfE* _M0L8_2atupleS2267;
  struct _M0TUsfE* _M0L8_2atupleS2268;
  struct _M0TUsfE** _M0L6_2atmpS2264;
  struct _M0TPB5ArrayGUsfEE* _M0L7updatesS2057;
  int32_t _M0L1iS2058;
  struct _M0TPB5ArrayGsE* _M0L8all__idsS2075;
  int32_t _M0L1iS2076;
  int32_t _M0L15count__200__400S2100;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2101;
  moonbit_string_t _M0L6_2atmpS2208;
  struct _M0TPB5ArrayGsE* _M0L12mid__playersS2102;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2103;
  moonbit_string_t _M0L7_2abindS2104;
  int32_t _M0L6_2atmpS2212;
  struct _M0TPC16string10StringView _M0L6_2atmpS2211;
  moonbit_string_t _M0L6_2atmpS2210;
  moonbit_string_t _M0L6_2atmpS2209;
  int32_t _M0L6_2atmpS2213;
  int32_t _M0L6_2atmpS2214;
  int32_t _M0L6_2atmpS2215;
  int32_t _M0L6_2atmpS2216;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2105;
  int32_t _M0L6_2atmpS2218;
  moonbit_string_t _M0L6_2atmpS2217;
  int32_t _M0L1iS2106;
  int32_t _M0L6_2atmpS2225;
  int32_t _M0L6_2atmpS2226;
  int32_t _M0L6_2atmpS2227;
  int32_t _M0L6_2atmpS2228;
  int32_t _M0L6_2atmpS2229;
  int32_t _M0L6_2atmpS2230;
  moonbit_string_t* _M0L6_2atmpS2263;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2262;
  struct _M0TPB5ArrayGsE* _M0L6mutualS2125;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2126;
  int32_t _M0L6_2atmpS2232;
  moonbit_string_t _M0L6_2atmpS2231;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2127;
  moonbit_string_t _M0L7_2abindS2128;
  int32_t _M0L6_2atmpS2236;
  struct _M0TPC16string10StringView _M0L6_2atmpS2235;
  moonbit_string_t _M0L6_2atmpS2234;
  moonbit_string_t _M0L6_2atmpS2233;
  moonbit_string_t* _M0L6_2atmpS2261;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2260;
  struct _M0TPB5ArrayGsE* _M0L12all__friendsS2129;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2130;
  int32_t _M0L6_2atmpS2238;
  moonbit_string_t _M0L6_2atmpS2237;
  moonbit_string_t* _M0L6_2atmpS2259;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2258;
  struct _M0TPB5ArrayGsE* _M0L8only__p1S2131;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2132;
  moonbit_string_t _M0L7_2abindS2133;
  int32_t _M0L6_2atmpS2242;
  struct _M0TPC16string10StringView _M0L6_2atmpS2241;
  moonbit_string_t _M0L6_2atmpS2240;
  moonbit_string_t _M0L6_2atmpS2239;
  moonbit_string_t* _M0L6_2atmpS2257;
  struct _M0TPB5ArrayGsE* _M0L8messagesS2134;
  int32_t _M0L7_2abindS2135;
  int32_t _M0L2__S2136;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2139;
  int32_t _M0L6_2atmpS2247;
  moonbit_string_t _M0L6_2atmpS2246;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2147;
  int32_t _M0L6_2atmpS2251;
  moonbit_string_t _M0L6_2atmpS2250;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2148;
  int32_t _M0L6_2atmpS2253;
  moonbit_string_t _M0L6_2atmpS2252;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2149;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2256;
  int32_t _M0L6_2atmpS2255;
  moonbit_string_t _M0L6_2atmpS2254;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  #line 18 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L2dbS2034 = _M0MP19moonbitDB8Database3new();
  #line 20 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_111.data);
  #line 21 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_112.data);
  #line 22 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_111.data);
  #line 25 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2270
  = _M0MP39moonbitDB8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_113.data, (moonbit_string_t)moonbit_string_literal_114.data);
  #line 26 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2271
  = _M0MP39moonbitDB8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_115.data, (moonbit_string_t)moonbit_string_literal_116.data);
  #line 27 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2272
  = _M0MP39moonbitDB8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_117.data, (moonbit_string_t)moonbit_string_literal_118.data);
  #line 28 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2273
  = _M0MP39moonbitDB8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_119.data, (moonbit_string_t)moonbit_string_literal_120.data);
  #line 29 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2274
  = _M0MP39moonbitDB8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_121.data, (moonbit_string_t)moonbit_string_literal_122.data);
  #line 30 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2275
  = _M0MP39moonbitDB8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_123.data, (moonbit_string_t)moonbit_string_literal_124.data);
  #line 31 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2276
  = _M0MP39moonbitDB8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_125.data, (moonbit_string_t)moonbit_string_literal_126.data);
  #line 32 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2277
  = _M0MP39moonbitDB8examples11leaderboard6Player3new((moonbit_string_t)moonbit_string_literal_127.data, (moonbit_string_t)moonbit_string_literal_128.data);
  _M0L6_2atmpS2269
  = (struct _M0TP39moonbitDB8examples11leaderboard6Player**)moonbit_make_ref_array_raw(8);
  _M0L6_2atmpS2269[0] = _M0L6_2atmpS2270;
  _M0L6_2atmpS2269[1] = _M0L6_2atmpS2271;
  _M0L6_2atmpS2269[2] = _M0L6_2atmpS2272;
  _M0L6_2atmpS2269[3] = _M0L6_2atmpS2273;
  _M0L6_2atmpS2269[4] = _M0L6_2atmpS2274;
  _M0L6_2atmpS2269[5] = _M0L6_2atmpS2275;
  _M0L6_2atmpS2269[6] = _M0L6_2atmpS2276;
  _M0L6_2atmpS2269[7] = _M0L6_2atmpS2277;
  _M0L7playersS2035
  = (struct _M0TPB5ArrayGRP39moonbitDB8examples11leaderboard6PlayerE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP39moonbitDB8examples11leaderboard6PlayerE));
  Moonbit_object_header(_M0L7playersS2035)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 135, 0);
  _M0L7playersS2035->$0 = _M0L6_2atmpS2269;
  _M0L7playersS2035->$1 = 8;
  #line 35 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_129.data);
  #line 36 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_130.data);
  _M0L1iS2036 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2170;
    #line 37 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
    _M0L6_2atmpS2170
    = _M0MPC15array5Array6lengthGRP39moonbitDB8examples11leaderboard6PlayerE(_M0L7playersS2035);
    if (_M0L1iS2036 < _M0L6_2atmpS2170) {
      struct _M0TP39moonbitDB8examples11leaderboard6Player* _M0L6playerS2037;
      int32_t _M0L6_2atmpS2189;
      int32_t _M0L6_2atmpS2188;
      float _M0L5scoreS2038;
      moonbit_string_t _M0L2idS2172;
      int32_t _M0L6_2atmpS2171;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2039;
      moonbit_string_t _M0L2idS2176;
      moonbit_string_t _M0L6_2atmpS2174;
      moonbit_string_t _M0L4nameS2175;
      int32_t _M0L6_2atmpS2173;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2040;
      moonbit_string_t _M0L2idS2182;
      moonbit_string_t _M0L6_2atmpS2178;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2041;
      int32_t _M0L6_2atmpS2181;
      int32_t _M0L6_2atmpS2180;
      moonbit_string_t _M0L6_2atmpS2179;
      int32_t _M0L6_2atmpS2177;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2042;
      moonbit_string_t _M0L4nameS2184;
      moonbit_string_t _M0L8_2afieldS4371;
      int32_t _M0L6_2acntS4469;
      moonbit_string_t _M0L2idS2185;
      int32_t _M0L6_2atmpS2187;
      int32_t _M0L6_2atmpS2186;
      moonbit_string_t _M0L6_2atmpS2183;
      int32_t _M0L6_2atmpS2190;
      #line 38 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6playerS2037
      = _M0MPC15array5Array2atGRP39moonbitDB8examples11leaderboard6PlayerE(_M0L7playersS2035, _M0L1iS2036);
      _M0L6_2atmpS2189 = _M0L1iS2036 * 100;
      _M0L6_2atmpS2188 = _M0L6_2atmpS2189 + 50;
      _M0L5scoreS2038 = (float)_M0L6_2atmpS2188;
      _M0L2idS2172 = _M0L6playerS2037->$0;
      moonbit_incref(_M0L2idS2172);
      #line 40 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2171
      = _M0MP19moonbitDB8Database4zadd(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_131.data, _M0L5scoreS2038, _M0L2idS2172);
      moonbit_decref(_M0L2idS2172);
      #line 41 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS2039
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 41 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2039, (moonbit_string_t)moonbit_string_literal_132.data);
      _M0L2idS2176 = _M0L6playerS2037->$0;
      moonbit_incref(_M0L2idS2176);
      #line 41 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2039, _M0L2idS2176);
      moonbit_decref(_M0L2idS2176);
      #line 41 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2174
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2039);
      moonbit_decref(_M0L18_2astring__builderS2039);
      _M0L4nameS2175 = _M0L6playerS2037->$1;
      moonbit_incref(_M0L4nameS2175);
      #line 41 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2173
      = _M0MP19moonbitDB8Database4hset(_M0L2dbS2034, _M0L6_2atmpS2174, (moonbit_string_t)moonbit_string_literal_133.data, _M0L4nameS2175);
      moonbit_decref(_M0L6_2atmpS2174);
      moonbit_decref(_M0L4nameS2175);
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS2040
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2040, (moonbit_string_t)moonbit_string_literal_132.data);
      _M0L2idS2182 = _M0L6playerS2037->$0;
      moonbit_incref(_M0L2idS2182);
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2040, _M0L2idS2182);
      moonbit_decref(_M0L2idS2182);
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2178
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2040);
      moonbit_decref(_M0L18_2astring__builderS2040);
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS2041
      = _M0MPB13StringBuilder21StringBuilder_2einner(0);
      _M0L6_2atmpS2181 = _M0L1iS2036 * 100;
      _M0L6_2atmpS2180 = _M0L6_2atmpS2181 + 50;
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2041, _M0L6_2atmpS2180);
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2179
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2041);
      moonbit_decref(_M0L18_2astring__builderS2041);
      #line 42 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2177
      = _M0MP19moonbitDB8Database4hset(_M0L2dbS2034, _M0L6_2atmpS2178, (moonbit_string_t)moonbit_string_literal_134.data, _M0L6_2atmpS2179);
      moonbit_decref(_M0L6_2atmpS2178);
      moonbit_decref(_M0L6_2atmpS2179);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS2042
      = _M0MPB13StringBuilder21StringBuilder_2einner(22);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2042, (moonbit_string_t)moonbit_string_literal_135.data);
      _M0L4nameS2184 = _M0L6playerS2037->$1;
      moonbit_incref(_M0L4nameS2184);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2042, _M0L4nameS2184);
      moonbit_decref(_M0L4nameS2184);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2042, (moonbit_string_t)moonbit_string_literal_136.data);
      _M0L8_2afieldS4371 = _M0L6playerS2037->$0;
      _M0L6_2acntS4469
      = Moonbit_rc_count(Moonbit_object_header(_M0L6playerS2037));
      if (_M0L6_2acntS4469 > 1) {
        int32_t _M0L11_2anew__cntS4471 = _M0L6_2acntS4469 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L6playerS2037), _M0L11_2anew__cntS4471);
        moonbit_incref(_M0L8_2afieldS4371);
      } else if (_M0L6_2acntS4469 == 1) {
        moonbit_string_t _M0L8_2afieldS4470 = _M0L6playerS2037->$1;
        moonbit_decref(_M0L8_2afieldS4470);
        #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        moonbit_free(_M0L6playerS2037);
      }
      _M0L2idS2185 = _M0L8_2afieldS4371;
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2042, _M0L2idS2185);
      moonbit_decref(_M0L2idS2185);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2042, (moonbit_string_t)moonbit_string_literal_137.data);
      _M0L6_2atmpS2187 = _M0L1iS2036 * 100;
      _M0L6_2atmpS2186 = _M0L6_2atmpS2187 + 50;
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2042, _M0L6_2atmpS2186);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2042, (moonbit_string_t)moonbit_string_literal_138.data);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2183
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2042);
      moonbit_decref(_M0L18_2astring__builderS2042);
      #line 43 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2183);
      moonbit_decref(_M0L6_2atmpS2183);
      _M0L6_2atmpS2190 = _M0L1iS2036 + 1;
      _M0L1iS2036 = _M0L6_2atmpS2190;
      continue;
    }
    break;
  }
  #line 46 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_139.data);
  #line 47 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_130.data);
  #line 48 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L9top5__idsS2044
  = _M0MP19moonbitDB8Database9zrevrange(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_131.data, 0, 4);
  #line 49 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_140.data);
  #line 50 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_141.data);
  _M0L1iS2045 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2191;
    #line 51 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
    _M0L6_2atmpS2191 = _M0MPC15array5Array6lengthGsE(_M0L9top5__idsS2044);
    if (_M0L1iS2045 < _M0L6_2atmpS2191) {
      moonbit_string_t _M0L3pidS2046;
      moonbit_string_t _M0L1nS2049;
      moonbit_string_t _M0L5pnameS2047;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2051;
      moonbit_string_t _M0L6_2atmpS2195;
      moonbit_string_t _M0L7_2abindS2050;
      void* _M0L6_2atmpS2194;
      moonbit_string_t _M0L5scoreS2054;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2055;
      int32_t _M0L6_2atmpS2193;
      moonbit_string_t _M0L6_2atmpS2192;
      int32_t _M0L6_2atmpS2196;
      #line 52 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L3pidS2046
      = _M0MPC15array5Array2atGsE(_M0L9top5__idsS2044, _M0L1iS2045);
      #line 53 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS2051
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 53 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2051, (moonbit_string_t)moonbit_string_literal_132.data);
      #line 53 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2051, _M0L3pidS2046);
      #line 53 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2195
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2051);
      moonbit_decref(_M0L18_2astring__builderS2051);
      #line 53 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L7_2abindS2050
      = _M0MP19moonbitDB8Database4hget(_M0L2dbS2034, _M0L6_2atmpS2195, (moonbit_string_t)moonbit_string_literal_133.data);
      moonbit_decref(_M0L6_2atmpS2195);
      if (_M0L7_2abindS2050 == 0) {
        if (_M0L7_2abindS2050) {
          moonbit_decref(_M0L7_2abindS2050);
        }
        _M0L5pnameS2047 = (moonbit_string_t)moonbit_string_literal_142.data;
      } else {
        moonbit_string_t _M0L7_2aSomeS2052 = _M0L7_2abindS2050;
        moonbit_string_t _M0L4_2anS2053 = _M0L7_2aSomeS2052;
        _M0L1nS2049 = _M0L4_2anS2053;
        goto join_2048;
      }
      goto joinlet_4801;
      join_2048:;
      _M0L5pnameS2047 = _M0L1nS2049;
      joinlet_4801:;
      #line 57 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2194
      = _M0MP19moonbitDB8Database6zscore(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_131.data, _M0L3pidS2046);
      #line 57 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L5scoreS2054
      = _M0FP39moonbitDB8examples11leaderboard16show__opt__float(_M0L6_2atmpS2194);
      moonbit_decref(_M0L6_2atmpS2194);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS2055
      = _M0MPB13StringBuilder21StringBuilder_2einner(21);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2055, (moonbit_string_t)moonbit_string_literal_143.data);
      _M0L6_2atmpS2193 = _M0L1iS2045 + 1;
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2055, _M0L6_2atmpS2193);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2055, (moonbit_string_t)moonbit_string_literal_144.data);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2055, _M0L3pidS2046);
      moonbit_decref(_M0L3pidS2046);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2055, (moonbit_string_t)moonbit_string_literal_145.data);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2055, _M0L5pnameS2047);
      moonbit_decref(_M0L5pnameS2047);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2055, (moonbit_string_t)moonbit_string_literal_146.data);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2055, _M0L5scoreS2054);
      moonbit_decref(_M0L5scoreS2054);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2192
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2055);
      moonbit_decref(_M0L18_2astring__builderS2055);
      #line 58 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2192);
      moonbit_decref(_M0L6_2atmpS2192);
      _M0L6_2atmpS2196 = _M0L1iS2045 + 1;
      _M0L1iS2045 = _M0L6_2atmpS2196;
      continue;
    } else {
      moonbit_decref(_M0L9top5__idsS2044);
    }
    break;
  }
  #line 61 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_147.data);
  #line 62 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_130.data);
  _M0L8_2atupleS2265
  = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
  Moonbit_object_header(_M0L8_2atupleS2265)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 13, 0);
  _M0L8_2atupleS2265->$0 = (moonbit_string_t)moonbit_string_literal_113.data;
  _M0L8_2atupleS2265->$1 = 0x1.f4p+7f;
  _M0L8_2atupleS2266
  = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
  Moonbit_object_header(_M0L8_2atupleS2266)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 13, 0);
  _M0L8_2atupleS2266->$0 = (moonbit_string_t)moonbit_string_literal_117.data;
  _M0L8_2atupleS2266->$1 = 0x1.68p+7f;
  _M0L8_2atupleS2267
  = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
  Moonbit_object_header(_M0L8_2atupleS2267)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 13, 0);
  _M0L8_2atupleS2267->$0 = (moonbit_string_t)moonbit_string_literal_121.data;
  _M0L8_2atupleS2267->$1 = 0x1.4p+8f;
  _M0L8_2atupleS2268
  = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
  Moonbit_object_header(_M0L8_2atupleS2268)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 13, 0);
  _M0L8_2atupleS2268->$0 = (moonbit_string_t)moonbit_string_literal_125.data;
  _M0L8_2atupleS2268->$1 = 0x1.9ap+8f;
  _M0L6_2atmpS2264 = (struct _M0TUsfE**)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2264[0] = _M0L8_2atupleS2265;
  _M0L6_2atmpS2264[1] = _M0L8_2atupleS2266;
  _M0L6_2atmpS2264[2] = _M0L8_2atupleS2267;
  _M0L6_2atmpS2264[3] = _M0L8_2atupleS2268;
  _M0L7updatesS2057
  = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
  Moonbit_object_header(_M0L7updatesS2057)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
  _M0L7updatesS2057->$0 = _M0L6_2atmpS2264;
  _M0L7updatesS2057->$1 = 4;
  _M0L1iS2058 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2197;
    #line 64 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
    _M0L6_2atmpS2197 = _M0MPC15array5Array6lengthGUsfEE(_M0L7updatesS2057);
    if (_M0L1iS2058 < _M0L6_2atmpS2197) {
      moonbit_string_t _M0L3pidS2060;
      float _M0L10add__scoreS2061;
      struct _M0TUsfE* _M0L7_2abindS2071;
      moonbit_string_t _M0L6_2apidS2072;
      float _M0L13_2aadd__scoreS2073;
      int32_t _M0L6_2acntS4472;
      float _M0L10new__scoreS2062;
      moonbit_string_t _M0L1nS2065;
      moonbit_string_t _M0L5pnameS2063;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2067;
      moonbit_string_t _M0L6_2atmpS2199;
      moonbit_string_t _M0L7_2abindS2066;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2070;
      moonbit_string_t _M0L6_2atmpS2198;
      int32_t _M0L6_2atmpS2200;
      #line 65 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L7_2abindS2071
      = _M0MPC15array5Array2atGUsfEE(_M0L7updatesS2057, _M0L1iS2058);
      _M0L6_2apidS2072 = _M0L7_2abindS2071->$0;
      _M0L13_2aadd__scoreS2073 = _M0L7_2abindS2071->$1;
      _M0L6_2acntS4472
      = Moonbit_rc_count(Moonbit_object_header(_M0L7_2abindS2071));
      if (_M0L6_2acntS4472 > 1) {
        int32_t _M0L11_2anew__cntS4473 = _M0L6_2acntS4472 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2abindS2071), _M0L11_2anew__cntS4473);
        moonbit_incref(_M0L6_2apidS2072);
      } else if (_M0L6_2acntS4472 == 1) {
        #line 65 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        moonbit_free(_M0L7_2abindS2071);
      }
      _M0L3pidS2060 = _M0L6_2apidS2072;
      _M0L10add__scoreS2061 = _M0L13_2aadd__scoreS2073;
      goto join_2059;
      goto joinlet_4803;
      join_2059:;
      #line 66 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L10new__scoreS2062
      = _M0MP19moonbitDB8Database7zincrby(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_131.data, _M0L10add__scoreS2061, _M0L3pidS2060);
      #line 67 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS2067
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 67 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2067, (moonbit_string_t)moonbit_string_literal_132.data);
      #line 67 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2067, _M0L3pidS2060);
      moonbit_decref(_M0L3pidS2060);
      #line 67 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2199
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2067);
      moonbit_decref(_M0L18_2astring__builderS2067);
      #line 67 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L7_2abindS2066
      = _M0MP19moonbitDB8Database4hget(_M0L2dbS2034, _M0L6_2atmpS2199, (moonbit_string_t)moonbit_string_literal_133.data);
      moonbit_decref(_M0L6_2atmpS2199);
      if (_M0L7_2abindS2066 == 0) {
        if (_M0L7_2abindS2066) {
          moonbit_decref(_M0L7_2abindS2066);
        }
        _M0L5pnameS2063 = (moonbit_string_t)moonbit_string_literal_142.data;
      } else {
        moonbit_string_t _M0L7_2aSomeS2068 = _M0L7_2abindS2066;
        moonbit_string_t _M0L4_2anS2069 = _M0L7_2aSomeS2068;
        _M0L1nS2065 = _M0L4_2anS2069;
        goto join_2064;
      }
      goto joinlet_4804;
      join_2064:;
      _M0L5pnameS2063 = _M0L1nS2065;
      joinlet_4804:;
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS2070
      = _M0MPB13StringBuilder21StringBuilder_2einner(28);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2070, (moonbit_string_t)moonbit_string_literal_148.data);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2070, _M0L5pnameS2063);
      moonbit_decref(_M0L5pnameS2063);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2070, (moonbit_string_t)moonbit_string_literal_149.data);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS2070, _M0L10add__scoreS2061);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2070, (moonbit_string_t)moonbit_string_literal_150.data);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS2070, _M0L10new__scoreS2062);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2198
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2070);
      moonbit_decref(_M0L18_2astring__builderS2070);
      #line 71 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2198);
      moonbit_decref(_M0L6_2atmpS2198);
      joinlet_4803:;
      _M0L6_2atmpS2200 = _M0L1iS2058 + 1;
      _M0L1iS2058 = _M0L6_2atmpS2200;
      continue;
    } else {
      moonbit_decref(_M0L7updatesS2057);
    }
    break;
  }
  #line 74 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_151.data);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_130.data);
  #line 76 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L8all__idsS2075
  = _M0MP19moonbitDB8Database9zrevrange(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_131.data, 0, -1);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_152.data);
  #line 78 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_153.data);
  _M0L1iS2076 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2201;
    #line 79 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
    _M0L6_2atmpS2201 = _M0MPC15array5Array6lengthGsE(_M0L8all__idsS2075);
    if (_M0L1iS2076 < _M0L6_2atmpS2201) {
      moonbit_string_t _M0L3pidS2077;
      moonbit_string_t _M0L1nS2080;
      moonbit_string_t _M0L5pnameS2078;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2082;
      moonbit_string_t _M0L6_2atmpS2206;
      moonbit_string_t _M0L7_2abindS2081;
      void* _M0L6_2atmpS2205;
      moonbit_string_t _M0L5scoreS2085;
      int64_t _M0L9new__rankS2086;
      int32_t _M0L1rS2089;
      moonbit_string_t _M0L9rank__strS2087;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2090;
      int32_t _M0L6_2atmpS2204;
      int32_t _M0L8old__idxS2093;
      int32_t _M0L6changeS2094;
      moonbit_string_t _M0L11change__strS2095;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2098;
      moonbit_string_t _M0L6_2atmpS2202;
      int32_t _M0L6_2atmpS2207;
      #line 80 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L3pidS2077
      = _M0MPC15array5Array2atGsE(_M0L8all__idsS2075, _M0L1iS2076);
      #line 81 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS2082
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 81 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2082, (moonbit_string_t)moonbit_string_literal_132.data);
      #line 81 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2082, _M0L3pidS2077);
      #line 81 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2206
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2082);
      moonbit_decref(_M0L18_2astring__builderS2082);
      #line 81 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L7_2abindS2081
      = _M0MP19moonbitDB8Database4hget(_M0L2dbS2034, _M0L6_2atmpS2206, (moonbit_string_t)moonbit_string_literal_133.data);
      moonbit_decref(_M0L6_2atmpS2206);
      if (_M0L7_2abindS2081 == 0) {
        if (_M0L7_2abindS2081) {
          moonbit_decref(_M0L7_2abindS2081);
        }
        _M0L5pnameS2078 = (moonbit_string_t)moonbit_string_literal_142.data;
      } else {
        moonbit_string_t _M0L7_2aSomeS2083 = _M0L7_2abindS2081;
        moonbit_string_t _M0L4_2anS2084 = _M0L7_2aSomeS2083;
        _M0L1nS2080 = _M0L4_2anS2084;
        goto join_2079;
      }
      goto joinlet_4806;
      join_2079:;
      _M0L5pnameS2078 = _M0L1nS2080;
      joinlet_4806:;
      #line 85 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2205
      = _M0MP19moonbitDB8Database6zscore(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_131.data, _M0L3pidS2077);
      #line 85 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L5scoreS2085
      = _M0FP39moonbitDB8examples11leaderboard16show__opt__float(_M0L6_2atmpS2205);
      moonbit_decref(_M0L6_2atmpS2205);
      #line 86 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L9new__rankS2086
      = _M0MP19moonbitDB8Database8zrevrank(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_131.data, _M0L3pidS2077);
      if (_M0L9new__rankS2086 == 4294967296ll) {
        _M0L9rank__strS2087 = (moonbit_string_t)moonbit_string_literal_0.data;
      } else {
        int64_t _M0L7_2aSomeS2091 = _M0L9new__rankS2086;
        int32_t _M0L4_2arS2092 = (int32_t)_M0L7_2aSomeS2091;
        _M0L1rS2089 = _M0L4_2arS2092;
        goto join_2088;
      }
      goto joinlet_4807;
      join_2088:;
      #line 88 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS2090
      = _M0MPB13StringBuilder21StringBuilder_2einner(1);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2090, (moonbit_string_t)moonbit_string_literal_154.data);
      _M0L6_2atmpS2204 = _M0L1rS2089 + 1;
      #line 88 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2090, _M0L6_2atmpS2204);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L9rank__strS2087
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2090);
      moonbit_decref(_M0L18_2astring__builderS2090);
      joinlet_4807:;
      #line 91 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L8old__idxS2093
      = _M0FP39moonbitDB8examples11leaderboard19find__player__index(_M0L7playersS2035, _M0L3pidS2077);
      if (_M0L8old__idxS2093 >= 0) {
        _M0L6changeS2094 = _M0L8old__idxS2093 - _M0L1iS2076;
      } else {
        _M0L6changeS2094 = 0;
      }
      if (_M0L6changeS2094 > 0) {
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2096;
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0L18_2astring__builderS2096
        = _M0MPB13StringBuilder21StringBuilder_2einner(3);
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2096, (moonbit_string_t)moonbit_string_literal_155.data);
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2096, _M0L6changeS2094);
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0L11change__strS2095
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2096);
        moonbit_decref(_M0L18_2astring__builderS2096);
      } else if (_M0L6changeS2094 < 0) {
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2097;
        int32_t _M0L6_2atmpS2203;
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0L18_2astring__builderS2097
        = _M0MPB13StringBuilder21StringBuilder_2einner(3);
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2097, (moonbit_string_t)moonbit_string_literal_156.data);
        _M0L6_2atmpS2203 = -_M0L6changeS2094;
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2097, _M0L6_2atmpS2203);
        #line 93 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0L11change__strS2095
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2097);
        moonbit_decref(_M0L18_2astring__builderS2097);
      } else {
        _M0L11change__strS2095
        = (moonbit_string_t)moonbit_string_literal_157.data;
      }
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS2098
      = _M0MPB13StringBuilder21StringBuilder_2einner(23);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2098, (moonbit_string_t)moonbit_string_literal_158.data);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2098, _M0L9rank__strS2087);
      moonbit_decref(_M0L9rank__strS2087);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2098, (moonbit_string_t)moonbit_string_literal_144.data);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2098, _M0L3pidS2077);
      moonbit_decref(_M0L3pidS2077);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2098, (moonbit_string_t)moonbit_string_literal_145.data);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2098, _M0L5pnameS2078);
      moonbit_decref(_M0L5pnameS2078);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2098, (moonbit_string_t)moonbit_string_literal_146.data);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2098, _M0L5scoreS2085);
      moonbit_decref(_M0L5scoreS2085);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2098, (moonbit_string_t)moonbit_string_literal_159.data);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2098, _M0L11change__strS2095);
      moonbit_decref(_M0L11change__strS2095);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2202
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2098);
      moonbit_decref(_M0L18_2astring__builderS2098);
      #line 94 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2202);
      moonbit_decref(_M0L6_2atmpS2202);
      _M0L6_2atmpS2207 = _M0L1iS2076 + 1;
      _M0L1iS2076 = _M0L6_2atmpS2207;
      continue;
    } else {
      moonbit_decref(_M0L8all__idsS2075);
      moonbit_decref(_M0L7playersS2035);
    }
    break;
  }
  #line 97 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_160.data);
  #line 98 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_130.data);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L15count__200__400S2100
  = _M0MP19moonbitDB8Database6zcount(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_131.data, 0x1.9p+7f, 0x1.9p+8f);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS2101
  = _M0MPB13StringBuilder21StringBuilder_2einner(30);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2101, (moonbit_string_t)moonbit_string_literal_161.data);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2101, _M0L15count__200__400S2100);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2208
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2101);
  moonbit_decref(_M0L18_2astring__builderS2101);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2208);
  moonbit_decref(_M0L6_2atmpS2208);
  #line 101 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L12mid__playersS2102
  = _M0MP19moonbitDB8Database13zrangebyscore(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_131.data, 0x1.9p+6f, 0x1.2cp+8f);
  #line 102 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS2103
  = _M0MPB13StringBuilder21StringBuilder_2einner(27);
  #line 102 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2103, (moonbit_string_t)moonbit_string_literal_162.data);
  _M0L7_2abindS2104 = (moonbit_string_t)moonbit_string_literal_163.data;
  _M0L6_2atmpS2212 = Moonbit_array_length(_M0L7_2abindS2104);
  _M0L6_2atmpS2211
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS2104, .$1 = 0, .$2 = _M0L6_2atmpS2212
  };
  #line 102 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2210
  = _M0MPC15array5Array4joinGsE(_M0L12mid__playersS2102, _M0L6_2atmpS2211);
  moonbit_decref(_M0L12mid__playersS2102);
  moonbit_decref(_M0L6_2atmpS2211.$0);
  #line 102 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2103, _M0L6_2atmpS2210);
  moonbit_decref(_M0L6_2atmpS2210);
  #line 102 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2209
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2103);
  moonbit_decref(_M0L18_2astring__builderS2103);
  #line 102 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2209);
  moonbit_decref(_M0L6_2atmpS2209);
  #line 104 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_164.data);
  #line 105 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_130.data);
  #line 106 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2213
  = _M0MP19moonbitDB8Database4zadd(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_165.data, 0x1.f4p+8f, (moonbit_string_t)moonbit_string_literal_113.data);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2214
  = _M0MP19moonbitDB8Database4zadd(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_165.data, 0x1.9p+8f, (moonbit_string_t)moonbit_string_literal_115.data);
  #line 108 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2215
  = _M0MP19moonbitDB8Database4zadd(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_165.data, 0x1.2cp+8f, (moonbit_string_t)moonbit_string_literal_117.data);
  #line 109 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2216
  = _M0MP19moonbitDB8Database6expire(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_165.data, 86400);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_166.data);
  #line 111 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS2105
  = _M0MPB13StringBuilder21StringBuilder_2einner(11);
  #line 111 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2105, (moonbit_string_t)moonbit_string_literal_167.data);
  #line 111 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2218
  = _M0MP19moonbitDB8Database3ttl(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_165.data);
  #line 111 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2105, _M0L6_2atmpS2218);
  #line 111 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2105, (moonbit_string_t)moonbit_string_literal_168.data);
  #line 111 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2217
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2105);
  moonbit_decref(_M0L18_2astring__builderS2105);
  #line 111 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2217);
  moonbit_decref(_M0L6_2atmpS2217);
  #line 113 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_169.data);
  #line 114 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_130.data);
  _M0L1iS2106 = 0;
  while (1) {
    if (_M0L1iS2106 < 3) {
      struct _M0TUsfE* _M0L6winnerS2107;
      moonbit_string_t _M0L3pidS2109;
      float _M0L5scoreS2110;
      moonbit_string_t _M0L1nS2113;
      moonbit_string_t _M0L5pnameS2111;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2115;
      moonbit_string_t _M0L6_2atmpS2221;
      moonbit_string_t _M0L7_2abindS2114;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2118;
      int32_t _M0L6_2atmpS2220;
      moonbit_string_t _M0L6_2atmpS2219;
      int32_t _M0L6_2atmpS2224;
      #line 116 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6winnerS2107
      = _M0MP19moonbitDB8Database7zpopmax(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_165.data);
      if (_M0L6winnerS2107 == 0) {
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2123;
        int32_t _M0L6_2atmpS2223;
        moonbit_string_t _M0L6_2atmpS2222;
        if (_M0L6winnerS2107) {
          moonbit_decref(_M0L6winnerS2107);
        }
        #line 125 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0L18_2astring__builderS2123
        = _M0MPB13StringBuilder21StringBuilder_2einner(15);
        #line 125 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2123, (moonbit_string_t)moonbit_string_literal_170.data);
        _M0L6_2atmpS2223 = _M0L1iS2106 + 1;
        #line 125 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2123, _M0L6_2atmpS2223);
        #line 125 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2123, (moonbit_string_t)moonbit_string_literal_171.data);
        #line 125 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0L6_2atmpS2222
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2123);
        moonbit_decref(_M0L18_2astring__builderS2123);
        #line 125 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
        _M0FPB7printlnGsE(_M0L6_2atmpS2222);
        moonbit_decref(_M0L6_2atmpS2222);
      } else {
        struct _M0TUsfE* _M0L7_2aSomeS2119 = _M0L6winnerS2107;
        struct _M0TUsfE* _M0L4_2axS2120 = _M0L7_2aSomeS2119;
        moonbit_string_t _M0L6_2apidS2121 = _M0L4_2axS2120->$0;
        float _M0L8_2ascoreS2122 = _M0L4_2axS2120->$1;
        int32_t _M0L6_2acntS4474 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS2120));
        if (_M0L6_2acntS4474 > 1) {
          int32_t _M0L11_2anew__cntS4475 = _M0L6_2acntS4474 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS2120), _M0L11_2anew__cntS4475);
          moonbit_incref(_M0L6_2apidS2121);
        } else if (_M0L6_2acntS4474 == 1) {
          #line 117 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
          moonbit_free(_M0L4_2axS2120);
        }
        _M0L3pidS2109 = _M0L6_2apidS2121;
        _M0L5scoreS2110 = _M0L8_2ascoreS2122;
        goto join_2108;
      }
      goto joinlet_4809;
      join_2108:;
      #line 119 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS2115
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 119 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2115, (moonbit_string_t)moonbit_string_literal_132.data);
      #line 119 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2115, _M0L3pidS2109);
      #line 119 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2221
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2115);
      moonbit_decref(_M0L18_2astring__builderS2115);
      #line 119 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L7_2abindS2114
      = _M0MP19moonbitDB8Database4hget(_M0L2dbS2034, _M0L6_2atmpS2221, (moonbit_string_t)moonbit_string_literal_133.data);
      moonbit_decref(_M0L6_2atmpS2221);
      if (_M0L7_2abindS2114 == 0) {
        if (_M0L7_2abindS2114) {
          moonbit_decref(_M0L7_2abindS2114);
        }
        _M0L5pnameS2111 = (moonbit_string_t)moonbit_string_literal_142.data;
      } else {
        moonbit_string_t _M0L7_2aSomeS2116 = _M0L7_2abindS2114;
        moonbit_string_t _M0L4_2anS2117 = _M0L7_2aSomeS2116;
        _M0L1nS2113 = _M0L4_2anS2117;
        goto join_2112;
      }
      goto joinlet_4810;
      join_2112:;
      _M0L5pnameS2111 = _M0L1nS2113;
      joinlet_4810:;
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS2118
      = _M0MPB13StringBuilder21StringBuilder_2einner(22);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2118, (moonbit_string_t)moonbit_string_literal_170.data);
      _M0L6_2atmpS2220 = _M0L1iS2106 + 1;
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2118, _M0L6_2atmpS2220);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2118, (moonbit_string_t)moonbit_string_literal_172.data);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2118, _M0L5pnameS2111);
      moonbit_decref(_M0L5pnameS2111);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2118, (moonbit_string_t)moonbit_string_literal_173.data);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2118, _M0L3pidS2109);
      moonbit_decref(_M0L3pidS2109);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2118, (moonbit_string_t)moonbit_string_literal_174.data);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS2118, _M0L5scoreS2110);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2118, (moonbit_string_t)moonbit_string_literal_138.data);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2219
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2118);
      moonbit_decref(_M0L18_2astring__builderS2118);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2219);
      moonbit_decref(_M0L6_2atmpS2219);
      joinlet_4809:;
      _M0L6_2atmpS2224 = _M0L1iS2106 + 1;
      _M0L1iS2106 = _M0L6_2atmpS2224;
      continue;
    }
    break;
  }
  #line 129 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_175.data);
  #line 130 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_130.data);
  #line 131 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2225
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_176.data, (moonbit_string_t)moonbit_string_literal_115.data);
  #line 132 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2226
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_176.data, (moonbit_string_t)moonbit_string_literal_117.data);
  #line 133 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2227
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_176.data, (moonbit_string_t)moonbit_string_literal_119.data);
  #line 134 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2228
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_177.data, (moonbit_string_t)moonbit_string_literal_113.data);
  #line 135 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2229
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_177.data, (moonbit_string_t)moonbit_string_literal_117.data);
  #line 136 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2230
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_177.data, (moonbit_string_t)moonbit_string_literal_121.data);
  _M0L6_2atmpS2263 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS2263[0] = (moonbit_string_t)moonbit_string_literal_176.data;
  _M0L6_2atmpS2263[1] = (moonbit_string_t)moonbit_string_literal_177.data;
  _M0L6_2atmpS2262
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2262)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS2262->$0 = _M0L6_2atmpS2263;
  _M0L6_2atmpS2262->$1 = 2;
  #line 137 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6mutualS2125
  = _M0MP19moonbitDB8Database6sinter(_M0L2dbS2034, _M0L6_2atmpS2262);
  moonbit_decref(_M0L6_2atmpS2262);
  #line 138 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS2126
  = _M0MPB13StringBuilder21StringBuilder_2einner(36);
  #line 138 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2126, (moonbit_string_t)moonbit_string_literal_178.data);
  #line 138 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2232 = _M0MPC15array5Array6lengthGsE(_M0L6mutualS2125);
  #line 138 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2126, _M0L6_2atmpS2232);
  #line 138 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2231
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2126);
  moonbit_decref(_M0L18_2astring__builderS2126);
  #line 138 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2231);
  moonbit_decref(_M0L6_2atmpS2231);
  #line 139 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS2127
  = _M0MPB13StringBuilder21StringBuilder_2einner(18);
  #line 139 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2127, (moonbit_string_t)moonbit_string_literal_179.data);
  _M0L7_2abindS2128 = (moonbit_string_t)moonbit_string_literal_163.data;
  _M0L6_2atmpS2236 = Moonbit_array_length(_M0L7_2abindS2128);
  _M0L6_2atmpS2235
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS2128, .$1 = 0, .$2 = _M0L6_2atmpS2236
  };
  #line 139 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2234
  = _M0MPC15array5Array4joinGsE(_M0L6mutualS2125, _M0L6_2atmpS2235);
  moonbit_decref(_M0L6mutualS2125);
  moonbit_decref(_M0L6_2atmpS2235.$0);
  #line 139 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2127, _M0L6_2atmpS2234);
  moonbit_decref(_M0L6_2atmpS2234);
  #line 139 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2233
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2127);
  moonbit_decref(_M0L18_2astring__builderS2127);
  #line 139 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2233);
  moonbit_decref(_M0L6_2atmpS2233);
  _M0L6_2atmpS2261 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS2261[0] = (moonbit_string_t)moonbit_string_literal_176.data;
  _M0L6_2atmpS2261[1] = (moonbit_string_t)moonbit_string_literal_177.data;
  _M0L6_2atmpS2260
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2260)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS2260->$0 = _M0L6_2atmpS2261;
  _M0L6_2atmpS2260->$1 = 2;
  #line 140 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L12all__friendsS2129
  = _M0MP19moonbitDB8Database6sunion(_M0L2dbS2034, _M0L6_2atmpS2260);
  moonbit_decref(_M0L6_2atmpS2260);
  #line 141 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS2130
  = _M0MPB13StringBuilder21StringBuilder_2einner(34);
  #line 141 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2130, (moonbit_string_t)moonbit_string_literal_180.data);
  #line 141 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2238 = _M0MPC15array5Array6lengthGsE(_M0L12all__friendsS2129);
  moonbit_decref(_M0L12all__friendsS2129);
  #line 141 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2130, _M0L6_2atmpS2238);
  #line 141 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2237
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2130);
  moonbit_decref(_M0L18_2astring__builderS2130);
  #line 141 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2237);
  moonbit_decref(_M0L6_2atmpS2237);
  _M0L6_2atmpS2259 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS2259[0] = (moonbit_string_t)moonbit_string_literal_176.data;
  _M0L6_2atmpS2259[1] = (moonbit_string_t)moonbit_string_literal_177.data;
  _M0L6_2atmpS2258
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2258)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS2258->$0 = _M0L6_2atmpS2259;
  _M0L6_2atmpS2258->$1 = 2;
  #line 142 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L8only__p1S2131
  = _M0MP19moonbitDB8Database5sdiff(_M0L2dbS2034, _M0L6_2atmpS2258);
  moonbit_decref(_M0L6_2atmpS2258);
  #line 143 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS2132
  = _M0MPB13StringBuilder21StringBuilder_2einner(26);
  #line 143 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2132, (moonbit_string_t)moonbit_string_literal_181.data);
  _M0L7_2abindS2133 = (moonbit_string_t)moonbit_string_literal_163.data;
  _M0L6_2atmpS2242 = Moonbit_array_length(_M0L7_2abindS2133);
  _M0L6_2atmpS2241
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS2133, .$1 = 0, .$2 = _M0L6_2atmpS2242
  };
  #line 143 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2240
  = _M0MPC15array5Array4joinGsE(_M0L8only__p1S2131, _M0L6_2atmpS2241);
  moonbit_decref(_M0L8only__p1S2131);
  moonbit_decref(_M0L6_2atmpS2241.$0);
  #line 143 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2132, _M0L6_2atmpS2240);
  moonbit_decref(_M0L6_2atmpS2240);
  #line 143 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2239
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2132);
  moonbit_decref(_M0L18_2astring__builderS2132);
  #line 143 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2239);
  moonbit_decref(_M0L6_2atmpS2239);
  #line 145 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_182.data);
  #line 146 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_130.data);
  _M0L6_2atmpS2257 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2257[0] = (moonbit_string_t)moonbit_string_literal_183.data;
  _M0L6_2atmpS2257[1] = (moonbit_string_t)moonbit_string_literal_184.data;
  _M0L6_2atmpS2257[2] = (moonbit_string_t)moonbit_string_literal_185.data;
  _M0L6_2atmpS2257[3] = (moonbit_string_t)moonbit_string_literal_186.data;
  _M0L8messagesS2134
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L8messagesS2134)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L8messagesS2134->$0 = _M0L6_2atmpS2257;
  _M0L8messagesS2134->$1 = 4;
  _M0L7_2abindS2135 = _M0L8messagesS2134->$1;
  _M0L2__S2136 = 0;
  while (1) {
    if (_M0L2__S2136 < _M0L7_2abindS2135) {
      moonbit_string_t* _M0L3bufS2245 = _M0L8messagesS2134->$0;
      moonbit_string_t _M0L3msgS2137 =
        (moonbit_string_t)_M0L3bufS2245[_M0L2__S2136];
      int32_t _M0L6_2atmpS2243;
      int32_t _M0L6_2atmpS2244;
      moonbit_incref(_M0L3msgS2137);
      #line 149 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2243
      = _M0MP19moonbitDB8Database5rpush(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_187.data, _M0L3msgS2137);
      moonbit_decref(_M0L3msgS2137);
      _M0L6_2atmpS2244 = _M0L2__S2136 + 1;
      _M0L2__S2136 = _M0L6_2atmpS2244;
      continue;
    } else {
      moonbit_decref(_M0L8messagesS2134);
    }
    break;
  }
  #line 151 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS2139
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 151 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2139, (moonbit_string_t)moonbit_string_literal_188.data);
  #line 151 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2247
  = _M0MP19moonbitDB8Database4llen(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_187.data);
  #line 151 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2139, _M0L6_2atmpS2247);
  #line 151 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2246
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2139);
  moonbit_decref(_M0L18_2astring__builderS2139);
  #line 151 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2246);
  moonbit_decref(_M0L6_2atmpS2246);
  #line 152 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_189.data);
  while (1) {
    int32_t _M0L6_2atmpS2248;
    #line 153 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
    _M0L6_2atmpS2248
    = _M0MP19moonbitDB8Database4llen(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_187.data);
    if (_M0L6_2atmpS2248 > 0) {
      moonbit_string_t _M0L3msgS2140;
      moonbit_string_t _M0L1mS2142;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2143;
      moonbit_string_t _M0L6_2atmpS2249;
      #line 154 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L3msgS2140
      = _M0MP19moonbitDB8Database4lpop(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_187.data);
      if (_M0L3msgS2140 == 0) {
        if (_M0L3msgS2140) {
          moonbit_decref(_M0L3msgS2140);
        }
        break;
      } else {
        moonbit_string_t _M0L7_2aSomeS2144 = _M0L3msgS2140;
        moonbit_string_t _M0L4_2amS2145 = _M0L7_2aSomeS2144;
        _M0L1mS2142 = _M0L4_2amS2145;
        goto join_2141;
      }
      goto joinlet_4813;
      join_2141:;
      #line 156 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L18_2astring__builderS2143
      = _M0MPB13StringBuilder21StringBuilder_2einner(15);
      #line 156 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2143, (moonbit_string_t)moonbit_string_literal_190.data);
      #line 156 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2143, _M0L1mS2142);
      moonbit_decref(_M0L1mS2142);
      #line 156 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0L6_2atmpS2249
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2143);
      moonbit_decref(_M0L18_2astring__builderS2143);
      #line 156 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2249);
      moonbit_decref(_M0L6_2atmpS2249);
      joinlet_4813:;
      continue;
    }
    break;
  }
  #line 160 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS2147
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 160 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2147, (moonbit_string_t)moonbit_string_literal_191.data);
  #line 160 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2251
  = _M0MP19moonbitDB8Database4llen(_M0L2dbS2034, (moonbit_string_t)moonbit_string_literal_187.data);
  #line 160 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2147, _M0L6_2atmpS2251);
  #line 160 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2250
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2147);
  moonbit_decref(_M0L18_2astring__builderS2147);
  #line 160 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2250);
  moonbit_decref(_M0L6_2atmpS2250);
  #line 162 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_192.data);
  #line 163 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_130.data);
  #line 164 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS2148
  = _M0MPB13StringBuilder21StringBuilder_2einner(15);
  #line 164 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2148, (moonbit_string_t)moonbit_string_literal_193.data);
  #line 164 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2253 = _M0MP19moonbitDB8Database6dbsize(_M0L2dbS2034);
  moonbit_decref(_M0L2dbS2034);
  #line 164 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2148, _M0L6_2atmpS2253);
  #line 164 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2252
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2148);
  moonbit_decref(_M0L18_2astring__builderS2148);
  #line 164 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2252);
  moonbit_decref(_M0L6_2atmpS2252);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L18_2astring__builderS2149
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2149, (moonbit_string_t)moonbit_string_literal_194.data);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2256 = _M0MP19moonbitDB8Database7command();
  #line 165 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2255 = _M0MPC15array5Array6lengthGsE(_M0L6_2atmpS2256);
  moonbit_decref(_M0L6_2atmpS2256);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2149, _M0L6_2atmpS2255);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0L6_2atmpS2254
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2149);
  moonbit_decref(_M0L18_2astring__builderS2149);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2254);
  moonbit_decref(_M0L6_2atmpS2254);
  #line 167 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_195.data);
  #line 168 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_196.data);
  #line 169 "/home/developer/Documents2/moonbitDB/examples/leaderboard/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_111.data);
  return 0;
}