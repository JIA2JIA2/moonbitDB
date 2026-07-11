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
struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2980__l711__;

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

struct _M0BTPB6Logger;

struct _M0DTP19moonbitDB10RedisValue6String;

struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE;

struct _M0DTP19moonbitDB10RedisValue4Hash;

struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u2970__l711__;

struct _M0TPB6Logger;

struct _M0TP19moonbitDB5Deque;

struct _M0TUsfE;

struct _M0TPB8MutLocalGORPB5EntryGsfEE;

struct _M0TPB5EntryGsbE;

struct _M0TPB8MutLocalGORPB5EntryGssEE;

struct _M0DTPC16option6OptionGfE4Some;

struct _M0TPB19MulShiftAll64Result;

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2950__l711__;

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

struct _M0TPB4IterGUsfEE;

struct _M0TWEOUsfE;

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

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2960__l711__;

struct _M0TPB4IterGUsbEE;

struct _M0TPB4IterGUssEE;

struct _M0TPB5EntryGsiE;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2980__l711__ {
  struct _M0TUsfE*(* code)(struct _M0TWEOUsfE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* $1;
  
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

struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u2970__l711__ {
  struct _M0TUsRP19moonbitDB10RedisValueE*(* code)(
    struct _M0TWEOUsRP19moonbitDB10RedisValueE*
  );
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE* $1;
  
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

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2950__l711__ {
  struct _M0TUssE*(* code)(struct _M0TWEOUssE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* $1;
  
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

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2960__l711__ {
  struct _M0TUsbE*(* code)(struct _M0TWEOUsbE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* $1;
  
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

moonbit_string_t _M0FP39moonbitDB8examples12basic__usage16show__opt__float(
  void*
);

moonbit_string_t _M0FP39moonbitDB8examples12basic__usage14show__opt__int(
  int64_t
);

moonbit_string_t _M0FP39moonbitDB8examples12basic__usage9show__opt(
  moonbit_string_t
);

moonbit_string_t _M0FP19moonbitDB16show__opt__float(void*);

moonbit_string_t _M0FP19moonbitDB14show__opt__int(int64_t);

moonbit_string_t _M0FP19moonbitDB9show__opt(moonbit_string_t);

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database7command();

moonbit_string_t _M0MP19moonbitDB8Database4echo(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

moonbit_string_t _M0MP19moonbitDB8Database4ping();

moonbit_string_t _M0MP19moonbitDB8Database9randomkey(
  struct _M0TP19moonbitDB8Database*
);

int32_t _M0MP19moonbitDB8Database6dbsize(struct _M0TP19moonbitDB8Database*);

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

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database8smembers(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t
);

int32_t _M0MP19moonbitDB8Database4sadd(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0MP19moonbitDB8Database6lindex(
  struct _M0TP19moonbitDB8Database*,
  moonbit_string_t,
  int32_t
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

moonbit_string_t _M0MP19moonbitDB5Deque7get__at(
  struct _M0TP19moonbitDB5Deque*,
  int32_t
);

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

struct _M0TUsfE* _M0MPB3Map4iterGsfEC2980l711(struct _M0TWEOUsfE*);

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB3Map4iterGsRP19moonbitDB10RedisValueEC2970l711(
  struct _M0TWEOUsRP19moonbitDB10RedisValueE*
);

struct _M0TUsbE* _M0MPB3Map4iterGsbEC2960l711(struct _M0TWEOUsbE*);

struct _M0TUssE* _M0MPB3Map4iterGssEC2950l711(struct _M0TWEOUssE*);

int32_t _M0MPB3Map6lengthGssE(struct _M0TPB3MapGssE*);

int32_t _M0MPB3Map6lengthGsbE(struct _M0TPB3MapGsbE*);

int32_t _M0MPB3Map6lengthGsfE(struct _M0TPB3MapGsfE*);

int32_t _M0MPB3Map6removeGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

int32_t _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  moonbit_string_t
);

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

int32_t _M0MPB3Map11shift__backGsiE(struct _M0TPB3MapGsiE*, int32_t);

int32_t _M0MPB3Map11shift__backGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  int32_t
);

int32_t _M0MPB3Map13remove__entryGsiE(
  struct _M0TPB3MapGsiE*,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPB3Map13remove__entryGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*
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

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_121 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 71, 69, 
    84, 32, 110, 97, 109, 101, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_96 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 110, 111, 
    110, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_95 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 0, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_91 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 55, 0};

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

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_61 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 90, 82, 
    65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_72 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 68, 66, 
    83, 73, 90, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_104 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 73, 110, 
    102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_103 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_182 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 77, 71, 
    69, 84, 32, 107, 49, 44, 107, 50, 44, 107, 51, 44, 107, 52, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_163 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 90, 67, 
    65, 82, 68, 32, 108, 101, 97, 100, 101, 114, 98, 111, 97, 114, 100, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_85 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_84 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_190 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 72, 101, 
    108, 108, 111, 32, 77, 111, 111, 110, 66, 105, 116, 68, 66, 33, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_53 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 83, 68, 
    73, 70, 70, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_187 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 68, 66, 
    83, 73, 90, 69, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_18 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 71, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_164 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 90, 83, 
    67, 79, 82, 69, 32, 112, 108, 97, 121, 101, 114, 50, 58, 32, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_193 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 84, 89, 
    80, 69, 32, 116, 97, 115, 107, 115, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_153 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 112, 114, 
    111, 103, 114, 97, 109, 109, 105, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_195 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 84, 89, 
    80, 69, 32, 110, 111, 110, 101, 120, 105, 115, 116, 101, 110, 116, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_183 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 32, 32, 91, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_63 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 90, 82, 
    69, 86, 82, 65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_88 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 52, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_115 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 46, 108, 101, 110, 103, 116, 104, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_171 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 115, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_120 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 50, 53, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_15 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 84, 
    84, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_161 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 112, 108, 
    97, 121, 101, 114, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_156 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 83, 73, 
    83, 77, 69, 77, 66, 69, 82, 32, 109, 111, 111, 110, 98, 105, 116, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_112 =
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
} const moonbit_string_literal_141 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 116, 97, 
    115, 107, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_140 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 10, 61, 
    61, 61, 32, 76, 105, 115, 116, 32, 25805, 20316, 32, 61, 61, 61, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_129 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 10, 61, 
    61, 61, 32, 72, 97, 115, 104, 32, 25805, 20316, 32, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_102 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 25, 73, 108, 
    108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 116, 69, 
    120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_54 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 77, 
    79, 86, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_134 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_189 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 69, 67, 
    72, 79, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_172 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 50, 48, 
    31186, 21518, 32, 84, 84, 76, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_94 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_132 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 97, 108, 
    105, 99, 101, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_116 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 61, 61, 
    61, 32, 83, 116, 114, 105, 110, 103, 32, 25805, 20316, 32, 61, 61, 
    61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_101 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 122, 115, 
    101, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_176 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 107, 50, 0};

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

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_188 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 80, 73, 
    78, 71, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_167 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 10, 61, 
    61, 61, 32, 75, 101, 121, 32, 36807, 26399, 26426, 21046, 32, 61, 
    61, 61, 0
  };

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

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_146 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 76, 82, 
    65, 78, 71, 69, 32, 116, 97, 115, 107, 115, 32, 48, 32, 45, 49, 58, 
    32, 91, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_136 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 72, 76, 
    69, 78, 32, 117, 115, 101, 114, 58, 49, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_75 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 82, 65, 
    78, 68, 79, 77, 75, 69, 89, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_143 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 84, 97, 
    115, 107, 32, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_157 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 83, 77, 
    69, 77, 66, 69, 82, 83, 58, 32, 91, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_196 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 110, 111, 
    110, 101, 120, 105, 115, 116, 101, 110, 116, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_179 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 118, 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_32 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_186 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 10, 61, 
    61, 61, 32, 26381, 21153, 22120, 20449, 24687, 32, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_177 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 107, 51, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_148 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 82, 80, 
    79, 80, 32, 116, 97, 115, 107, 115, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_66 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 90, 82, 
    69, 86, 82, 65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_119 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 97, 103, 101, 0};

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

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_174 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 10, 61, 
    61, 61, 32, 25209, 37327, 25805, 20316, 32, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_55 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_152 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 116, 101, 
    99, 104, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_65 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 90, 82, 
    65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_180 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 118, 51, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_82 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 67, 79, 
    77, 77, 65, 78, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_58 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 90, 67, 
    65, 82, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_147 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 76, 80, 
    79, 80, 32, 116, 97, 115, 107, 115, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_138 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 44, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_14 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 84, 84, 76, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_165 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 90, 82, 
    65, 78, 75, 32, 112, 108, 97, 121, 101, 114, 51, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_159 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 108, 101, 
    97, 100, 101, 114, 98, 111, 97, 114, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_34 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 76, 
    69, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_108 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 30, 114, 97, 
    100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 98, 101, 
    116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_86 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 50, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_162 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 112, 108, 
    97, 121, 101, 114, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_135 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 22, 72, 71, 
    69, 84, 32, 117, 115, 101, 114, 58, 49, 32, 117, 115, 101, 114, 110, 
    97, 109, 101, 58, 32, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_173 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 51, 53, 
    31186, 21518, 32, 69, 88, 73, 83, 84, 83, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_145 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 76, 76, 
    69, 78, 32, 116, 97, 115, 107, 115, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_114 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 44, 32, 
    108, 101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_107 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 102, 97, 
    108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_122 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 71, 69, 
    84, 32, 97, 103, 101, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_111 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 36, 98, 111, 
    117, 110, 100, 115, 32, 99, 104, 101, 99, 107, 32, 102, 97, 105, 
    108, 101, 100, 58, 32, 97, 108, 108, 111, 99, 97, 116, 101, 95, 108, 
    101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_131 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 117, 115, 
    101, 114, 110, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_191 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 10, 61, 
    61, 61, 32, 84, 89, 80, 69, 32, 26816, 26597, 32, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_154 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 109, 111, 
    111, 110, 98, 105, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_144 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 84, 97, 
    115, 107, 32, 51, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_142 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 84, 97, 
    115, 107, 32, 49, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_100 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 115, 101, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_57 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 82, 
    65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_158 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 22, 10, 61, 
    61, 61, 32, 83, 111, 114, 116, 101, 100, 32, 83, 101, 116, 32, 25805,
    20316, 32, 61, 61, 61, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_109 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_130 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 117, 115, 
    101, 114, 58, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_181 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 107, 52, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_149 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 76, 73, 
    78, 68, 69, 88, 32, 116, 97, 115, 107, 115, 32, 48, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_168 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 115, 101, 
    115, 115, 105, 111, 110, 58, 117, 115, 101, 114, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_124 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 22, 65, 80, 
    80, 69, 78, 68, 32, 110, 97, 109, 101, 32, 39, 32, 83, 109, 105, 
    116, 104, 39, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_123 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 83, 84, 
    82, 76, 69, 78, 32, 110, 97, 109, 101, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_118 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 65, 108, 
    105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_97 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 115, 116, 
    114, 105, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_67 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 90, 73, 
    78, 67, 82, 66, 89, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_133 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_69 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 90, 82, 
    69, 77, 82, 65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_125 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 32, 83, 
    109, 105, 116, 104, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_51 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 83, 85, 
    78, 73, 79, 78, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_28 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 77, 
    83, 69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_178 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 118, 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_113 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    100, 115, 116, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_110 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 105, 110, 
    118, 97, 108, 105, 100, 32, 99, 111, 100, 101, 32, 112, 111, 105, 
    110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_126 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 23, 71, 69, 
    84, 32, 110, 97, 109, 101, 32, 97, 102, 116, 101, 114, 32, 97, 112, 
    112, 101, 110, 100, 58, 32, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_170 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 84, 84, 
    76, 32, 115, 101, 115, 115, 105, 111, 110, 58, 117, 115, 101, 114, 
    49, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_137 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 72, 71, 
    69, 84, 65, 76, 76, 32, 102, 105, 101, 108, 100, 115, 58, 32, 91, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_78 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 73, 
    78, 71, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_184 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 93, 32, 
    61, 32, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_192 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 84, 89, 
    80, 69, 32, 117, 115, 101, 114, 58, 49, 58, 32, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_117 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 110, 97, 
    109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_139 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 93, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_155 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 83, 67, 
    65, 82, 68, 32, 116, 97, 103, 115, 58, 112, 111, 115, 116, 58, 49, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_151 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 116, 97, 
    103, 115, 58, 112, 111, 115, 116, 58, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_106 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 116, 114, 
    117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_175 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 107, 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_128 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 68, 69, 
    67, 82, 32, 97, 103, 101, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_17 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_1 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_127 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 73, 78, 
    67, 82, 32, 97, 103, 101, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_92 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 56, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_194 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 84, 89, 
    80, 69, 32, 108, 101, 97, 100, 101, 114, 98, 111, 97, 114, 100, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_166 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 90, 82, 
    65, 78, 71, 69, 32, 48, 32, 50, 58, 32, 91, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_99 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 108, 105, 
    115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_41 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 82, 80, 
    85, 83, 72, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_197 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 10, 9989, 
    32, 22522, 26412, 20351, 29992, 31034, 20363, 36816, 34892, 23436, 
    25104, 65281, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_160 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 112, 108, 
    97, 121, 101, 114, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_62 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 67, 
    79, 85, 78, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_185 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 77, 68, 
    69, 76, 32, 21024, 38500, 25968, 37327, 58, 32, 0
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
} const moonbit_string_literal_98 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 104, 97, 
    115, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_79 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 69, 67, 
    72, 79, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_150 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 10, 61, 
    61, 61, 32, 83, 101, 116, 32, 25805, 20316, 32, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_105 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 48, 46, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_73 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 70, 76, 
    85, 83, 72, 68, 66, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_16 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 80, 69, 
    82, 83, 73, 83, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_169 =
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
    sizeof(struct _M0TPB5ArrayGsE) / 4, 1,
    offsetof(struct _M0TPB5ArrayGsE, $0) / 4,
    sizeof(struct _M0TPB5ArrayGUsfEE) / 4, 1,
    offsetof(struct _M0TPB5ArrayGUsfEE, $0) / 4, sizeof(struct _M0TUsfE) / 4,
    1, offsetof(struct _M0TUsfE, $0) / 4,
    sizeof(struct _M0DTP19moonbitDB10RedisValue4ZSet) / 4, 1,
    offsetof(struct _M0DTP19moonbitDB10RedisValue4ZSet, $0) / 4,
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
    sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2950__l711__)
    / 4, 2,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2950__l711__, $0)
    / 4,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2950__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsbEE, $0) / 4,
    sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2960__l711__)
    / 4, 2,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2960__l711__, $0)
    / 4,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2960__l711__, $1)
    / 4,
    sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE)
    / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE, $0)
    / 4,
    sizeof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u2970__l711__)
    / 4, 2,
    offsetof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u2970__l711__, $0)
    / 4,
    offsetof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u2970__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsfEE, $0) / 4,
    sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2980__l711__)
    / 4, 2,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2980__l711__, $0)
    / 4,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2980__l711__, $1)
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

int64_t _M0MPB4Iter4nextN6constrS9980GUsRP19moonbitDB10RedisValueEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsRP19moonbitDB10RedisValueEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9980GUsfEE = 0ll;

int64_t _M0MPB4Iter4nextN6constrS9981GUsfEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUssEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsbEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsRP19moonbitDB10RedisValueEE = 0ll;

int64_t _M0MPB4Iter3newN6constrS9988GUsfEE = 0ll;

moonbit_string_t _M0FP39moonbitDB8examples12basic__usage16show__opt__float(
  void* _M0L3optS1867
) {
  float _M0L1vS1865;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1866;
  moonbit_string_t _result_4086;
  #line 15 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  switch (Moonbit_object_tag(_M0L3optS1867)) {
    case 1: {
      struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS1868 =
        (struct _M0DTPC16option6OptionGfE4Some*)_M0L3optS1867;
      float _M0L4_2avS1869 = _M0L7_2aSomeS1868->$0;
      _M0L1vS1865 = _M0L4_2avS1869;
      goto join_1864;
      break;
    }
    default: {
      return (moonbit_string_t)moonbit_string_literal_0.data;
      break;
    }
  }
  join_1864:;
  #line 17 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1866
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 17 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1866, _M0L1vS1865);
  #line 17 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _result_4086
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1866);
  moonbit_decref(_M0L18_2astring__builderS1866);
  return _result_4086;
}

moonbit_string_t _M0FP39moonbitDB8examples12basic__usage14show__opt__int(
  int64_t _M0L3optS1861
) {
  int32_t _M0L1vS1859;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1860;
  moonbit_string_t _result_4088;
  #line 8 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  if (_M0L3optS1861 == 4294967296ll) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int64_t _M0L7_2aSomeS1862 = _M0L3optS1861;
    int32_t _M0L4_2avS1863 = (int32_t)_M0L7_2aSomeS1862;
    _M0L1vS1859 = _M0L4_2avS1863;
    goto join_1858;
  }
  join_1858:;
  #line 10 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1860
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 10 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1860, _M0L1vS1859);
  #line 10 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _result_4088
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1860);
  moonbit_decref(_M0L18_2astring__builderS1860);
  return _result_4088;
}

moonbit_string_t _M0FP39moonbitDB8examples12basic__usage9show__opt(
  moonbit_string_t _M0L3optS1855
) {
  moonbit_string_t _M0L1vS1853;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1854;
  moonbit_string_t _result_4090;
  #line 1 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  if (_M0L3optS1855 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    moonbit_string_t _M0L7_2aSomeS1856 = _M0L3optS1855;
    moonbit_string_t _M0L4_2avS1857 = _M0L7_2aSomeS1856;
    moonbit_incref(_M0L4_2avS1857);
    _M0L1vS1853 = _M0L4_2avS1857;
    goto join_1852;
  }
  join_1852:;
  #line 3 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1854
  = _M0MPB13StringBuilder21StringBuilder_2einner(2);
  #line 3 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1854, (moonbit_string_t)moonbit_string_literal_1.data);
  #line 3 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1854, _M0L1vS1853);
  moonbit_decref(_M0L1vS1853);
  #line 3 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1854, (moonbit_string_t)moonbit_string_literal_1.data);
  #line 3 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _result_4090
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1854);
  moonbit_decref(_M0L18_2astring__builderS1854);
  return _result_4090;
}

moonbit_string_t _M0FP19moonbitDB16show__opt__float(void* _M0L3optS1788) {
  float _M0L1vS1786;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1787;
  moonbit_string_t _result_4092;
  #line 15 "/home/developer/Documents2/moonbitDB/demo.mbt"
  switch (Moonbit_object_tag(_M0L3optS1788)) {
    case 1: {
      struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS1789 =
        (struct _M0DTPC16option6OptionGfE4Some*)_M0L3optS1788;
      float _M0L4_2avS1790 = _M0L7_2aSomeS1789->$0;
      _M0L1vS1786 = _M0L4_2avS1790;
      goto join_1785;
      break;
    }
    default: {
      return (moonbit_string_t)moonbit_string_literal_0.data;
      break;
    }
  }
  join_1785:;
  #line 17 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1787
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 17 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1787, _M0L1vS1786);
  #line 17 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _result_4092
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1787);
  moonbit_decref(_M0L18_2astring__builderS1787);
  return _result_4092;
}

moonbit_string_t _M0FP19moonbitDB14show__opt__int(int64_t _M0L3optS1782) {
  int32_t _M0L1vS1780;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1781;
  moonbit_string_t _result_4094;
  #line 8 "/home/developer/Documents2/moonbitDB/demo.mbt"
  if (_M0L3optS1782 == 4294967296ll) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int64_t _M0L7_2aSomeS1783 = _M0L3optS1782;
    int32_t _M0L4_2avS1784 = (int32_t)_M0L7_2aSomeS1783;
    _M0L1vS1780 = _M0L4_2avS1784;
    goto join_1779;
  }
  join_1779:;
  #line 10 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1781
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 10 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1781, _M0L1vS1780);
  #line 10 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _result_4094
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1781);
  moonbit_decref(_M0L18_2astring__builderS1781);
  return _result_4094;
}

moonbit_string_t _M0FP19moonbitDB9show__opt(moonbit_string_t _M0L3optS1776) {
  moonbit_string_t _M0L1vS1774;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1775;
  moonbit_string_t _result_4096;
  #line 1 "/home/developer/Documents2/moonbitDB/demo.mbt"
  if (_M0L3optS1776 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    moonbit_string_t _M0L7_2aSomeS1777 = _M0L3optS1776;
    moonbit_string_t _M0L4_2avS1778 = _M0L7_2aSomeS1777;
    moonbit_incref(_M0L4_2avS1778);
    _M0L1vS1774 = _M0L4_2avS1778;
    goto join_1773;
  }
  join_1773:;
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1775
  = _M0MPB13StringBuilder21StringBuilder_2einner(2);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1775, (moonbit_string_t)moonbit_string_literal_1.data);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1775, _M0L1vS1774);
  moonbit_decref(_M0L1vS1774);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1775, (moonbit_string_t)moonbit_string_literal_1.data);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _result_4096
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1775);
  moonbit_decref(_M0L18_2astring__builderS1775);
  return _result_4096;
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database7command() {
  moonbit_string_t* _M0L6_2atmpS3630;
  struct _M0TPB5ArrayGsE* _block_4097;
  #line 1594 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3630 = (moonbit_string_t*)moonbit_make_ref_array_raw(81);
  _M0L6_2atmpS3630[0] = (moonbit_string_t)moonbit_string_literal_2.data;
  _M0L6_2atmpS3630[1] = (moonbit_string_t)moonbit_string_literal_3.data;
  _M0L6_2atmpS3630[2] = (moonbit_string_t)moonbit_string_literal_4.data;
  _M0L6_2atmpS3630[3] = (moonbit_string_t)moonbit_string_literal_5.data;
  _M0L6_2atmpS3630[4] = (moonbit_string_t)moonbit_string_literal_6.data;
  _M0L6_2atmpS3630[5] = (moonbit_string_t)moonbit_string_literal_7.data;
  _M0L6_2atmpS3630[6] = (moonbit_string_t)moonbit_string_literal_8.data;
  _M0L6_2atmpS3630[7] = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3630[8] = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3630[9] = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS3630[10] = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS3630[11] = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS3630[12] = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS3630[13] = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS3630[14] = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS3630[15] = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS3630[16] = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS3630[17] = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L6_2atmpS3630[18] = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L6_2atmpS3630[19] = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS3630[20] = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS3630[21] = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L6_2atmpS3630[22] = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L6_2atmpS3630[23] = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS3630[24] = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS3630[25] = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L6_2atmpS3630[26] = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L6_2atmpS3630[27] = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L6_2atmpS3630[28] = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS3630[29] = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS3630[30] = (moonbit_string_t)moonbit_string_literal_32.data;
  _M0L6_2atmpS3630[31] = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L6_2atmpS3630[32] = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L6_2atmpS3630[33] = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L6_2atmpS3630[34] = (moonbit_string_t)moonbit_string_literal_36.data;
  _M0L6_2atmpS3630[35] = (moonbit_string_t)moonbit_string_literal_37.data;
  _M0L6_2atmpS3630[36] = (moonbit_string_t)moonbit_string_literal_38.data;
  _M0L6_2atmpS3630[37] = (moonbit_string_t)moonbit_string_literal_39.data;
  _M0L6_2atmpS3630[38] = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L6_2atmpS3630[39] = (moonbit_string_t)moonbit_string_literal_41.data;
  _M0L6_2atmpS3630[40] = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS3630[41] = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS3630[42] = (moonbit_string_t)moonbit_string_literal_44.data;
  _M0L6_2atmpS3630[43] = (moonbit_string_t)moonbit_string_literal_45.data;
  _M0L6_2atmpS3630[44] = (moonbit_string_t)moonbit_string_literal_46.data;
  _M0L6_2atmpS3630[45] = (moonbit_string_t)moonbit_string_literal_47.data;
  _M0L6_2atmpS3630[46] = (moonbit_string_t)moonbit_string_literal_48.data;
  _M0L6_2atmpS3630[47] = (moonbit_string_t)moonbit_string_literal_49.data;
  _M0L6_2atmpS3630[48] = (moonbit_string_t)moonbit_string_literal_50.data;
  _M0L6_2atmpS3630[49] = (moonbit_string_t)moonbit_string_literal_51.data;
  _M0L6_2atmpS3630[50] = (moonbit_string_t)moonbit_string_literal_52.data;
  _M0L6_2atmpS3630[51] = (moonbit_string_t)moonbit_string_literal_53.data;
  _M0L6_2atmpS3630[52] = (moonbit_string_t)moonbit_string_literal_54.data;
  _M0L6_2atmpS3630[53] = (moonbit_string_t)moonbit_string_literal_55.data;
  _M0L6_2atmpS3630[54] = (moonbit_string_t)moonbit_string_literal_56.data;
  _M0L6_2atmpS3630[55] = (moonbit_string_t)moonbit_string_literal_57.data;
  _M0L6_2atmpS3630[56] = (moonbit_string_t)moonbit_string_literal_58.data;
  _M0L6_2atmpS3630[57] = (moonbit_string_t)moonbit_string_literal_59.data;
  _M0L6_2atmpS3630[58] = (moonbit_string_t)moonbit_string_literal_60.data;
  _M0L6_2atmpS3630[59] = (moonbit_string_t)moonbit_string_literal_61.data;
  _M0L6_2atmpS3630[60] = (moonbit_string_t)moonbit_string_literal_62.data;
  _M0L6_2atmpS3630[61] = (moonbit_string_t)moonbit_string_literal_63.data;
  _M0L6_2atmpS3630[62] = (moonbit_string_t)moonbit_string_literal_64.data;
  _M0L6_2atmpS3630[63] = (moonbit_string_t)moonbit_string_literal_65.data;
  _M0L6_2atmpS3630[64] = (moonbit_string_t)moonbit_string_literal_66.data;
  _M0L6_2atmpS3630[65] = (moonbit_string_t)moonbit_string_literal_67.data;
  _M0L6_2atmpS3630[66] = (moonbit_string_t)moonbit_string_literal_68.data;
  _M0L6_2atmpS3630[67] = (moonbit_string_t)moonbit_string_literal_69.data;
  _M0L6_2atmpS3630[68] = (moonbit_string_t)moonbit_string_literal_70.data;
  _M0L6_2atmpS3630[69] = (moonbit_string_t)moonbit_string_literal_71.data;
  _M0L6_2atmpS3630[70] = (moonbit_string_t)moonbit_string_literal_72.data;
  _M0L6_2atmpS3630[71] = (moonbit_string_t)moonbit_string_literal_73.data;
  _M0L6_2atmpS3630[72] = (moonbit_string_t)moonbit_string_literal_74.data;
  _M0L6_2atmpS3630[73] = (moonbit_string_t)moonbit_string_literal_75.data;
  _M0L6_2atmpS3630[74] = (moonbit_string_t)moonbit_string_literal_76.data;
  _M0L6_2atmpS3630[75] = (moonbit_string_t)moonbit_string_literal_77.data;
  _M0L6_2atmpS3630[76] = (moonbit_string_t)moonbit_string_literal_78.data;
  _M0L6_2atmpS3630[77] = (moonbit_string_t)moonbit_string_literal_79.data;
  _M0L6_2atmpS3630[78] = (moonbit_string_t)moonbit_string_literal_80.data;
  _M0L6_2atmpS3630[79] = (moonbit_string_t)moonbit_string_literal_81.data;
  _M0L6_2atmpS3630[80] = (moonbit_string_t)moonbit_string_literal_82.data;
  _block_4097
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_block_4097)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _block_4097->$0 = _M0L6_2atmpS3630;
  _block_4097->$1 = 81;
  return _block_4097;
}

moonbit_string_t _M0MP19moonbitDB8Database4echo(
  struct _M0TP19moonbitDB8Database* _M0L12_2adiscard__S1772,
  moonbit_string_t _M0L7messageS1771
) {
  #line 1560 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  moonbit_incref(_M0L7messageS1771);
  return _M0L7messageS1771;
}

moonbit_string_t _M0MP19moonbitDB8Database4ping() {
  #line 1556 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  return (moonbit_string_t)moonbit_string_literal_83.data;
}

moonbit_string_t _M0MP19moonbitDB8Database9randomkey(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1761
) {
  moonbit_string_t* _M0L6_2atmpS3629;
  struct _M0TPB5ArrayGsE* _M0L9all__keysS1759;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3624;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L5_2aitS1760;
  int32_t _M0L6_2atmpS3625;
  #line 1503 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3629 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L9all__keysS1759
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L9all__keysS1759)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L9all__keysS1759->$0 = _M0L6_2atmpS3629;
  _M0L9all__keysS1759->$1 = 0;
  _M0L4dataS3624 = _M0L4selfS1761->$0;
  moonbit_incref(_M0L4dataS3624);
  #line 1504 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1760
  = _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(_M0L4dataS3624);
  moonbit_decref(_M0L4dataS3624);
  while (1) {
    moonbit_string_t _M0L3keyS1763;
    struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2abindS1765;
    int32_t _M0L6_2atmpS3623;
    #line 1505 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1765
    = _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(_M0L5_2aitS1760);
    if (_M0L7_2abindS1765 == 0) {
      if (_M0L7_2abindS1765) {
        moonbit_decref(_M0L7_2abindS1765);
      }
      moonbit_decref(_M0L5_2aitS1760);
    } else {
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1766 =
        _M0L7_2abindS1765;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L4_2axS1767 =
        _M0L7_2aSomeS1766;
      moonbit_string_t _M0L8_2afieldS3631 = _M0L4_2axS1767->$0;
      int32_t _M0L6_2acntS4009 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1767));
      moonbit_string_t _M0L6_2akeyS1768;
      if (_M0L6_2acntS4009 > 1) {
        int32_t _M0L11_2anew__cntS4011 = _M0L6_2acntS4009 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1767), _M0L11_2anew__cntS4011);
        moonbit_incref(_M0L8_2afieldS3631);
      } else if (_M0L6_2acntS4009 == 1) {
        void* _M0L8_2afieldS4010 = _M0L4_2axS1767->$1;
        moonbit_decref(_M0L8_2afieldS4010);
        #line 1505 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1767);
      }
      _M0L6_2akeyS1768 = _M0L8_2afieldS3631;
      _M0L3keyS1763 = _M0L6_2akeyS1768;
      goto join_1762;
    }
    goto joinlet_4099;
    join_1762:;
    #line 1506 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3623
    = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1761, _M0L3keyS1763);
    if (!_M0L6_2atmpS3623) {
      #line 1507 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L9all__keysS1759, _M0L3keyS1763);
      moonbit_decref(_M0L3keyS1763);
    } else {
      moonbit_decref(_M0L3keyS1763);
    }
    continue;
    joinlet_4099:;
    break;
  }
  #line 1510 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3625 = _M0MPC15array5Array6lengthGsE(_M0L9all__keysS1759);
  if (_M0L6_2atmpS3625 == 0) {
    moonbit_decref(_M0L9all__keysS1759);
    return 0;
  } else {
    int32_t _M0L13current__timeS3627 = _M0L4selfS1761->$2;
    int32_t _M0L6_2atmpS3628;
    int32_t _M0L3idxS1769;
    int32_t _M0L9safe__idxS1770;
    moonbit_string_t _M0L6_2atmpS3626;
    #line 1513 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3628 = _M0MPC15array5Array6lengthGsE(_M0L9all__keysS1759);
    _M0L3idxS1769 = _M0L13current__timeS3627 % _M0L6_2atmpS3628;
    if (_M0L3idxS1769 < 0) {
      _M0L9safe__idxS1770 = -_M0L3idxS1769;
    } else {
      _M0L9safe__idxS1770 = _M0L3idxS1769;
    }
    #line 1515 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3626
    = _M0MPC15array5Array2atGsE(_M0L9all__keysS1759, _M0L9safe__idxS1770);
    moonbit_decref(_M0L9all__keysS1759);
    return _M0L6_2atmpS3626;
  }
}

int32_t _M0MP19moonbitDB8Database6dbsize(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1751
) {
  struct _M0TPB8MutLocalGiE* _M0L5countS1749;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3622;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L5_2aitS1750;
  int32_t _result_4102;
  #line 1484 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5countS1749
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5countS1749)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5countS1749->$0 = 0;
  _M0L4dataS3622 = _M0L4selfS1751->$0;
  moonbit_incref(_M0L4dataS3622);
  #line 1485 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1750
  = _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(_M0L4dataS3622);
  moonbit_decref(_M0L4dataS3622);
  while (1) {
    moonbit_string_t _M0L3keyS1753;
    struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2abindS1755;
    int32_t _M0L6_2atmpS3619;
    #line 1486 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1755
    = _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(_M0L5_2aitS1750);
    if (_M0L7_2abindS1755 == 0) {
      if (_M0L7_2abindS1755) {
        moonbit_decref(_M0L7_2abindS1755);
      }
      moonbit_decref(_M0L5_2aitS1750);
    } else {
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1756 =
        _M0L7_2abindS1755;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L4_2axS1757 =
        _M0L7_2aSomeS1756;
      moonbit_string_t _M0L8_2afieldS3633 = _M0L4_2axS1757->$0;
      int32_t _M0L6_2acntS4012 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1757));
      moonbit_string_t _M0L6_2akeyS1758;
      if (_M0L6_2acntS4012 > 1) {
        int32_t _M0L11_2anew__cntS4014 = _M0L6_2acntS4012 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1757), _M0L11_2anew__cntS4014);
        moonbit_incref(_M0L8_2afieldS3633);
      } else if (_M0L6_2acntS4012 == 1) {
        void* _M0L8_2afieldS4013 = _M0L4_2axS1757->$1;
        moonbit_decref(_M0L8_2afieldS4013);
        #line 1486 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1757);
      }
      _M0L6_2akeyS1758 = _M0L8_2afieldS3633;
      _M0L3keyS1753 = _M0L6_2akeyS1758;
      goto join_1752;
    }
    goto joinlet_4101;
    join_1752:;
    #line 1487 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3619
    = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1751, _M0L3keyS1753);
    moonbit_decref(_M0L3keyS1753);
    if (!_M0L6_2atmpS3619) {
      int32_t _M0L3valS3621 = _M0L5countS1749->$0;
      int32_t _M0L6_2atmpS3620 = _M0L3valS3621 + 1;
      _M0L5countS1749->$0 = _M0L6_2atmpS3620;
    }
    continue;
    joinlet_4101:;
    break;
  }
  _result_4102 = _M0L5countS1749->$0;
  moonbit_decref(_M0L5countS1749);
  return _result_4102;
}

int64_t _M0MP19moonbitDB8Database5zrank(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1734,
  moonbit_string_t _M0L3keyS1735,
  moonbit_string_t _M0L11member__valS1739
) {
  #line 1328 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1329 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1734, _M0L3keyS1735)
  ) {
    return 4294967296ll;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1738;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3618 =
      _M0L4selfS1734->$0;
    void* _M0L7_2abindS1744;
    int32_t _M0L6_2atmpS3612;
    moonbit_incref(_M0L4dataS3618);
    #line 1332 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1744
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3618, _M0L3keyS1735);
    moonbit_decref(_M0L4dataS3618);
    if (_M0L7_2abindS1744 == 0) {
      if (_M0L7_2abindS1744) {
        moonbit_decref(_M0L7_2abindS1744);
      }
      goto join_1736;
    } else {
      void* _M0L7_2aSomeS1745 = _M0L7_2abindS1744;
      void* _M0L4_2axS1746 = _M0L7_2aSomeS1745;
      switch (Moonbit_object_tag(_M0L4_2axS1746)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1747 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1746;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3636 = _M0L7_2aZSetS1747->$0;
          int32_t _M0L6_2acntS4017 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1747));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1748;
          if (_M0L6_2acntS4017 > 1) {
            int32_t _M0L11_2anew__cntS4018 = _M0L6_2acntS4017 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1747), _M0L11_2anew__cntS4018);
            moonbit_incref(_M0L8_2afieldS3636);
          } else if (_M0L6_2acntS4017 == 1) {
            #line 1332 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1747);
          }
          _M0L7_2azsetS1748 = _M0L8_2afieldS3636;
          _M0L4zsetS1738 = _M0L7_2azsetS1748;
          goto join_1737;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1746);
          goto join_1736;
          break;
        }
      }
    }
    join_1737:;
    #line 1334 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3612
    = _M0MPB3Map8containsGsfE(_M0L4zsetS1738, _M0L11member__valS1739);
    moonbit_decref(_M0L4zsetS1738);
    if (!_M0L6_2atmpS3612) {
      return 4294967296ll;
    } else {
      struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1740;
      struct _M0TPB8MutLocalGiE* _M0L4rankS1741;
      int32_t _M0L1iS1742;
      int32_t _M0L3valS3617;
      #line 1337 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6sortedS1740
      = _M0MP19moonbitDB8Database17get__sorted__zset(_M0L4selfS1734, _M0L3keyS1735);
      _M0L4rankS1741
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L4rankS1741)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L4rankS1741->$0 = 0;
      _M0L1iS1742 = 0;
      while (1) {
        int32_t _M0L6_2atmpS3613;
        #line 1339 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3613 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1740);
        if (_M0L1iS1742 < _M0L6_2atmpS3613) {
          struct _M0TUsfE* _M0L6_2atmpS3615;
          moonbit_string_t _M0L8_2afieldS3635;
          int32_t _M0L6_2acntS4015;
          moonbit_string_t _M0L6_2atmpS3614;
          int32_t _result_4106;
          int32_t _M0L6_2atmpS3616;
          #line 1340 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0L6_2atmpS3615
          = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1740, _M0L1iS1742);
          _M0L8_2afieldS3635 = _M0L6_2atmpS3615->$0;
          _M0L6_2acntS4015
          = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3615));
          if (_M0L6_2acntS4015 > 1) {
            int32_t _M0L11_2anew__cntS4016 = _M0L6_2acntS4015 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3615), _M0L11_2anew__cntS4016);
            moonbit_incref(_M0L8_2afieldS3635);
          } else if (_M0L6_2acntS4015 == 1) {
            #line 1340 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2atmpS3615);
          }
          _M0L6_2atmpS3614 = _M0L8_2afieldS3635;
          #line 1340 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _result_4106
          = _M0L6_2atmpS3614 == _M0L11member__valS1739
            || Moonbit_array_length(_M0L6_2atmpS3614)
               == Moonbit_array_length(_M0L11member__valS1739)
               && 0
                  == memcmp(_M0L6_2atmpS3614, _M0L11member__valS1739, Moonbit_array_length(_M0L6_2atmpS3614) * 2);
          moonbit_decref(_M0L6_2atmpS3614);
          if (_result_4106) {
            moonbit_decref(_M0L6sortedS1740);
            _M0L4rankS1741->$0 = _M0L1iS1742;
            break;
          }
          _M0L6_2atmpS3616 = _M0L1iS1742 + 1;
          _M0L1iS1742 = _M0L6_2atmpS3616;
          continue;
        } else {
          moonbit_decref(_M0L6sortedS1740);
        }
        break;
      }
      _M0L3valS3617 = _M0L4rankS1741->$0;
      moonbit_decref(_M0L4rankS1741);
      return (int64_t)_M0L3valS3617;
    }
    join_1736:;
    return 4294967296ll;
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database9zrevrange(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1723,
  moonbit_string_t _M0L3keyS1724,
  int32_t _M0L5startS1728,
  int32_t _M0L3endS1730
) {
  struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1722;
  int32_t _M0L3lenS1725;
  moonbit_string_t* _M0L6_2atmpS3611;
  struct _M0TPB5ArrayGsE* _M0L6resultS1726;
  int32_t _M0L10start__idxS1727;
  int32_t _M0L8end__idxS1729;
  int32_t _M0L1iS1731;
  #line 1302 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1303 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6sortedS1722
  = _M0MP19moonbitDB8Database17get__sorted__zset(_M0L4selfS1723, _M0L3keyS1724);
  #line 1304 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L3lenS1725 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1722);
  _M0L6_2atmpS3611 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1726
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1726)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6resultS1726->$0 = _M0L6_2atmpS3611;
  _M0L6resultS1726->$1 = 0;
  if (_M0L5startS1728 < 0) {
    _M0L10start__idxS1727 = _M0L3lenS1725 + _M0L5startS1728;
  } else {
    _M0L10start__idxS1727 = _M0L5startS1728;
  }
  if (_M0L3endS1730 < 0) {
    _M0L8end__idxS1729 = _M0L3lenS1725 + _M0L3endS1730;
  } else {
    _M0L8end__idxS1729 = _M0L3endS1730;
  }
  _M0L1iS1731 = _M0L10start__idxS1727;
  while (1) {
    int32_t _if__result_4108;
    if (_M0L1iS1731 <= _M0L8end__idxS1729) {
      _if__result_4108 = _M0L1iS1731 < _M0L3lenS1725;
    } else {
      _if__result_4108 = 0;
    }
    if (_if__result_4108) {
      int32_t _M0L6_2atmpS3609 = _M0L3lenS1725 - 1;
      int32_t _M0L8rev__idxS1732 = _M0L6_2atmpS3609 - _M0L1iS1731;
      int32_t _M0L6_2atmpS3610;
      if (_M0L8rev__idxS1732 >= 0) {
        struct _M0TUsfE* _M0L6_2atmpS3608;
        moonbit_string_t _M0L8_2afieldS3638;
        int32_t _M0L6_2acntS4019;
        moonbit_string_t _M0L6_2atmpS3607;
        #line 1311 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3608
        = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1722, _M0L8rev__idxS1732);
        _M0L8_2afieldS3638 = _M0L6_2atmpS3608->$0;
        _M0L6_2acntS4019
        = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3608));
        if (_M0L6_2acntS4019 > 1) {
          int32_t _M0L11_2anew__cntS4020 = _M0L6_2acntS4019 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3608), _M0L11_2anew__cntS4020);
          moonbit_incref(_M0L8_2afieldS3638);
        } else if (_M0L6_2acntS4019 == 1) {
          #line 1311 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L6_2atmpS3608);
        }
        _M0L6_2atmpS3607 = _M0L8_2afieldS3638;
        #line 1311 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1726, _M0L6_2atmpS3607);
        moonbit_decref(_M0L6_2atmpS3607);
      }
      _M0L6_2atmpS3610 = _M0L1iS1731 + 1;
      _M0L1iS1731 = _M0L6_2atmpS3610;
      continue;
    } else {
      moonbit_decref(_M0L6sortedS1722);
    }
    break;
  }
  return _M0L6resultS1726;
}

struct _M0TPB5ArrayGUsfEE* _M0MP19moonbitDB8Database17get__sorted__zset(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1701,
  moonbit_string_t _M0L3keyS1702
) {
  #line 1263 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1264 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1701, _M0L3keyS1702)
  ) {
    struct _M0TUsfE** _M0L6_2atmpS3602 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _block_4109 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_4109)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
    _block_4109->$0 = _M0L6_2atmpS3602;
    _block_4109->$1 = 0;
    return _block_4109;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1705;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3606 =
      _M0L4selfS1701->$0;
    void* _M0L7_2abindS1717;
    struct _M0TUsfE** _M0L6_2atmpS3605;
    struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1706;
    struct _M0TPB4IterGUsfEE* _M0L5_2aitS1707;
    struct _M0TPB5ArrayGUsfEE* _result_4114;
    struct _M0TUsfE** _M0L6_2atmpS3603;
    struct _M0TPB5ArrayGUsfEE* _block_4115;
    moonbit_incref(_M0L4dataS3606);
    #line 1267 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1717
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3606, _M0L3keyS1702);
    moonbit_decref(_M0L4dataS3606);
    if (_M0L7_2abindS1717 == 0) {
      if (_M0L7_2abindS1717) {
        moonbit_decref(_M0L7_2abindS1717);
      }
      goto join_1703;
    } else {
      void* _M0L7_2aSomeS1718 = _M0L7_2abindS1717;
      void* _M0L4_2axS1719 = _M0L7_2aSomeS1718;
      switch (Moonbit_object_tag(_M0L4_2axS1719)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1720 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1719;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3640 = _M0L7_2aZSetS1720->$0;
          int32_t _M0L6_2acntS4023 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1720));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1721;
          if (_M0L6_2acntS4023 > 1) {
            int32_t _M0L11_2anew__cntS4024 = _M0L6_2acntS4023 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1720), _M0L11_2anew__cntS4024);
            moonbit_incref(_M0L8_2afieldS3640);
          } else if (_M0L6_2acntS4023 == 1) {
            #line 1267 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1720);
          }
          _M0L7_2azsetS1721 = _M0L8_2afieldS3640;
          _M0L4zsetS1705 = _M0L7_2azsetS1721;
          goto join_1704;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1719);
          goto join_1703;
          break;
        }
      }
    }
    join_1704:;
    _M0L6_2atmpS3605 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5itemsS1706
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5itemsS1706)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
    _M0L5itemsS1706->$0 = _M0L6_2atmpS3605;
    _M0L5itemsS1706->$1 = 0;
    #line 1269 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L5_2aitS1707 = _M0MPB3Map5iter2GsfE(_M0L4zsetS1705);
    moonbit_decref(_M0L4zsetS1705);
    while (1) {
      moonbit_string_t _M0L1mS1709;
      float _M0L1sS1710;
      struct _M0TUsfE* _M0L7_2abindS1712;
      struct _M0TUsfE* _M0L8_2atupleS3604;
      #line 1270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L7_2abindS1712 = _M0MPB5Iter24nextGsfE(_M0L5_2aitS1707);
      if (_M0L7_2abindS1712 == 0) {
        if (_M0L7_2abindS1712) {
          moonbit_decref(_M0L7_2abindS1712);
        }
        moonbit_decref(_M0L5_2aitS1707);
      } else {
        struct _M0TUsfE* _M0L7_2aSomeS1713 = _M0L7_2abindS1712;
        struct _M0TUsfE* _M0L4_2axS1714 = _M0L7_2aSomeS1713;
        moonbit_string_t _M0L4_2amS1715 = _M0L4_2axS1714->$0;
        float _M0L4_2asS1716 = _M0L4_2axS1714->$1;
        int32_t _M0L6_2acntS4021 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1714));
        if (_M0L6_2acntS4021 > 1) {
          int32_t _M0L11_2anew__cntS4022 = _M0L6_2acntS4021 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1714), _M0L11_2anew__cntS4022);
          moonbit_incref(_M0L4_2amS1715);
        } else if (_M0L6_2acntS4021 == 1) {
          #line 1270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L4_2axS1714);
        }
        _M0L1mS1709 = _M0L4_2amS1715;
        _M0L1sS1710 = _M0L4_2asS1716;
        goto join_1708;
      }
      goto joinlet_4113;
      join_1708:;
      _M0L8_2atupleS3604
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS3604)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
      _M0L8_2atupleS3604->$0 = _M0L1mS1709;
      _M0L8_2atupleS3604->$1 = _M0L1sS1710;
      #line 1271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L5itemsS1706, _M0L8_2atupleS3604);
      moonbit_decref(_M0L8_2atupleS3604);
      continue;
      joinlet_4113:;
      break;
    }
    #line 1273 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4114 = _M0FP19moonbitDB15sort__by__score(_M0L5itemsS1706);
    moonbit_decref(_M0L5itemsS1706);
    return _result_4114;
    join_1703:;
    _M0L6_2atmpS3603 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _block_4115
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_4115)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
    _block_4115->$0 = _M0L6_2atmpS3603;
    _block_4115->$1 = 0;
    return _block_4115;
  }
}

void* _M0MP19moonbitDB8Database6zscore(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1690,
  moonbit_string_t _M0L3keyS1691,
  moonbit_string_t _M0L11member__valS1695
) {
  #line 1234 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1235 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1690, _M0L3keyS1691)
  ) {
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1694;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3601 =
      _M0L4selfS1690->$0;
    void* _M0L7_2abindS1696;
    void* _result_4118;
    moonbit_incref(_M0L4dataS3601);
    #line 1238 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1696
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3601, _M0L3keyS1691);
    moonbit_decref(_M0L4dataS3601);
    if (_M0L7_2abindS1696 == 0) {
      if (_M0L7_2abindS1696) {
        moonbit_decref(_M0L7_2abindS1696);
      }
      goto join_1692;
    } else {
      void* _M0L7_2aSomeS1697 = _M0L7_2abindS1696;
      void* _M0L4_2axS1698 = _M0L7_2aSomeS1697;
      switch (Moonbit_object_tag(_M0L4_2axS1698)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1699 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1698;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3642 = _M0L7_2aZSetS1699->$0;
          int32_t _M0L6_2acntS4025 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1699));
          struct _M0TPB3MapGsfE* _M0L4_2azS1700;
          if (_M0L6_2acntS4025 > 1) {
            int32_t _M0L11_2anew__cntS4026 = _M0L6_2acntS4025 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1699), _M0L11_2anew__cntS4026);
            moonbit_incref(_M0L8_2afieldS3642);
          } else if (_M0L6_2acntS4025 == 1) {
            #line 1238 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1699);
          }
          _M0L4_2azS1700 = _M0L8_2afieldS3642;
          _M0L1zS1694 = _M0L4_2azS1700;
          goto join_1693;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1698);
          goto join_1692;
          break;
        }
      }
    }
    join_1693:;
    #line 1239 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4118 = _M0MPB3Map3getGsfE(_M0L1zS1694, _M0L11member__valS1695);
    moonbit_decref(_M0L1zS1694);
    return _result_4118;
    join_1692:;
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
}

int32_t _M0MP19moonbitDB8Database5zcard(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1681,
  moonbit_string_t _M0L3keyS1682
) {
  #line 1223 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1224 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1681, _M0L3keyS1682)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1684;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3600 =
      _M0L4selfS1681->$0;
    void* _M0L7_2abindS1685;
    int32_t _result_4120;
    moonbit_incref(_M0L4dataS3600);
    #line 1227 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1685
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3600, _M0L3keyS1682);
    moonbit_decref(_M0L4dataS3600);
    if (_M0L7_2abindS1685 == 0) {
      if (_M0L7_2abindS1685) {
        moonbit_decref(_M0L7_2abindS1685);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1686 = _M0L7_2abindS1685;
      void* _M0L4_2axS1687 = _M0L7_2aSomeS1686;
      switch (Moonbit_object_tag(_M0L4_2axS1687)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1688 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1687;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3644 = _M0L7_2aZSetS1688->$0;
          int32_t _M0L6_2acntS4027 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1688));
          struct _M0TPB3MapGsfE* _M0L4_2azS1689;
          if (_M0L6_2acntS4027 > 1) {
            int32_t _M0L11_2anew__cntS4028 = _M0L6_2acntS4027 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1688), _M0L11_2anew__cntS4028);
            moonbit_incref(_M0L8_2afieldS3644);
          } else if (_M0L6_2acntS4027 == 1) {
            #line 1227 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1688);
          }
          _M0L4_2azS1689 = _M0L8_2afieldS3644;
          _M0L1zS1684 = _M0L4_2azS1689;
          goto join_1683;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1687);
          return 0;
          break;
        }
      }
    }
    join_1683:;
    #line 1228 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4120 = _M0MPB3Map6lengthGsfE(_M0L1zS1684);
    moonbit_decref(_M0L1zS1684);
    return _result_4120;
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database6zrange(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1651,
  moonbit_string_t _M0L3keyS1652,
  int32_t _M0L5startS1671,
  int32_t _M0L3endS1673
) {
  #line 1152 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1153 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1651, _M0L3keyS1652)
  ) {
    moonbit_string_t* _M0L6_2atmpS3591 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4121 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4121)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_4121->$0 = _M0L6_2atmpS3591;
    _block_4121->$1 = 0;
    return _block_4121;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1655;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3599 =
      _M0L4selfS1651->$0;
    void* _M0L7_2abindS1676;
    struct _M0TUsfE** _M0L6_2atmpS3598;
    struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1656;
    struct _M0TPB4IterGUsfEE* _M0L5_2aitS1657;
    struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1667;
    int32_t _M0L3lenS1668;
    moonbit_string_t* _M0L6_2atmpS3597;
    struct _M0TPB5ArrayGsE* _M0L6resultS1669;
    int32_t _M0L10start__idxS1670;
    int32_t _M0L8end__idxS1672;
    int32_t _M0L1iS1674;
    moonbit_string_t* _M0L6_2atmpS3592;
    struct _M0TPB5ArrayGsE* _block_4128;
    moonbit_incref(_M0L4dataS3599);
    #line 1156 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1676
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3599, _M0L3keyS1652);
    moonbit_decref(_M0L4dataS3599);
    if (_M0L7_2abindS1676 == 0) {
      if (_M0L7_2abindS1676) {
        moonbit_decref(_M0L7_2abindS1676);
      }
      goto join_1653;
    } else {
      void* _M0L7_2aSomeS1677 = _M0L7_2abindS1676;
      void* _M0L4_2axS1678 = _M0L7_2aSomeS1677;
      switch (Moonbit_object_tag(_M0L4_2axS1678)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1679 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1678;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3648 = _M0L7_2aZSetS1679->$0;
          int32_t _M0L6_2acntS4033 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1679));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1680;
          if (_M0L6_2acntS4033 > 1) {
            int32_t _M0L11_2anew__cntS4034 = _M0L6_2acntS4033 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1679), _M0L11_2anew__cntS4034);
            moonbit_incref(_M0L8_2afieldS3648);
          } else if (_M0L6_2acntS4033 == 1) {
            #line 1156 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1679);
          }
          _M0L7_2azsetS1680 = _M0L8_2afieldS3648;
          _M0L4zsetS1655 = _M0L7_2azsetS1680;
          goto join_1654;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1678);
          goto join_1653;
          break;
        }
      }
    }
    join_1654:;
    _M0L6_2atmpS3598 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5itemsS1656
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5itemsS1656)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
    _M0L5itemsS1656->$0 = _M0L6_2atmpS3598;
    _M0L5itemsS1656->$1 = 0;
    #line 1158 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L5_2aitS1657 = _M0MPB3Map5iter2GsfE(_M0L4zsetS1655);
    moonbit_decref(_M0L4zsetS1655);
    while (1) {
      moonbit_string_t _M0L1mS1659;
      float _M0L1sS1660;
      struct _M0TUsfE* _M0L7_2abindS1662;
      struct _M0TUsfE* _M0L8_2atupleS3593;
      #line 1159 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L7_2abindS1662 = _M0MPB5Iter24nextGsfE(_M0L5_2aitS1657);
      if (_M0L7_2abindS1662 == 0) {
        if (_M0L7_2abindS1662) {
          moonbit_decref(_M0L7_2abindS1662);
        }
        moonbit_decref(_M0L5_2aitS1657);
      } else {
        struct _M0TUsfE* _M0L7_2aSomeS1663 = _M0L7_2abindS1662;
        struct _M0TUsfE* _M0L4_2axS1664 = _M0L7_2aSomeS1663;
        moonbit_string_t _M0L4_2amS1665 = _M0L4_2axS1664->$0;
        float _M0L4_2asS1666 = _M0L4_2axS1664->$1;
        int32_t _M0L6_2acntS4029 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1664));
        if (_M0L6_2acntS4029 > 1) {
          int32_t _M0L11_2anew__cntS4030 = _M0L6_2acntS4029 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1664), _M0L11_2anew__cntS4030);
          moonbit_incref(_M0L4_2amS1665);
        } else if (_M0L6_2acntS4029 == 1) {
          #line 1159 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L4_2axS1664);
        }
        _M0L1mS1659 = _M0L4_2amS1665;
        _M0L1sS1660 = _M0L4_2asS1666;
        goto join_1658;
      }
      goto joinlet_4125;
      join_1658:;
      _M0L8_2atupleS3593
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS3593)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
      _M0L8_2atupleS3593->$0 = _M0L1mS1659;
      _M0L8_2atupleS3593->$1 = _M0L1sS1660;
      #line 1160 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L5itemsS1656, _M0L8_2atupleS3593);
      moonbit_decref(_M0L8_2atupleS3593);
      continue;
      joinlet_4125:;
      break;
    }
    #line 1162 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6sortedS1667 = _M0FP19moonbitDB15sort__by__score(_M0L5itemsS1656);
    moonbit_decref(_M0L5itemsS1656);
    #line 1163 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3lenS1668 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1667);
    _M0L6_2atmpS3597 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1669
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1669)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS1669->$0 = _M0L6_2atmpS3597;
    _M0L6resultS1669->$1 = 0;
    if (_M0L5startS1671 < 0) {
      _M0L10start__idxS1670 = _M0L3lenS1668 + _M0L5startS1671;
    } else {
      _M0L10start__idxS1670 = _M0L5startS1671;
    }
    if (_M0L3endS1673 < 0) {
      _M0L8end__idxS1672 = _M0L3lenS1668 + _M0L3endS1673;
    } else {
      _M0L8end__idxS1672 = _M0L3endS1673;
    }
    _M0L1iS1674 = _M0L10start__idxS1670;
    while (1) {
      int32_t _if__result_4127;
      if (_M0L1iS1674 <= _M0L8end__idxS1672) {
        _if__result_4127 = _M0L1iS1674 < _M0L3lenS1668;
      } else {
        _if__result_4127 = 0;
      }
      if (_if__result_4127) {
        struct _M0TUsfE* _M0L6_2atmpS3595;
        moonbit_string_t _M0L8_2afieldS3646;
        int32_t _M0L6_2acntS4031;
        moonbit_string_t _M0L6_2atmpS3594;
        int32_t _M0L6_2atmpS3596;
        #line 1168 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3595
        = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1667, _M0L1iS1674);
        _M0L8_2afieldS3646 = _M0L6_2atmpS3595->$0;
        _M0L6_2acntS4031
        = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3595));
        if (_M0L6_2acntS4031 > 1) {
          int32_t _M0L11_2anew__cntS4032 = _M0L6_2acntS4031 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3595), _M0L11_2anew__cntS4032);
          moonbit_incref(_M0L8_2afieldS3646);
        } else if (_M0L6_2acntS4031 == 1) {
          #line 1168 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L6_2atmpS3595);
        }
        _M0L6_2atmpS3594 = _M0L8_2afieldS3646;
        #line 1168 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1669, _M0L6_2atmpS3594);
        moonbit_decref(_M0L6_2atmpS3594);
        _M0L6_2atmpS3596 = _M0L1iS1674 + 1;
        _M0L1iS1674 = _M0L6_2atmpS3596;
        continue;
      } else {
        moonbit_decref(_M0L6sortedS1667);
      }
      break;
    }
    return _M0L6resultS1669;
    join_1653:;
    _M0L6_2atmpS3592 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_4128
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4128)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_4128->$0 = _M0L6_2atmpS3592;
    _block_4128->$1 = 0;
    return _block_4128;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP19moonbitDB15sort__by__score(
  struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1650
) {
  #line 1177 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1178 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  return _M0FP19moonbitDB11merge__sort(_M0L5itemsS1650);
}

struct _M0TPB5ArrayGUsfEE* _M0FP19moonbitDB11merge__sort(
  struct _M0TPB5ArrayGUsfEE* _M0L3arrS1642
) {
  int32_t _M0L3lenS1641;
  #line 1181 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1182 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L3lenS1641 = _M0MPC15array5Array6lengthGUsfEE(_M0L3arrS1642);
  if (_M0L3lenS1641 <= 1) {
    moonbit_incref(_M0L3arrS1642);
    return _M0L3arrS1642;
  } else {
    int32_t _M0L3midS1643 = _M0L3lenS1641 / 2;
    struct _M0TUsfE** _M0L6_2atmpS3590 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _M0L4leftS1644 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    struct _M0TUsfE** _M0L6_2atmpS3589;
    struct _M0TPB5ArrayGUsfEE* _M0L5rightS1645;
    int32_t _M0L1iS1646;
    int32_t _M0L1iS1648;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS3587;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS3588;
    struct _M0TPB5ArrayGUsfEE* _result_4131;
    Moonbit_object_header(_M0L4leftS1644)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
    _M0L4leftS1644->$0 = _M0L6_2atmpS3590;
    _M0L4leftS1644->$1 = 0;
    _M0L6_2atmpS3589 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5rightS1645
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5rightS1645)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
    _M0L5rightS1645->$0 = _M0L6_2atmpS3589;
    _M0L5rightS1645->$1 = 0;
    _M0L1iS1646 = 0;
    while (1) {
      if (_M0L1iS1646 < _M0L3midS1643) {
        struct _M0TUsfE* _M0L6_2atmpS3583;
        int32_t _M0L6_2atmpS3584;
        #line 1190 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3583
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1642, _M0L1iS1646);
        #line 1190 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L4leftS1644, _M0L6_2atmpS3583);
        moonbit_decref(_M0L6_2atmpS3583);
        _M0L6_2atmpS3584 = _M0L1iS1646 + 1;
        _M0L1iS1646 = _M0L6_2atmpS3584;
        continue;
      }
      break;
    }
    _M0L1iS1648 = _M0L3midS1643;
    while (1) {
      if (_M0L1iS1648 < _M0L3lenS1641) {
        struct _M0TUsfE* _M0L6_2atmpS3585;
        int32_t _M0L6_2atmpS3586;
        #line 1193 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3585
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1642, _M0L1iS1648);
        #line 1193 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L5rightS1645, _M0L6_2atmpS3585);
        moonbit_decref(_M0L6_2atmpS3585);
        _M0L6_2atmpS3586 = _M0L1iS1648 + 1;
        _M0L1iS1648 = _M0L6_2atmpS3586;
        continue;
      }
      break;
    }
    #line 1195 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3587 = _M0FP19moonbitDB11merge__sort(_M0L4leftS1644);
    moonbit_decref(_M0L4leftS1644);
    #line 1195 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3588 = _M0FP19moonbitDB11merge__sort(_M0L5rightS1645);
    moonbit_decref(_M0L5rightS1645);
    #line 1195 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4131 = _M0FP19moonbitDB5merge(_M0L6_2atmpS3587, _M0L6_2atmpS3588);
    moonbit_decref(_M0L6_2atmpS3587);
    moonbit_decref(_M0L6_2atmpS3588);
    return _result_4131;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP19moonbitDB5merge(
  struct _M0TPB5ArrayGUsfEE* _M0L4leftS1636,
  struct _M0TPB5ArrayGUsfEE* _M0L5rightS1637
) {
  struct _M0TUsfE** _M0L6_2atmpS3582;
  struct _M0TPB5ArrayGUsfEE* _M0L6resultS1633;
  struct _M0TPB8MutLocalGiE* _M0L1iS1634;
  struct _M0TPB8MutLocalGiE* _M0L1jS1635;
  #line 1199 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3582 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6resultS1633
  = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
  Moonbit_object_header(_M0L6resultS1633)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 3, 0);
  _M0L6resultS1633->$0 = _M0L6_2atmpS3582;
  _M0L6resultS1633->$1 = 0;
  _M0L1iS1634
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1634)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS1634->$0 = 0;
  _M0L1jS1635
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1jS1635)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1jS1635->$0 = 0;
  while (1) {
    int32_t _M0L3valS3554 = _M0L1iS1634->$0;
    int32_t _M0L6_2atmpS3555;
    int32_t _if__result_4133;
    #line 1203 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3555 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1636);
    if (_M0L3valS3554 < _M0L6_2atmpS3555) {
      int32_t _M0L3valS3552 = _M0L1jS1635->$0;
      int32_t _M0L6_2atmpS3553;
      #line 1203 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3553 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1637);
      _if__result_4133 = _M0L3valS3552 < _M0L6_2atmpS3553;
    } else {
      _if__result_4133 = 0;
    }
    if (_if__result_4133) {
      int32_t _M0L3valS3561 = _M0L1iS1634->$0;
      struct _M0TUsfE* _M0L6_2atmpS3560;
      float _M0L6_2atmpS3556;
      int32_t _M0L3valS3559;
      struct _M0TUsfE* _M0L6_2atmpS3558;
      float _M0L6_2atmpS3557;
      #line 1204 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3560
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1636, _M0L3valS3561);
      _M0L6_2atmpS3556 = _M0L6_2atmpS3560->$1;
      moonbit_decref(_M0L6_2atmpS3560);
      _M0L3valS3559 = _M0L1jS1635->$0;
      #line 1204 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3558
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1637, _M0L3valS3559);
      _M0L6_2atmpS3557 = _M0L6_2atmpS3558->$1;
      moonbit_decref(_M0L6_2atmpS3558);
      if (_M0L6_2atmpS3556 <= _M0L6_2atmpS3557) {
        int32_t _M0L3valS3563 = _M0L1iS1634->$0;
        struct _M0TUsfE* _M0L6_2atmpS3562;
        int32_t _M0L3valS3565;
        int32_t _M0L6_2atmpS3564;
        #line 1205 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3562
        = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1636, _M0L3valS3563);
        #line 1205 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1633, _M0L6_2atmpS3562);
        moonbit_decref(_M0L6_2atmpS3562);
        _M0L3valS3565 = _M0L1iS1634->$0;
        _M0L6_2atmpS3564 = _M0L3valS3565 + 1;
        _M0L1iS1634->$0 = _M0L6_2atmpS3564;
      } else {
        int32_t _M0L3valS3567 = _M0L1jS1635->$0;
        struct _M0TUsfE* _M0L6_2atmpS3566;
        int32_t _M0L3valS3569;
        int32_t _M0L6_2atmpS3568;
        #line 1208 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3566
        = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1637, _M0L3valS3567);
        #line 1208 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1633, _M0L6_2atmpS3566);
        moonbit_decref(_M0L6_2atmpS3566);
        _M0L3valS3569 = _M0L1jS1635->$0;
        _M0L6_2atmpS3568 = _M0L3valS3569 + 1;
        _M0L1jS1635->$0 = _M0L6_2atmpS3568;
      }
      continue;
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS3570 = _M0L1iS1634->$0;
    int32_t _M0L6_2atmpS3571;
    #line 1212 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3571 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1636);
    if (_M0L3valS3570 < _M0L6_2atmpS3571) {
      int32_t _M0L3valS3573 = _M0L1iS1634->$0;
      struct _M0TUsfE* _M0L6_2atmpS3572;
      int32_t _M0L3valS3575;
      int32_t _M0L6_2atmpS3574;
      #line 1213 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3572
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1636, _M0L3valS3573);
      #line 1213 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1633, _M0L6_2atmpS3572);
      moonbit_decref(_M0L6_2atmpS3572);
      _M0L3valS3575 = _M0L1iS1634->$0;
      _M0L6_2atmpS3574 = _M0L3valS3575 + 1;
      _M0L1iS1634->$0 = _M0L6_2atmpS3574;
      continue;
    } else {
      moonbit_decref(_M0L1iS1634);
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS3576 = _M0L1jS1635->$0;
    int32_t _M0L6_2atmpS3577;
    #line 1216 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3577 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1637);
    if (_M0L3valS3576 < _M0L6_2atmpS3577) {
      int32_t _M0L3valS3579 = _M0L1jS1635->$0;
      struct _M0TUsfE* _M0L6_2atmpS3578;
      int32_t _M0L3valS3581;
      int32_t _M0L6_2atmpS3580;
      #line 1217 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3578
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1637, _M0L3valS3579);
      #line 1217 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1633, _M0L6_2atmpS3578);
      moonbit_decref(_M0L6_2atmpS3578);
      _M0L3valS3581 = _M0L1jS1635->$0;
      _M0L6_2atmpS3580 = _M0L3valS3581 + 1;
      _M0L1jS1635->$0 = _M0L6_2atmpS3580;
      continue;
    } else {
      moonbit_decref(_M0L1jS1635);
    }
    break;
  }
  return _M0L6resultS1633;
}

int32_t _M0MP19moonbitDB8Database4zadd(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1618,
  moonbit_string_t _M0L3keyS1619,
  float _M0L5scoreS1632,
  moonbit_string_t _M0L11member__valS1631
) {
  int32_t _M0L6_2atmpS3546;
  struct _M0TPB3MapGsfE* _M0L4zsetS1620;
  struct _M0TPB3MapGsfE* _M0L1zS1624;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3551;
  void* _M0L7_2abindS1625;
  struct _M0TUsfE** _M0L7_2abindS1622;
  struct _M0TUsfE** _M0L6_2atmpS3550;
  struct _M0TPB9ArrayViewGUsfEE _M0L6_2atmpS3549;
  int32_t _M0L7existedS1630;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3547;
  void* _M0L4ZSetS3548;
  #line 1140 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1141 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3546
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1618, _M0L3keyS1619);
  _M0L4dataS3551 = _M0L4selfS1618->$0;
  moonbit_incref(_M0L4dataS3551);
  #line 1142 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1625
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3551, _M0L3keyS1619);
  moonbit_decref(_M0L4dataS3551);
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
        struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1628 =
          (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1627;
        struct _M0TPB3MapGsfE* _M0L8_2afieldS3651 = _M0L7_2aZSetS1628->$0;
        int32_t _M0L6_2acntS4035 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1628));
        struct _M0TPB3MapGsfE* _M0L4_2azS1629;
        if (_M0L6_2acntS4035 > 1) {
          int32_t _M0L11_2anew__cntS4036 = _M0L6_2acntS4035 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1628), _M0L11_2anew__cntS4036);
          moonbit_incref(_M0L8_2afieldS3651);
        } else if (_M0L6_2acntS4035 == 1) {
          #line 1142 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aZSetS1628);
        }
        _M0L4_2azS1629 = _M0L8_2afieldS3651;
        _M0L1zS1624 = _M0L4_2azS1629;
        goto join_1623;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1627);
        goto join_1621;
        break;
      }
    }
  }
  goto joinlet_4137;
  join_1623:;
  _M0L4zsetS1620 = _M0L1zS1624;
  joinlet_4137:;
  goto joinlet_4136;
  join_1621:;
  _M0L7_2abindS1622 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3550 = _M0L7_2abindS1622;
  _M0L6_2atmpS3549
  = (struct _M0TPB9ArrayViewGUsfEE){
    .$0 = _M0L6_2atmpS3550, .$1 = 0, .$2 = 0
  };
  #line 1144 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4zsetS1620 = _M0MPB3Map3MapGsfE(_M0L6_2atmpS3549, 10ll);
  moonbit_decref(_M0L6_2atmpS3549.$0);
  joinlet_4136:;
  #line 1146 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7existedS1630
  = _M0MPB3Map8containsGsfE(_M0L4zsetS1620, _M0L11member__valS1631);
  #line 1147 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsfE(_M0L4zsetS1620, _M0L11member__valS1631, _M0L5scoreS1632);
  _M0L4dataS3547 = _M0L4selfS1618->$0;
  _M0L4ZSetS3548
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4ZSet));
  Moonbit_object_header(_M0L4ZSetS3548)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 9, 4);
  ((struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4ZSetS3548)->$0
  = _M0L4zsetS1620;
  moonbit_incref(_M0L4dataS3547);
  #line 1148 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3547, _M0L3keyS1619, _M0L4ZSetS3548);
  moonbit_decref(_M0L4dataS3547);
  moonbit_decref(_M0L4ZSetS3548);
  return !_M0L7existedS1630;
}

int32_t _M0MP19moonbitDB8Database9sismember(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1608,
  moonbit_string_t _M0L3keyS1609,
  moonbit_string_t _M0L5valueS1612
) {
  #line 963 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 964 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1608, _M0L3keyS1609)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1611;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3545 =
      _M0L4selfS1608->$0;
    void* _M0L7_2abindS1613;
    int32_t _result_4139;
    moonbit_incref(_M0L4dataS3545);
    #line 967 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1613
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3545, _M0L3keyS1609);
    moonbit_decref(_M0L4dataS3545);
    if (_M0L7_2abindS1613 == 0) {
      if (_M0L7_2abindS1613) {
        moonbit_decref(_M0L7_2abindS1613);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1614 = _M0L7_2abindS1613;
      void* _M0L4_2axS1615 = _M0L7_2aSomeS1614;
      switch (Moonbit_object_tag(_M0L4_2axS1615)) {
        case 3: {
          struct _M0DTP19moonbitDB10RedisValue3Set* _M0L6_2aSetS1616 =
            (struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L4_2axS1615;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3653 = _M0L6_2aSetS1616->$0;
          int32_t _M0L6_2acntS4037 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1616));
          struct _M0TPB3MapGsbE* _M0L4_2asS1617;
          if (_M0L6_2acntS4037 > 1) {
            int32_t _M0L11_2anew__cntS4038 = _M0L6_2acntS4037 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1616), _M0L11_2anew__cntS4038);
            moonbit_incref(_M0L8_2afieldS3653);
          } else if (_M0L6_2acntS4037 == 1) {
            #line 967 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2aSetS1616);
          }
          _M0L4_2asS1617 = _M0L8_2afieldS3653;
          _M0L1sS1611 = _M0L4_2asS1617;
          goto join_1610;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1615);
          return 0;
          break;
        }
      }
    }
    join_1610:;
    #line 968 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4139 = _M0MPB3Map8containsGsbE(_M0L1sS1611, _M0L5valueS1612);
    moonbit_decref(_M0L1sS1611);
    return _result_4139;
  }
}

int32_t _M0MP19moonbitDB8Database5scard(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1599,
  moonbit_string_t _M0L3keyS1600
) {
  #line 952 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 953 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1599, _M0L3keyS1600)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1602;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3544 =
      _M0L4selfS1599->$0;
    void* _M0L7_2abindS1603;
    int32_t _result_4141;
    moonbit_incref(_M0L4dataS3544);
    #line 956 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1603
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3544, _M0L3keyS1600);
    moonbit_decref(_M0L4dataS3544);
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
          struct _M0DTP19moonbitDB10RedisValue3Set* _M0L6_2aSetS1606 =
            (struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L4_2axS1605;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3655 = _M0L6_2aSetS1606->$0;
          int32_t _M0L6_2acntS4039 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1606));
          struct _M0TPB3MapGsbE* _M0L4_2asS1607;
          if (_M0L6_2acntS4039 > 1) {
            int32_t _M0L11_2anew__cntS4040 = _M0L6_2acntS4039 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1606), _M0L11_2anew__cntS4040);
            moonbit_incref(_M0L8_2afieldS3655);
          } else if (_M0L6_2acntS4039 == 1) {
            #line 956 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2aSetS1606);
          }
          _M0L4_2asS1607 = _M0L8_2afieldS3655;
          _M0L1sS1602 = _M0L4_2asS1607;
          goto join_1601;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1605);
          return 0;
          break;
        }
      }
    }
    join_1601:;
    #line 957 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4141 = _M0MPB3Map6lengthGsbE(_M0L1sS1602);
    moonbit_decref(_M0L1sS1602);
    return _result_4141;
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database8smembers(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1580,
  moonbit_string_t _M0L3keyS1581
) {
  #line 917 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 918 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1580, _M0L3keyS1581)
  ) {
    moonbit_string_t* _M0L6_2atmpS3540 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4142 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4142)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_4142->$0 = _M0L6_2atmpS3540;
    _block_4142->$1 = 0;
    return _block_4142;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1584;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3543 =
      _M0L4selfS1580->$0;
    void* _M0L7_2abindS1594;
    moonbit_string_t* _M0L6_2atmpS3542;
    struct _M0TPB5ArrayGsE* _M0L6resultS1585;
    struct _M0TPB4IterGUsbEE* _M0L5_2aitS1586;
    moonbit_string_t* _M0L6_2atmpS3541;
    struct _M0TPB5ArrayGsE* _block_4147;
    moonbit_incref(_M0L4dataS3543);
    #line 921 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1594
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3543, _M0L3keyS1581);
    moonbit_decref(_M0L4dataS3543);
    if (_M0L7_2abindS1594 == 0) {
      if (_M0L7_2abindS1594) {
        moonbit_decref(_M0L7_2abindS1594);
      }
      goto join_1582;
    } else {
      void* _M0L7_2aSomeS1595 = _M0L7_2abindS1594;
      void* _M0L4_2axS1596 = _M0L7_2aSomeS1595;
      switch (Moonbit_object_tag(_M0L4_2axS1596)) {
        case 3: {
          struct _M0DTP19moonbitDB10RedisValue3Set* _M0L6_2aSetS1597 =
            (struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L4_2axS1596;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3658 = _M0L6_2aSetS1597->$0;
          int32_t _M0L6_2acntS4043 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1597));
          struct _M0TPB3MapGsbE* _M0L4_2asS1598;
          if (_M0L6_2acntS4043 > 1) {
            int32_t _M0L11_2anew__cntS4044 = _M0L6_2acntS4043 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1597), _M0L11_2anew__cntS4044);
            moonbit_incref(_M0L8_2afieldS3658);
          } else if (_M0L6_2acntS4043 == 1) {
            #line 921 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2aSetS1597);
          }
          _M0L4_2asS1598 = _M0L8_2afieldS3658;
          _M0L1sS1584 = _M0L4_2asS1598;
          goto join_1583;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1596);
          goto join_1582;
          break;
        }
      }
    }
    join_1583:;
    _M0L6_2atmpS3542 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1585
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1585)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS1585->$0 = _M0L6_2atmpS3542;
    _M0L6resultS1585->$1 = 0;
    #line 923 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L5_2aitS1586 = _M0MPB3Map5iter2GsbE(_M0L1sS1584);
    moonbit_decref(_M0L1sS1584);
    while (1) {
      moonbit_string_t _M0L1mS1588;
      struct _M0TUsbE* _M0L7_2abindS1590;
      #line 924 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L7_2abindS1590 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1586);
      if (_M0L7_2abindS1590 == 0) {
        if (_M0L7_2abindS1590) {
          moonbit_decref(_M0L7_2abindS1590);
        }
        moonbit_decref(_M0L5_2aitS1586);
      } else {
        struct _M0TUsbE* _M0L7_2aSomeS1591 = _M0L7_2abindS1590;
        struct _M0TUsbE* _M0L4_2axS1592 = _M0L7_2aSomeS1591;
        moonbit_string_t _M0L8_2afieldS3657 = _M0L4_2axS1592->$0;
        int32_t _M0L6_2acntS4041 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1592));
        moonbit_string_t _M0L4_2amS1593;
        if (_M0L6_2acntS4041 > 1) {
          int32_t _M0L11_2anew__cntS4042 = _M0L6_2acntS4041 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1592), _M0L11_2anew__cntS4042);
          moonbit_incref(_M0L8_2afieldS3657);
        } else if (_M0L6_2acntS4041 == 1) {
          #line 924 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L4_2axS1592);
        }
        _M0L4_2amS1593 = _M0L8_2afieldS3657;
        _M0L1mS1588 = _M0L4_2amS1593;
        goto join_1587;
      }
      goto joinlet_4146;
      join_1587:;
      #line 925 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1585, _M0L1mS1588);
      moonbit_decref(_M0L1mS1588);
      continue;
      joinlet_4146:;
      break;
    }
    return _M0L6resultS1585;
    join_1582:;
    _M0L6_2atmpS3541 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_4147
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4147)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_4147->$0 = _M0L6_2atmpS3541;
    _block_4147->$1 = 0;
    return _block_4147;
  }
}

int32_t _M0MP19moonbitDB8Database4sadd(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1567,
  moonbit_string_t _M0L3keyS1568,
  moonbit_string_t _M0L5valueS1579
) {
  int32_t _M0L6_2atmpS3534;
  struct _M0TPB3MapGsbE* _M0L3setS1569;
  struct _M0TPB3MapGsbE* _M0L1sS1573;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3539;
  void* _M0L7_2abindS1574;
  struct _M0TUsbE** _M0L7_2abindS1571;
  struct _M0TUsbE** _M0L6_2atmpS3538;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3537;
  #line 902 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 903 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3534
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1567, _M0L3keyS1568);
  _M0L4dataS3539 = _M0L4selfS1567->$0;
  moonbit_incref(_M0L4dataS3539);
  #line 904 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1574
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3539, _M0L3keyS1568);
  moonbit_decref(_M0L4dataS3539);
  if (_M0L7_2abindS1574 == 0) {
    if (_M0L7_2abindS1574) {
      moonbit_decref(_M0L7_2abindS1574);
    }
    goto join_1570;
  } else {
    void* _M0L7_2aSomeS1575 = _M0L7_2abindS1574;
    void* _M0L4_2axS1576 = _M0L7_2aSomeS1575;
    switch (Moonbit_object_tag(_M0L4_2axS1576)) {
      case 3: {
        struct _M0DTP19moonbitDB10RedisValue3Set* _M0L6_2aSetS1577 =
          (struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L4_2axS1576;
        struct _M0TPB3MapGsbE* _M0L8_2afieldS3661 = _M0L6_2aSetS1577->$0;
        int32_t _M0L6_2acntS4045 =
          Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1577));
        struct _M0TPB3MapGsbE* _M0L4_2asS1578;
        if (_M0L6_2acntS4045 > 1) {
          int32_t _M0L11_2anew__cntS4046 = _M0L6_2acntS4045 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1577), _M0L11_2anew__cntS4046);
          moonbit_incref(_M0L8_2afieldS3661);
        } else if (_M0L6_2acntS4045 == 1) {
          #line 904 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L6_2aSetS1577);
        }
        _M0L4_2asS1578 = _M0L8_2afieldS3661;
        _M0L1sS1573 = _M0L4_2asS1578;
        goto join_1572;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1576);
        goto join_1570;
        break;
      }
    }
  }
  goto joinlet_4149;
  join_1572:;
  _M0L3setS1569 = _M0L1sS1573;
  joinlet_4149:;
  goto joinlet_4148;
  join_1570:;
  _M0L7_2abindS1571 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3538 = _M0L7_2abindS1571;
  _M0L6_2atmpS3537
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3538, .$1 = 0, .$2 = 0
  };
  #line 906 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L3setS1569 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3537, 10ll);
  moonbit_decref(_M0L6_2atmpS3537.$0);
  joinlet_4148:;
  #line 908 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (_M0MPB3Map8containsGsbE(_M0L3setS1569, _M0L5valueS1579)) {
    moonbit_decref(_M0L3setS1569);
    return 0;
  } else {
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3535;
    void* _M0L3SetS3536;
    #line 911 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsbE(_M0L3setS1569, _M0L5valueS1579, 1);
    _M0L4dataS3535 = _M0L4selfS1567->$0;
    _M0L3SetS3536
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue3Set));
    Moonbit_object_header(_M0L3SetS3536)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 12, 3);
    ((struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L3SetS3536)->$0
    = _M0L3setS1569;
    moonbit_incref(_M0L4dataS3535);
    #line 912 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3535, _M0L3keyS1568, _M0L3SetS3536);
    moonbit_decref(_M0L4dataS3535);
    moonbit_decref(_M0L3SetS3536);
    return 1;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database6lindex(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1556,
  moonbit_string_t _M0L3keyS1557,
  int32_t _M0L5indexS1561
) {
  #line 741 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 742 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1556, _M0L3keyS1557)
  ) {
    return 0;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L5dequeS1560;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3533 =
      _M0L4selfS1556->$0;
    void* _M0L7_2abindS1562;
    moonbit_incref(_M0L4dataS3533);
    #line 745 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1562
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3533, _M0L3keyS1557);
    moonbit_decref(_M0L4dataS3533);
    if (_M0L7_2abindS1562 == 0) {
      if (_M0L7_2abindS1562) {
        moonbit_decref(_M0L7_2abindS1562);
      }
      goto join_1558;
    } else {
      void* _M0L7_2aSomeS1563 = _M0L7_2abindS1562;
      void* _M0L4_2axS1564 = _M0L7_2aSomeS1563;
      switch (Moonbit_object_tag(_M0L4_2axS1564)) {
        case 2: {
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1565 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1564;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS3663 =
            _M0L7_2aListS1565->$0;
          int32_t _M0L6_2acntS4047 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1565));
          struct _M0TP19moonbitDB5Deque* _M0L8_2adequeS1566;
          if (_M0L6_2acntS4047 > 1) {
            int32_t _M0L11_2anew__cntS4048 = _M0L6_2acntS4047 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1565), _M0L11_2anew__cntS4048);
            moonbit_incref(_M0L8_2afieldS3663);
          } else if (_M0L6_2acntS4047 == 1) {
            #line 745 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1565);
          }
          _M0L8_2adequeS1566 = _M0L8_2afieldS3663;
          _M0L5dequeS1560 = _M0L8_2adequeS1566;
          goto join_1559;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1564);
          goto join_1558;
          break;
        }
      }
    }
    join_1559:;
    if (_M0L5indexS1561 < 0) {
      int32_t _M0L6_2atmpS3532;
      int32_t _M0L6_2atmpS3531;
      moonbit_string_t _result_4152;
      #line 748 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3532 = _M0MP19moonbitDB5Deque6length(_M0L5dequeS1560);
      _M0L6_2atmpS3531 = _M0L6_2atmpS3532 + _M0L5indexS1561;
      #line 748 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _result_4152
      = _M0MP19moonbitDB5Deque7get__at(_M0L5dequeS1560, _M0L6_2atmpS3531);
      moonbit_decref(_M0L5dequeS1560);
      return _result_4152;
    } else {
      moonbit_string_t _result_4153;
      #line 750 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _result_4153
      = _M0MP19moonbitDB5Deque7get__at(_M0L5dequeS1560, _M0L5indexS1561);
      moonbit_decref(_M0L5dequeS1560);
      return _result_4153;
    }
    join_1558:;
    return 0;
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database6lrange(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1537,
  moonbit_string_t _M0L3keyS1538,
  int32_t _M0L5startS1545,
  int32_t _M0L3endS1547
) {
  #line 720 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 721 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1537, _M0L3keyS1538)
  ) {
    moonbit_string_t* _M0L6_2atmpS3525 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4154 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4154)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_4154->$0 = _M0L6_2atmpS3525;
    _block_4154->$1 = 0;
    return _block_4154;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L5dequeS1541;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3530 =
      _M0L4selfS1537->$0;
    void* _M0L7_2abindS1551;
    struct _M0TPB5ArrayGsE* _M0L3arrS1542;
    int32_t _M0L3lenS1543;
    int32_t _M0L10start__idxS1544;
    int32_t _M0L8end__idxS1546;
    moonbit_string_t* _M0L6_2atmpS3529;
    struct _M0TPB5ArrayGsE* _M0L6resultS1548;
    int32_t _M0L1iS1549;
    moonbit_string_t* _M0L6_2atmpS3526;
    struct _M0TPB5ArrayGsE* _block_4159;
    moonbit_incref(_M0L4dataS3530);
    #line 724 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1551
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3530, _M0L3keyS1538);
    moonbit_decref(_M0L4dataS3530);
    if (_M0L7_2abindS1551 == 0) {
      if (_M0L7_2abindS1551) {
        moonbit_decref(_M0L7_2abindS1551);
      }
      goto join_1539;
    } else {
      void* _M0L7_2aSomeS1552 = _M0L7_2abindS1551;
      void* _M0L4_2axS1553 = _M0L7_2aSomeS1552;
      switch (Moonbit_object_tag(_M0L4_2axS1553)) {
        case 2: {
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1554 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1553;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS3665 =
            _M0L7_2aListS1554->$0;
          int32_t _M0L6_2acntS4049 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1554));
          struct _M0TP19moonbitDB5Deque* _M0L8_2adequeS1555;
          if (_M0L6_2acntS4049 > 1) {
            int32_t _M0L11_2anew__cntS4050 = _M0L6_2acntS4049 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1554), _M0L11_2anew__cntS4050);
            moonbit_incref(_M0L8_2afieldS3665);
          } else if (_M0L6_2acntS4049 == 1) {
            #line 724 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1554);
          }
          _M0L8_2adequeS1555 = _M0L8_2afieldS3665;
          _M0L5dequeS1541 = _M0L8_2adequeS1555;
          goto join_1540;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1553);
          goto join_1539;
          break;
        }
      }
    }
    join_1540:;
    #line 726 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3arrS1542 = _M0MP19moonbitDB5Deque9to__array(_M0L5dequeS1541);
    moonbit_decref(_M0L5dequeS1541);
    #line 727 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3lenS1543 = _M0MPC15array5Array6lengthGsE(_M0L3arrS1542);
    if (_M0L5startS1545 < 0) {
      _M0L10start__idxS1544 = _M0L3lenS1543 + _M0L5startS1545;
    } else {
      _M0L10start__idxS1544 = _M0L5startS1545;
    }
    if (_M0L3endS1547 < 0) {
      _M0L8end__idxS1546 = _M0L3lenS1543 + _M0L3endS1547;
    } else {
      _M0L8end__idxS1546 = _M0L3endS1547;
    }
    _M0L6_2atmpS3529 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1548
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1548)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS1548->$0 = _M0L6_2atmpS3529;
    _M0L6resultS1548->$1 = 0;
    _M0L1iS1549 = _M0L10start__idxS1544;
    while (1) {
      int32_t _if__result_4158;
      if (_M0L1iS1549 <= _M0L8end__idxS1546) {
        if (_M0L1iS1549 >= 0) {
          _if__result_4158 = _M0L1iS1549 < _M0L3lenS1543;
        } else {
          _if__result_4158 = 0;
        }
      } else {
        _if__result_4158 = 0;
      }
      if (_if__result_4158) {
        moonbit_string_t _M0L6_2atmpS3527;
        int32_t _M0L6_2atmpS3528;
        #line 732 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3527
        = _M0MPC15array5Array2atGsE(_M0L3arrS1542, _M0L1iS1549);
        #line 732 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1548, _M0L6_2atmpS3527);
        moonbit_decref(_M0L6_2atmpS3527);
        _M0L6_2atmpS3528 = _M0L1iS1549 + 1;
        _M0L1iS1549 = _M0L6_2atmpS3528;
        continue;
      } else {
        moonbit_decref(_M0L3arrS1542);
      }
      break;
    }
    return _M0L6resultS1548;
    join_1539:;
    _M0L6_2atmpS3526 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_4159
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4159)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _block_4159->$0 = _M0L6_2atmpS3526;
    _block_4159->$1 = 0;
    return _block_4159;
  }
}

int32_t _M0MP19moonbitDB8Database4llen(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1528,
  moonbit_string_t _M0L3keyS1529
) {
  #line 709 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 710 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1528, _M0L3keyS1529)
  ) {
    return 0;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L1dS1531;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3524 =
      _M0L4selfS1528->$0;
    void* _M0L7_2abindS1532;
    int32_t _result_4161;
    moonbit_incref(_M0L4dataS3524);
    #line 713 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1532
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3524, _M0L3keyS1529);
    moonbit_decref(_M0L4dataS3524);
    if (_M0L7_2abindS1532 == 0) {
      if (_M0L7_2abindS1532) {
        moonbit_decref(_M0L7_2abindS1532);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1533 = _M0L7_2abindS1532;
      void* _M0L4_2axS1534 = _M0L7_2aSomeS1533;
      switch (Moonbit_object_tag(_M0L4_2axS1534)) {
        case 2: {
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1535 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1534;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS3667 =
            _M0L7_2aListS1535->$0;
          int32_t _M0L6_2acntS4051 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1535));
          struct _M0TP19moonbitDB5Deque* _M0L4_2adS1536;
          if (_M0L6_2acntS4051 > 1) {
            int32_t _M0L11_2anew__cntS4052 = _M0L6_2acntS4051 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1535), _M0L11_2anew__cntS4052);
            moonbit_incref(_M0L8_2afieldS3667);
          } else if (_M0L6_2acntS4051 == 1) {
            #line 713 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1535);
          }
          _M0L4_2adS1536 = _M0L8_2afieldS3667;
          _M0L1dS1531 = _M0L4_2adS1536;
          goto join_1530;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1534);
          return 0;
          break;
        }
      }
    }
    join_1530:;
    #line 714 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4161 = _M0MP19moonbitDB5Deque6length(_M0L1dS1531);
    moonbit_decref(_M0L1dS1531);
    return _result_4161;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database4rpop(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1517,
  moonbit_string_t _M0L3keyS1518
) {
  #line 694 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 695 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1517, _M0L3keyS1518)
  ) {
    return 0;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L5dequeS1521;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3523 =
      _M0L4selfS1517->$0;
    void* _M0L7_2abindS1523;
    moonbit_string_t _M0L3valS1522;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3521;
    void* _M0L4ListS3522;
    moonbit_incref(_M0L4dataS3523);
    #line 698 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1523
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3523, _M0L3keyS1518);
    moonbit_decref(_M0L4dataS3523);
    if (_M0L7_2abindS1523 == 0) {
      if (_M0L7_2abindS1523) {
        moonbit_decref(_M0L7_2abindS1523);
      }
      goto join_1519;
    } else {
      void* _M0L7_2aSomeS1524 = _M0L7_2abindS1523;
      void* _M0L4_2axS1525 = _M0L7_2aSomeS1524;
      switch (Moonbit_object_tag(_M0L4_2axS1525)) {
        case 2: {
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1526 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1525;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS3670 =
            _M0L7_2aListS1526->$0;
          int32_t _M0L6_2acntS4053 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1526));
          struct _M0TP19moonbitDB5Deque* _M0L8_2adequeS1527;
          if (_M0L6_2acntS4053 > 1) {
            int32_t _M0L11_2anew__cntS4054 = _M0L6_2acntS4053 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1526), _M0L11_2anew__cntS4054);
            moonbit_incref(_M0L8_2afieldS3670);
          } else if (_M0L6_2acntS4053 == 1) {
            #line 698 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1526);
          }
          _M0L8_2adequeS1527 = _M0L8_2afieldS3670;
          _M0L5dequeS1521 = _M0L8_2adequeS1527;
          goto join_1520;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1525);
          goto join_1519;
          break;
        }
      }
    }
    join_1520:;
    #line 700 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3valS1522 = _M0MP19moonbitDB5Deque9pop__back(_M0L5dequeS1521);
    _M0L4dataS3521 = _M0L4selfS1517->$0;
    _M0L4ListS3522
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4List));
    Moonbit_object_header(_M0L4ListS3522)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
    ((struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4ListS3522)->$0
    = _M0L5dequeS1521;
    moonbit_incref(_M0L4dataS3521);
    #line 701 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3521, _M0L3keyS1518, _M0L4ListS3522);
    moonbit_decref(_M0L4dataS3521);
    moonbit_decref(_M0L4ListS3522);
    return _M0L3valS1522;
    join_1519:;
    return 0;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database4lpop(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1506,
  moonbit_string_t _M0L3keyS1507
) {
  #line 679 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 680 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1506, _M0L3keyS1507)
  ) {
    return 0;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L5dequeS1510;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3520 =
      _M0L4selfS1506->$0;
    void* _M0L7_2abindS1512;
    moonbit_string_t _M0L3valS1511;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3518;
    void* _M0L4ListS3519;
    moonbit_incref(_M0L4dataS3520);
    #line 683 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1512
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3520, _M0L3keyS1507);
    moonbit_decref(_M0L4dataS3520);
    if (_M0L7_2abindS1512 == 0) {
      if (_M0L7_2abindS1512) {
        moonbit_decref(_M0L7_2abindS1512);
      }
      goto join_1508;
    } else {
      void* _M0L7_2aSomeS1513 = _M0L7_2abindS1512;
      void* _M0L4_2axS1514 = _M0L7_2aSomeS1513;
      switch (Moonbit_object_tag(_M0L4_2axS1514)) {
        case 2: {
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1515 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1514;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS3673 =
            _M0L7_2aListS1515->$0;
          int32_t _M0L6_2acntS4055 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1515));
          struct _M0TP19moonbitDB5Deque* _M0L8_2adequeS1516;
          if (_M0L6_2acntS4055 > 1) {
            int32_t _M0L11_2anew__cntS4056 = _M0L6_2acntS4055 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1515), _M0L11_2anew__cntS4056);
            moonbit_incref(_M0L8_2afieldS3673);
          } else if (_M0L6_2acntS4055 == 1) {
            #line 683 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1515);
          }
          _M0L8_2adequeS1516 = _M0L8_2afieldS3673;
          _M0L5dequeS1510 = _M0L8_2adequeS1516;
          goto join_1509;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1514);
          goto join_1508;
          break;
        }
      }
    }
    join_1509:;
    #line 685 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3valS1511 = _M0MP19moonbitDB5Deque10pop__front(_M0L5dequeS1510);
    _M0L4dataS3518 = _M0L4selfS1506->$0;
    _M0L4ListS3519
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4List));
    Moonbit_object_header(_M0L4ListS3519)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
    ((struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4ListS3519)->$0
    = _M0L5dequeS1510;
    moonbit_incref(_M0L4dataS3518);
    #line 686 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3518, _M0L3keyS1507, _M0L4ListS3519);
    moonbit_decref(_M0L4dataS3518);
    moonbit_decref(_M0L4ListS3519);
    return _M0L3valS1511;
    join_1508:;
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database5rpush(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1494,
  moonbit_string_t _M0L3keyS1495,
  moonbit_string_t _M0L5valueS1505
) {
  int32_t _M0L6_2atmpS3514;
  struct _M0TP19moonbitDB5Deque* _M0L5dequeS1496;
  struct _M0TP19moonbitDB5Deque* _M0L1dS1499;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3517;
  void* _M0L7_2abindS1500;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3515;
  void* _M0L4ListS3516;
  int32_t _result_4168;
  #line 668 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 669 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3514
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1494, _M0L3keyS1495);
  _M0L4dataS3517 = _M0L4selfS1494->$0;
  moonbit_incref(_M0L4dataS3517);
  #line 670 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1500
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3517, _M0L3keyS1495);
  moonbit_decref(_M0L4dataS3517);
  if (_M0L7_2abindS1500 == 0) {
    if (_M0L7_2abindS1500) {
      moonbit_decref(_M0L7_2abindS1500);
    }
    goto join_1497;
  } else {
    void* _M0L7_2aSomeS1501 = _M0L7_2abindS1500;
    void* _M0L4_2axS1502 = _M0L7_2aSomeS1501;
    switch (Moonbit_object_tag(_M0L4_2axS1502)) {
      case 2: {
        struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1503 =
          (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1502;
        struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS3676 =
          _M0L7_2aListS1503->$0;
        int32_t _M0L6_2acntS4057 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1503));
        struct _M0TP19moonbitDB5Deque* _M0L4_2adS1504;
        if (_M0L6_2acntS4057 > 1) {
          int32_t _M0L11_2anew__cntS4058 = _M0L6_2acntS4057 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1503), _M0L11_2anew__cntS4058);
          moonbit_incref(_M0L8_2afieldS3676);
        } else if (_M0L6_2acntS4057 == 1) {
          #line 670 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aListS1503);
        }
        _M0L4_2adS1504 = _M0L8_2afieldS3676;
        _M0L1dS1499 = _M0L4_2adS1504;
        goto join_1498;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1502);
        goto join_1497;
        break;
      }
    }
  }
  goto joinlet_4167;
  join_1498:;
  _M0L5dequeS1496 = _M0L1dS1499;
  joinlet_4167:;
  goto joinlet_4166;
  join_1497:;
  #line 672 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5dequeS1496 = _M0MP19moonbitDB5Deque3new();
  joinlet_4166:;
  #line 674 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MP19moonbitDB5Deque10push__back(_M0L5dequeS1496, _M0L5valueS1505);
  _M0L4dataS3515 = _M0L4selfS1494->$0;
  moonbit_incref(_M0L5dequeS1496);
  _M0L4ListS3516
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4List));
  Moonbit_object_header(_M0L4ListS3516)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 15, 2);
  ((struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4ListS3516)->$0
  = _M0L5dequeS1496;
  moonbit_incref(_M0L4dataS3515);
  #line 675 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3515, _M0L3keyS1495, _M0L4ListS3516);
  moonbit_decref(_M0L4dataS3515);
  moonbit_decref(_M0L4ListS3516);
  #line 676 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_4168 = _M0MP19moonbitDB5Deque6length(_M0L5dequeS1496);
  moonbit_decref(_M0L5dequeS1496);
  return _result_4168;
}

int32_t _M0MP19moonbitDB8Database4hlen(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1485,
  moonbit_string_t _M0L3keyS1486
) {
  #line 646 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 647 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1485, _M0L3keyS1486)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1488;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3513 =
      _M0L4selfS1485->$0;
    void* _M0L7_2abindS1489;
    int32_t _result_4170;
    moonbit_incref(_M0L4dataS3513);
    #line 650 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1489
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3513, _M0L3keyS1486);
    moonbit_decref(_M0L4dataS3513);
    if (_M0L7_2abindS1489 == 0) {
      if (_M0L7_2abindS1489) {
        moonbit_decref(_M0L7_2abindS1489);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1490 = _M0L7_2abindS1489;
      void* _M0L4_2axS1491 = _M0L7_2aSomeS1490;
      switch (Moonbit_object_tag(_M0L4_2axS1491)) {
        case 1: {
          struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1492 =
            (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1491;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3678 = _M0L7_2aHashS1492->$0;
          int32_t _M0L6_2acntS4059 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1492));
          struct _M0TPB3MapGssE* _M0L4_2ahS1493;
          if (_M0L6_2acntS4059 > 1) {
            int32_t _M0L11_2anew__cntS4060 = _M0L6_2acntS4059 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1492), _M0L11_2anew__cntS4060);
            moonbit_incref(_M0L8_2afieldS3678);
          } else if (_M0L6_2acntS4059 == 1) {
            #line 650 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aHashS1492);
          }
          _M0L4_2ahS1493 = _M0L8_2afieldS3678;
          _M0L1hS1488 = _M0L4_2ahS1493;
          goto join_1487;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1491);
          return 0;
          break;
        }
      }
    }
    join_1487:;
    #line 651 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4170 = _M0MPB3Map6lengthGssE(_M0L1hS1488);
    moonbit_decref(_M0L1hS1488);
    return _result_4170;
  }
}

struct _M0TPB3MapGssE* _M0MP19moonbitDB8Database7hgetall(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1473,
  moonbit_string_t _M0L3keyS1474
) {
  #line 635 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 636 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1473, _M0L3keyS1474)
  ) {
    struct _M0TUssE** _M0L7_2abindS1475 =
      (struct _M0TUssE**)moonbit_empty_ref_array;
    struct _M0TUssE** _M0L6_2atmpS3509 = _M0L7_2abindS1475;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3508 =
      (struct _M0TPB9ArrayViewGUssEE){.$0 = _M0L6_2atmpS3509,
                                        .$1 = 0,
                                        .$2 = 0};
    struct _M0TPB3MapGssE* _result_4171;
    #line 637 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4171 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3508, 0ll);
    moonbit_decref(_M0L6_2atmpS3508.$0);
    return _result_4171;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1479;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3512 =
      _M0L4selfS1473->$0;
    void* _M0L7_2abindS1480;
    struct _M0TUssE** _M0L7_2abindS1477;
    struct _M0TUssE** _M0L6_2atmpS3511;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3510;
    struct _M0TPB3MapGssE* _result_4174;
    moonbit_incref(_M0L4dataS3512);
    #line 639 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1480
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3512, _M0L3keyS1474);
    moonbit_decref(_M0L4dataS3512);
    if (_M0L7_2abindS1480 == 0) {
      if (_M0L7_2abindS1480) {
        moonbit_decref(_M0L7_2abindS1480);
      }
      goto join_1476;
    } else {
      void* _M0L7_2aSomeS1481 = _M0L7_2abindS1480;
      void* _M0L4_2axS1482 = _M0L7_2aSomeS1481;
      switch (Moonbit_object_tag(_M0L4_2axS1482)) {
        case 1: {
          struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1483 =
            (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1482;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3680 = _M0L7_2aHashS1483->$0;
          int32_t _M0L6_2acntS4061 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1483));
          struct _M0TPB3MapGssE* _M0L4_2ahS1484;
          if (_M0L6_2acntS4061 > 1) {
            int32_t _M0L11_2anew__cntS4062 = _M0L6_2acntS4061 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1483), _M0L11_2anew__cntS4062);
            moonbit_incref(_M0L8_2afieldS3680);
          } else if (_M0L6_2acntS4061 == 1) {
            #line 639 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aHashS1483);
          }
          _M0L4_2ahS1484 = _M0L8_2afieldS3680;
          _M0L1hS1479 = _M0L4_2ahS1484;
          goto join_1478;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1482);
          goto join_1476;
          break;
        }
      }
    }
    join_1478:;
    return _M0L1hS1479;
    join_1476:;
    _M0L7_2abindS1477 = (struct _M0TUssE**)moonbit_empty_ref_array;
    _M0L6_2atmpS3511 = _M0L7_2abindS1477;
    _M0L6_2atmpS3510
    = (struct _M0TPB9ArrayViewGUssEE){
      .$0 = _M0L6_2atmpS3511, .$1 = 0, .$2 = 0
    };
    #line 641 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4174 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3510, 0ll);
    moonbit_decref(_M0L6_2atmpS3510.$0);
    return _result_4174;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database4hget(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1462,
  moonbit_string_t _M0L3keyS1463,
  moonbit_string_t _M0L5fieldS1467
) {
  #line 606 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 607 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1462, _M0L3keyS1463)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1466;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3507 =
      _M0L4selfS1462->$0;
    void* _M0L7_2abindS1468;
    moonbit_string_t _result_4177;
    moonbit_incref(_M0L4dataS3507);
    #line 610 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1468
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3507, _M0L3keyS1463);
    moonbit_decref(_M0L4dataS3507);
    if (_M0L7_2abindS1468 == 0) {
      if (_M0L7_2abindS1468) {
        moonbit_decref(_M0L7_2abindS1468);
      }
      goto join_1464;
    } else {
      void* _M0L7_2aSomeS1469 = _M0L7_2abindS1468;
      void* _M0L4_2axS1470 = _M0L7_2aSomeS1469;
      switch (Moonbit_object_tag(_M0L4_2axS1470)) {
        case 1: {
          struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1471 =
            (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1470;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3682 = _M0L7_2aHashS1471->$0;
          int32_t _M0L6_2acntS4063 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1471));
          struct _M0TPB3MapGssE* _M0L4_2ahS1472;
          if (_M0L6_2acntS4063 > 1) {
            int32_t _M0L11_2anew__cntS4064 = _M0L6_2acntS4063 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1471), _M0L11_2anew__cntS4064);
            moonbit_incref(_M0L8_2afieldS3682);
          } else if (_M0L6_2acntS4063 == 1) {
            #line 610 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aHashS1471);
          }
          _M0L4_2ahS1472 = _M0L8_2afieldS3682;
          _M0L1hS1466 = _M0L4_2ahS1472;
          goto join_1465;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1470);
          goto join_1464;
          break;
        }
      }
    }
    join_1465:;
    #line 611 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4177 = _M0MPB3Map3getGssE(_M0L1hS1466, _M0L5fieldS1467);
    moonbit_decref(_M0L1hS1466);
    return _result_4177;
    join_1464:;
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database4hset(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1448,
  moonbit_string_t _M0L3keyS1449,
  moonbit_string_t _M0L5fieldS1460,
  moonbit_string_t _M0L5valueS1461
) {
  int32_t _M0L6_2atmpS3501;
  struct _M0TPB3MapGssE* _M0L4hashS1450;
  struct _M0TPB3MapGssE* _M0L1hS1454;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3506;
  void* _M0L7_2abindS1455;
  struct _M0TUssE** _M0L7_2abindS1452;
  struct _M0TUssE** _M0L6_2atmpS3505;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3504;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3502;
  void* _M0L4HashS3503;
  #line 596 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 597 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3501
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1448, _M0L3keyS1449);
  _M0L4dataS3506 = _M0L4selfS1448->$0;
  moonbit_incref(_M0L4dataS3506);
  #line 598 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1455
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3506, _M0L3keyS1449);
  moonbit_decref(_M0L4dataS3506);
  if (_M0L7_2abindS1455 == 0) {
    if (_M0L7_2abindS1455) {
      moonbit_decref(_M0L7_2abindS1455);
    }
    goto join_1451;
  } else {
    void* _M0L7_2aSomeS1456 = _M0L7_2abindS1455;
    void* _M0L4_2axS1457 = _M0L7_2aSomeS1456;
    switch (Moonbit_object_tag(_M0L4_2axS1457)) {
      case 1: {
        struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1458 =
          (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1457;
        struct _M0TPB3MapGssE* _M0L8_2afieldS3685 = _M0L7_2aHashS1458->$0;
        int32_t _M0L6_2acntS4065 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1458));
        struct _M0TPB3MapGssE* _M0L4_2ahS1459;
        if (_M0L6_2acntS4065 > 1) {
          int32_t _M0L11_2anew__cntS4066 = _M0L6_2acntS4065 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1458), _M0L11_2anew__cntS4066);
          moonbit_incref(_M0L8_2afieldS3685);
        } else if (_M0L6_2acntS4065 == 1) {
          #line 598 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aHashS1458);
        }
        _M0L4_2ahS1459 = _M0L8_2afieldS3685;
        _M0L1hS1454 = _M0L4_2ahS1459;
        goto join_1453;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1457);
        goto join_1451;
        break;
      }
    }
  }
  goto joinlet_4179;
  join_1453:;
  _M0L4hashS1450 = _M0L1hS1454;
  joinlet_4179:;
  goto joinlet_4178;
  join_1451:;
  _M0L7_2abindS1452 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3505 = _M0L7_2abindS1452;
  _M0L6_2atmpS3504
  = (struct _M0TPB9ArrayViewGUssEE){
    .$0 = _M0L6_2atmpS3505, .$1 = 0, .$2 = 0
  };
  #line 600 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4hashS1450 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3504, 10ll);
  moonbit_decref(_M0L6_2atmpS3504.$0);
  joinlet_4178:;
  #line 602 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGssE(_M0L4hashS1450, _M0L5fieldS1460, _M0L5valueS1461);
  _M0L4dataS3502 = _M0L4selfS1448->$0;
  _M0L4HashS3503
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4Hash));
  Moonbit_object_header(_M0L4HashS3503)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 18, 1);
  ((struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4HashS3503)->$0
  = _M0L4hashS1450;
  moonbit_incref(_M0L4dataS3502);
  #line 603 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3502, _M0L3keyS1449, _M0L4HashS3503);
  moonbit_decref(_M0L4dataS3502);
  moonbit_decref(_M0L4HashS3503);
  return 0;
}

int64_t _M0MP19moonbitDB8Database4decr(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1432,
  moonbit_string_t _M0L3keyS1433
) {
  int32_t _M0L6_2atmpS3494;
  moonbit_string_t _M0L1sS1436;
  moonbit_string_t _M0L7currentS1434;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3500;
  void* _M0L7_2abindS1437;
  int32_t _M0L1nS1443;
  int64_t _M0L7_2abindS1445;
  int32_t _M0L6_2atmpS3499;
  moonbit_string_t _M0L8new__valS1444;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3495;
  void* _M0L6StringS3496;
  struct _M0TPB3MapGsiE* _M0L7expiresS3497;
  int32_t _M0L6_2atmpS3498;
  #line 579 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 580 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3494
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1432, _M0L3keyS1433);
  _M0L4dataS3500 = _M0L4selfS1432->$0;
  moonbit_incref(_M0L4dataS3500);
  #line 581 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1437
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3500, _M0L3keyS1433);
  moonbit_decref(_M0L4dataS3500);
  if (_M0L7_2abindS1437 == 0) {
    if (_M0L7_2abindS1437) {
      moonbit_decref(_M0L7_2abindS1437);
    }
    _M0L7currentS1434 = (moonbit_string_t)moonbit_string_literal_84.data;
  } else {
    void* _M0L7_2aSomeS1438 = _M0L7_2abindS1437;
    void* _M0L4_2axS1439 = _M0L7_2aSomeS1438;
    switch (Moonbit_object_tag(_M0L4_2axS1439)) {
      case 0: {
        struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1440 =
          (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1439;
        moonbit_string_t _M0L8_2afieldS3689 = _M0L9_2aStringS1440->$0;
        int32_t _M0L6_2acntS4067 =
          Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1440));
        moonbit_string_t _M0L4_2asS1441;
        if (_M0L6_2acntS4067 > 1) {
          int32_t _M0L11_2anew__cntS4068 = _M0L6_2acntS4067 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1440), _M0L11_2anew__cntS4068);
          moonbit_incref(_M0L8_2afieldS3689);
        } else if (_M0L6_2acntS4067 == 1) {
          #line 581 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L9_2aStringS1440);
        }
        _M0L4_2asS1441 = _M0L8_2afieldS3689;
        _M0L1sS1436 = _M0L4_2asS1441;
        goto join_1435;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1439);
        _M0L7currentS1434 = (moonbit_string_t)moonbit_string_literal_84.data;
        break;
      }
    }
  }
  goto joinlet_4180;
  join_1435:;
  _M0L7currentS1434 = _M0L1sS1436;
  joinlet_4180:;
  #line 585 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1445 = _M0FP19moonbitDB10parse__int(_M0L7currentS1434);
  moonbit_decref(_M0L7currentS1434);
  if (_M0L7_2abindS1445 == 4294967296ll) {
    return 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS1446 = _M0L7_2abindS1445;
    int32_t _M0L4_2anS1447 = (int32_t)_M0L7_2aSomeS1446;
    _M0L1nS1443 = _M0L4_2anS1447;
    goto join_1442;
  }
  join_1442:;
  _M0L6_2atmpS3499 = _M0L1nS1443 - 1;
  #line 587 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L8new__valS1444 = _M0FP19moonbitDB15int__to__string(_M0L6_2atmpS3499);
  _M0L4dataS3495 = _M0L4selfS1432->$0;
  _M0L6StringS3496
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3496)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
  ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS3496)->$0
  = _M0L8new__valS1444;
  moonbit_incref(_M0L4dataS3495);
  #line 588 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3495, _M0L3keyS1433, _M0L6StringS3496);
  moonbit_decref(_M0L4dataS3495);
  moonbit_decref(_M0L6StringS3496);
  _M0L7expiresS3497 = _M0L4selfS1432->$1;
  moonbit_incref(_M0L7expiresS3497);
  #line 589 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3497, _M0L3keyS1433);
  moonbit_decref(_M0L7expiresS3497);
  _M0L6_2atmpS3498 = _M0L1nS1443 - 1;
  return (int64_t)_M0L6_2atmpS3498;
}

int64_t _M0MP19moonbitDB8Database4incr(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1416,
  moonbit_string_t _M0L3keyS1417
) {
  int32_t _M0L6_2atmpS3487;
  moonbit_string_t _M0L1sS1420;
  moonbit_string_t _M0L7currentS1418;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3493;
  void* _M0L7_2abindS1421;
  int32_t _M0L1nS1427;
  int64_t _M0L7_2abindS1429;
  int32_t _M0L6_2atmpS3492;
  moonbit_string_t _M0L8new__valS1428;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3488;
  void* _M0L6StringS3489;
  struct _M0TPB3MapGsiE* _M0L7expiresS3490;
  int32_t _M0L6_2atmpS3491;
  #line 562 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 563 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3487
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1416, _M0L3keyS1417);
  _M0L4dataS3493 = _M0L4selfS1416->$0;
  moonbit_incref(_M0L4dataS3493);
  #line 564 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1421
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3493, _M0L3keyS1417);
  moonbit_decref(_M0L4dataS3493);
  if (_M0L7_2abindS1421 == 0) {
    if (_M0L7_2abindS1421) {
      moonbit_decref(_M0L7_2abindS1421);
    }
    _M0L7currentS1418 = (moonbit_string_t)moonbit_string_literal_84.data;
  } else {
    void* _M0L7_2aSomeS1422 = _M0L7_2abindS1421;
    void* _M0L4_2axS1423 = _M0L7_2aSomeS1422;
    switch (Moonbit_object_tag(_M0L4_2axS1423)) {
      case 0: {
        struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1424 =
          (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1423;
        moonbit_string_t _M0L8_2afieldS3693 = _M0L9_2aStringS1424->$0;
        int32_t _M0L6_2acntS4069 =
          Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1424));
        moonbit_string_t _M0L4_2asS1425;
        if (_M0L6_2acntS4069 > 1) {
          int32_t _M0L11_2anew__cntS4070 = _M0L6_2acntS4069 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1424), _M0L11_2anew__cntS4070);
          moonbit_incref(_M0L8_2afieldS3693);
        } else if (_M0L6_2acntS4069 == 1) {
          #line 564 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L9_2aStringS1424);
        }
        _M0L4_2asS1425 = _M0L8_2afieldS3693;
        _M0L1sS1420 = _M0L4_2asS1425;
        goto join_1419;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1423);
        _M0L7currentS1418 = (moonbit_string_t)moonbit_string_literal_84.data;
        break;
      }
    }
  }
  goto joinlet_4182;
  join_1419:;
  _M0L7currentS1418 = _M0L1sS1420;
  joinlet_4182:;
  #line 568 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1429 = _M0FP19moonbitDB10parse__int(_M0L7currentS1418);
  moonbit_decref(_M0L7currentS1418);
  if (_M0L7_2abindS1429 == 4294967296ll) {
    return 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS1430 = _M0L7_2abindS1429;
    int32_t _M0L4_2anS1431 = (int32_t)_M0L7_2aSomeS1430;
    _M0L1nS1427 = _M0L4_2anS1431;
    goto join_1426;
  }
  join_1426:;
  _M0L6_2atmpS3492 = _M0L1nS1427 + 1;
  #line 570 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L8new__valS1428 = _M0FP19moonbitDB15int__to__string(_M0L6_2atmpS3492);
  _M0L4dataS3488 = _M0L4selfS1416->$0;
  _M0L6StringS3489
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3489)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
  ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS3489)->$0
  = _M0L8new__valS1428;
  moonbit_incref(_M0L4dataS3488);
  #line 571 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3488, _M0L3keyS1417, _M0L6StringS3489);
  moonbit_decref(_M0L4dataS3488);
  moonbit_decref(_M0L6StringS3489);
  _M0L7expiresS3490 = _M0L4selfS1416->$1;
  moonbit_incref(_M0L7expiresS3490);
  #line 572 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3490, _M0L3keyS1417);
  moonbit_decref(_M0L7expiresS3490);
  _M0L6_2atmpS3491 = _M0L1nS1427 + 1;
  return (int64_t)_M0L6_2atmpS3491;
}

moonbit_string_t _M0FP19moonbitDB15int__to__string(int32_t _M0L1nS1407) {
  #line 526 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (_M0L1nS1407 == 0) {
    return (moonbit_string_t)moonbit_string_literal_84.data;
  } else {
    struct _M0TPB8MutLocalGiE* _M0L3numS1408 =
      (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    moonbit_string_t* _M0L6_2atmpS3486;
    struct _M0TPB5ArrayGsE* _M0L5charsS1409;
    int32_t _M0L3valS3472;
    moonbit_string_t* _M0L6_2atmpS3485;
    struct _M0TPB5ArrayGsE* _M0L6resultS1412;
    int32_t _M0L6_2atmpS3482;
    int32_t _M0L6_2atmpS3481;
    int32_t _M0L1iS1413;
    moonbit_string_t _M0L7_2abindS1415;
    int32_t _M0L6_2atmpS3484;
    struct _M0TPC16string10StringView _M0L6_2atmpS3483;
    moonbit_string_t _result_4186;
    Moonbit_object_header(_M0L3numS1408)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L3numS1408->$0 = _M0L1nS1407;
    _M0L6_2atmpS3486 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L5charsS1409
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L5charsS1409)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L5charsS1409->$0 = _M0L6_2atmpS3486;
    _M0L5charsS1409->$1 = 0;
    _M0L3valS3472 = _M0L3numS1408->$0;
    if (_M0L3valS3472 < 0) {
      int32_t _M0L3valS3474 = _M0L3numS1408->$0;
      int32_t _M0L6_2atmpS3473 = -_M0L3valS3474;
      _M0L3numS1408->$0 = _M0L6_2atmpS3473;
    }
    while (1) {
      int32_t _M0L3valS3475 = _M0L3numS1408->$0;
      if (_M0L3valS3475 > 0) {
        int32_t _M0L3valS3476 = _M0L3numS1408->$0;
        int32_t _M0L7_2abindS1410 = _M0L3valS3476 % 10;
        int32_t _M0L3valS3478;
        int32_t _M0L6_2atmpS3477;
        switch (_M0L7_2abindS1410) {
          case 0: {
            #line 537 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1409, (moonbit_string_t)moonbit_string_literal_84.data);
            break;
          }
          
          case 1: {
            #line 538 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1409, (moonbit_string_t)moonbit_string_literal_85.data);
            break;
          }
          
          case 2: {
            #line 539 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1409, (moonbit_string_t)moonbit_string_literal_86.data);
            break;
          }
          
          case 3: {
            #line 540 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1409, (moonbit_string_t)moonbit_string_literal_87.data);
            break;
          }
          
          case 4: {
            #line 541 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1409, (moonbit_string_t)moonbit_string_literal_88.data);
            break;
          }
          
          case 5: {
            #line 542 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1409, (moonbit_string_t)moonbit_string_literal_89.data);
            break;
          }
          
          case 6: {
            #line 543 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1409, (moonbit_string_t)moonbit_string_literal_90.data);
            break;
          }
          
          case 7: {
            #line 544 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1409, (moonbit_string_t)moonbit_string_literal_91.data);
            break;
          }
          
          case 8: {
            #line 545 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1409, (moonbit_string_t)moonbit_string_literal_92.data);
            break;
          }
          
          case 9: {
            #line 546 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1409, (moonbit_string_t)moonbit_string_literal_93.data);
            break;
          }
          default: {
            #line 547 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1409, (moonbit_string_t)moonbit_string_literal_84.data);
            break;
          }
        }
        _M0L3valS3478 = _M0L3numS1408->$0;
        _M0L6_2atmpS3477 = _M0L3valS3478 / 10;
        _M0L3numS1408->$0 = _M0L6_2atmpS3477;
        continue;
      } else {
        moonbit_decref(_M0L3numS1408);
      }
      break;
    }
    if (_M0L1nS1407 < 0) {
      #line 552 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L5charsS1409, (moonbit_string_t)moonbit_string_literal_94.data);
    }
    _M0L6_2atmpS3485 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1412
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1412)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
    _M0L6resultS1412->$0 = _M0L6_2atmpS3485;
    _M0L6resultS1412->$1 = 0;
    #line 555 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3482 = _M0MPC15array5Array6lengthGsE(_M0L5charsS1409);
    _M0L6_2atmpS3481 = _M0L6_2atmpS3482 - 1;
    _M0L1iS1413 = _M0L6_2atmpS3481;
    while (1) {
      if (_M0L1iS1413 >= 0) {
        moonbit_string_t _M0L6_2atmpS3479;
        int32_t _M0L6_2atmpS3480;
        #line 556 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3479
        = _M0MPC15array5Array2atGsE(_M0L5charsS1409, _M0L1iS1413);
        #line 556 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1412, _M0L6_2atmpS3479);
        moonbit_decref(_M0L6_2atmpS3479);
        _M0L6_2atmpS3480 = _M0L1iS1413 - 1;
        _M0L1iS1413 = _M0L6_2atmpS3480;
        continue;
      } else {
        moonbit_decref(_M0L5charsS1409);
      }
      break;
    }
    _M0L7_2abindS1415 = (moonbit_string_t)moonbit_string_literal_95.data;
    _M0L6_2atmpS3484 = Moonbit_array_length(_M0L7_2abindS1415);
    _M0L6_2atmpS3483
    = (struct _M0TPC16string10StringView){
      .$0 = _M0L7_2abindS1415, .$1 = 0, .$2 = _M0L6_2atmpS3484
    };
    #line 558 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4186
    = _M0MPC15array5Array4joinGsE(_M0L6resultS1412, _M0L6_2atmpS3483);
    moonbit_decref(_M0L6resultS1412);
    moonbit_decref(_M0L6_2atmpS3483.$0);
    return _result_4186;
  }
}

int64_t _M0FP19moonbitDB10parse__int(moonbit_string_t _M0L1sS1396) {
  int32_t _M0L6_2atmpS3459;
  #line 503 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3459 = Moonbit_array_length(_M0L1sS1396);
  if (_M0L6_2atmpS3459 == 0) {
    return 4294967296ll;
  } else {
    struct _M0TPB8MutLocalGiE* _M0L6resultS1397 =
      (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    struct _M0TPB8MutLocalGiE* _M0L4signS1398;
    struct _M0TPB8MutLocalGiE* _M0L5startS1399;
    int32_t _M0L6_2atmpS3460;
    int32_t _M0L3valS3468;
    int32_t _M0L1iS1400;
    int32_t _M0L3valS3470;
    int32_t _M0L3valS3471;
    int32_t _M0L6_2atmpS3469;
    Moonbit_object_header(_M0L6resultS1397)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L6resultS1397->$0 = 0;
    _M0L4signS1398
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L4signS1398)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L4signS1398->$0 = 1;
    _M0L5startS1399
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L5startS1399)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L5startS1399->$0 = 0;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1396)) {
      #line 510 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3460 = _M0L1sS1396[0];
    if (_M0L6_2atmpS3460 == 45) {
      _M0L4signS1398->$0 = -1;
      _M0L5startS1399->$0 = 1;
    } else {
      int32_t _M0L6_2atmpS3461;
      if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1396)) {
        #line 513 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3461 = _M0L1sS1396[0];
      if (_M0L6_2atmpS3461 == 43) {
        _M0L5startS1399->$0 = 1;
      }
    }
    _M0L3valS3468 = _M0L5startS1399->$0;
    moonbit_decref(_M0L5startS1399);
    _M0L1iS1400 = _M0L3valS3468;
    while (1) {
      int32_t _M0L6_2atmpS3462 = Moonbit_array_length(_M0L1sS1396);
      if (_M0L1iS1400 < _M0L6_2atmpS3462) {
        int32_t _M0L5digitS1402;
        int32_t _M0L6_2atmpS3466;
        int64_t _M0L7_2abindS1403;
        int32_t _M0L3valS3465;
        int32_t _M0L6_2atmpS3464;
        int32_t _M0L6_2atmpS3463;
        int32_t _M0L6_2atmpS3467;
        if (
          _M0L1iS1400 < 0 || _M0L1iS1400 >= Moonbit_array_length(_M0L1sS1396)
        ) {
          #line 517 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3466 = _M0L1sS1396[_M0L1iS1400];
        #line 517 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L7_2abindS1403
        = _M0FP19moonbitDB17uint16__to__digit(_M0L6_2atmpS3466);
        if (_M0L7_2abindS1403 == 4294967296ll) {
          moonbit_decref(_M0L4signS1398);
          moonbit_decref(_M0L6resultS1397);
          return 4294967296ll;
        } else {
          int64_t _M0L7_2aSomeS1404 = _M0L7_2abindS1403;
          int32_t _M0L8_2adigitS1405 = (int32_t)_M0L7_2aSomeS1404;
          _M0L5digitS1402 = _M0L8_2adigitS1405;
          goto join_1401;
        }
        goto joinlet_4188;
        join_1401:;
        _M0L3valS3465 = _M0L6resultS1397->$0;
        _M0L6_2atmpS3464 = _M0L3valS3465 * 10;
        _M0L6_2atmpS3463 = _M0L6_2atmpS3464 + _M0L5digitS1402;
        _M0L6resultS1397->$0 = _M0L6_2atmpS3463;
        joinlet_4188:;
        _M0L6_2atmpS3467 = _M0L1iS1400 + 1;
        _M0L1iS1400 = _M0L6_2atmpS3467;
        continue;
      }
      break;
    }
    _M0L3valS3470 = _M0L6resultS1397->$0;
    moonbit_decref(_M0L6resultS1397);
    _M0L3valS3471 = _M0L4signS1398->$0;
    moonbit_decref(_M0L4signS1398);
    _M0L6_2atmpS3469 = _M0L3valS3470 * _M0L3valS3471;
    return (int64_t)_M0L6_2atmpS3469;
  }
}

int64_t _M0FP19moonbitDB17uint16__to__digit(int32_t _M0L1cS1395) {
  #line 471 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  switch (_M0L1cS1395) {
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
  struct _M0TP19moonbitDB8Database* _M0L4selfS1386,
  moonbit_string_t _M0L3keyS1387
) {
  #line 460 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 461 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1386, _M0L3keyS1387)
  ) {
    return 0;
  } else {
    moonbit_string_t _M0L1sS1389;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3458 =
      _M0L4selfS1386->$0;
    void* _M0L7_2abindS1390;
    int32_t _result_4190;
    moonbit_incref(_M0L4dataS3458);
    #line 464 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1390
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3458, _M0L3keyS1387);
    moonbit_decref(_M0L4dataS3458);
    if (_M0L7_2abindS1390 == 0) {
      if (_M0L7_2abindS1390) {
        moonbit_decref(_M0L7_2abindS1390);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1391 = _M0L7_2abindS1390;
      void* _M0L4_2axS1392 = _M0L7_2aSomeS1391;
      switch (Moonbit_object_tag(_M0L4_2axS1392)) {
        case 0: {
          struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1393 =
            (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1392;
          moonbit_string_t _M0L8_2afieldS3695 = _M0L9_2aStringS1393->$0;
          int32_t _M0L6_2acntS4071 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1393));
          moonbit_string_t _M0L4_2asS1394;
          if (_M0L6_2acntS4071 > 1) {
            int32_t _M0L11_2anew__cntS4072 = _M0L6_2acntS4071 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1393), _M0L11_2anew__cntS4072);
            moonbit_incref(_M0L8_2afieldS3695);
          } else if (_M0L6_2acntS4071 == 1) {
            #line 464 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L9_2aStringS1393);
          }
          _M0L4_2asS1394 = _M0L8_2afieldS3695;
          _M0L1sS1389 = _M0L4_2asS1394;
          goto join_1388;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1392);
          return 0;
          break;
        }
      }
    }
    join_1388:;
    _result_4190 = Moonbit_array_length(_M0L1sS1389);
    moonbit_decref(_M0L1sS1389);
    return _result_4190;
  }
}

int32_t _M0MP19moonbitDB8Database6append(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1373,
  moonbit_string_t _M0L3keyS1374,
  moonbit_string_t _M0L5valueS1376
) {
  #line 444 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 445 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1373, _M0L3keyS1374)
  ) {
    moonbit_string_t _M0L8new__valS1375 = _M0L5valueS1376;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3453 =
      _M0L4selfS1373->$0;
    void* _M0L6StringS3454;
    moonbit_incref(_M0L8new__valS1375);
    _M0L6StringS3454
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
    Moonbit_object_header(_M0L6StringS3454)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
    ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS3454)->$0
    = _M0L8new__valS1375;
    moonbit_incref(_M0L4dataS3453);
    #line 447 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3453, _M0L3keyS1374, _M0L6StringS3454);
    moonbit_decref(_M0L4dataS3453);
    moonbit_decref(_M0L6StringS3454);
    return Moonbit_array_length(_M0L8new__valS1375);
  } else {
    moonbit_string_t _M0L1sS1379;
    moonbit_string_t _M0L7currentS1377;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3457 =
      _M0L4selfS1373->$0;
    void* _M0L7_2abindS1380;
    moonbit_string_t _M0L8new__valS1385;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3455;
    void* _M0L6StringS3456;
    int32_t _result_4192;
    moonbit_incref(_M0L4dataS3457);
    #line 450 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1380
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3457, _M0L3keyS1374);
    moonbit_decref(_M0L4dataS3457);
    if (_M0L7_2abindS1380 == 0) {
      if (_M0L7_2abindS1380) {
        moonbit_decref(_M0L7_2abindS1380);
      }
      _M0L7currentS1377 = (moonbit_string_t)moonbit_string_literal_95.data;
    } else {
      void* _M0L7_2aSomeS1381 = _M0L7_2abindS1380;
      void* _M0L4_2axS1382 = _M0L7_2aSomeS1381;
      switch (Moonbit_object_tag(_M0L4_2axS1382)) {
        case 0: {
          struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1383 =
            (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1382;
          moonbit_string_t _M0L8_2afieldS3699 = _M0L9_2aStringS1383->$0;
          int32_t _M0L6_2acntS4073 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1383));
          moonbit_string_t _M0L4_2asS1384;
          if (_M0L6_2acntS4073 > 1) {
            int32_t _M0L11_2anew__cntS4074 = _M0L6_2acntS4073 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1383), _M0L11_2anew__cntS4074);
            moonbit_incref(_M0L8_2afieldS3699);
          } else if (_M0L6_2acntS4073 == 1) {
            #line 450 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L9_2aStringS1383);
          }
          _M0L4_2asS1384 = _M0L8_2afieldS3699;
          _M0L1sS1379 = _M0L4_2asS1384;
          goto join_1378;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1382);
          _M0L7currentS1377
          = (moonbit_string_t)moonbit_string_literal_95.data;
          break;
        }
      }
    }
    goto joinlet_4191;
    join_1378:;
    _M0L7currentS1377 = _M0L1sS1379;
    joinlet_4191:;
    #line 454 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L8new__valS1385
    = moonbit_add_string(_M0L7currentS1377, _M0L5valueS1376);
    moonbit_decref(_M0L7currentS1377);
    _M0L4dataS3455 = _M0L4selfS1373->$0;
    moonbit_incref(_M0L8new__valS1385);
    _M0L6StringS3456
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
    Moonbit_object_header(_M0L6StringS3456)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
    ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS3456)->$0
    = _M0L8new__valS1385;
    moonbit_incref(_M0L4dataS3455);
    #line 455 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3455, _M0L3keyS1374, _M0L6StringS3456);
    moonbit_decref(_M0L4dataS3455);
    moonbit_decref(_M0L6StringS3456);
    _result_4192 = Moonbit_array_length(_M0L8new__valS1385);
    moonbit_decref(_M0L8new__valS1385);
    return _result_4192;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database8type__of(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1368,
  moonbit_string_t _M0L3keyS1369
) {
  #line 429 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 430 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1368, _M0L3keyS1369)
  ) {
    return (moonbit_string_t)moonbit_string_literal_96.data;
  } else {
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3452 =
      _M0L4selfS1368->$0;
    void* _M0L7_2abindS1370;
    moonbit_incref(_M0L4dataS3452);
    #line 433 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1370
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3452, _M0L3keyS1369);
    moonbit_decref(_M0L4dataS3452);
    if (_M0L7_2abindS1370 == 0) {
      if (_M0L7_2abindS1370) {
        moonbit_decref(_M0L7_2abindS1370);
      }
      return (moonbit_string_t)moonbit_string_literal_96.data;
    } else {
      void* _M0L7_2aSomeS1371 = _M0L7_2abindS1370;
      void* _M0L4_2axS1372 = _M0L7_2aSomeS1371;
      switch (Moonbit_object_tag(_M0L4_2axS1372)) {
        case 0: {
          moonbit_decref(_M0L4_2axS1372);
          return (moonbit_string_t)moonbit_string_literal_97.data;
          break;
        }
        
        case 1: {
          moonbit_decref(_M0L4_2axS1372);
          return (moonbit_string_t)moonbit_string_literal_98.data;
          break;
        }
        
        case 2: {
          moonbit_decref(_M0L4_2axS1372);
          return (moonbit_string_t)moonbit_string_literal_99.data;
          break;
        }
        
        case 3: {
          moonbit_decref(_M0L4_2axS1372);
          return (moonbit_string_t)moonbit_string_literal_100.data;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1372);
          return (moonbit_string_t)moonbit_string_literal_101.data;
          break;
        }
      }
    }
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database4keys(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1360
) {
  moonbit_string_t* _M0L6_2atmpS3451;
  struct _M0TPB5ArrayGsE* _M0L6resultS1358;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3450;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L5_2aitS1359;
  #line 377 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3451 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1358
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1358)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6resultS1358->$0 = _M0L6_2atmpS3451;
  _M0L6resultS1358->$1 = 0;
  _M0L4dataS3450 = _M0L4selfS1360->$0;
  moonbit_incref(_M0L4dataS3450);
  #line 378 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1359
  = _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(_M0L4dataS3450);
  moonbit_decref(_M0L4dataS3450);
  while (1) {
    moonbit_string_t _M0L3keyS1362;
    struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2abindS1364;
    int32_t _M0L6_2atmpS3449;
    #line 379 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1364
    = _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(_M0L5_2aitS1359);
    if (_M0L7_2abindS1364 == 0) {
      if (_M0L7_2abindS1364) {
        moonbit_decref(_M0L7_2abindS1364);
      }
      moonbit_decref(_M0L5_2aitS1359);
    } else {
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1365 =
        _M0L7_2abindS1364;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L4_2axS1366 =
        _M0L7_2aSomeS1365;
      moonbit_string_t _M0L8_2afieldS3702 = _M0L4_2axS1366->$0;
      int32_t _M0L6_2acntS4075 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1366));
      moonbit_string_t _M0L6_2akeyS1367;
      if (_M0L6_2acntS4075 > 1) {
        int32_t _M0L11_2anew__cntS4077 = _M0L6_2acntS4075 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1366), _M0L11_2anew__cntS4077);
        moonbit_incref(_M0L8_2afieldS3702);
      } else if (_M0L6_2acntS4075 == 1) {
        void* _M0L8_2afieldS4076 = _M0L4_2axS1366->$1;
        moonbit_decref(_M0L8_2afieldS4076);
        #line 379 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1366);
      }
      _M0L6_2akeyS1367 = _M0L8_2afieldS3702;
      _M0L3keyS1362 = _M0L6_2akeyS1367;
      goto join_1361;
    }
    goto joinlet_4194;
    join_1361:;
    #line 380 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3449
    = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1360, _M0L3keyS1362);
    if (!_M0L6_2atmpS3449) {
      #line 381 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1358, _M0L3keyS1362);
      moonbit_decref(_M0L3keyS1362);
    } else {
      moonbit_decref(_M0L3keyS1362);
    }
    continue;
    joinlet_4194:;
    break;
  }
  return _M0L6resultS1358;
}

int32_t _M0MP19moonbitDB8Database6exists(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1356,
  moonbit_string_t _M0L3keyS1357
) {
  #line 369 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 370 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1356, _M0L3keyS1357)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3448 =
      _M0L4selfS1356->$0;
    int32_t _result_4195;
    moonbit_incref(_M0L4dataS3448);
    #line 373 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4195
    = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS3448, _M0L3keyS1357);
    moonbit_decref(_M0L4dataS3448);
    return _result_4195;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database3get(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1346,
  moonbit_string_t _M0L3keyS1347
) {
  #line 345 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 346 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1346, _M0L3keyS1347)
  ) {
    return 0;
  } else {
    moonbit_string_t _M0L1sS1350;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3447 =
      _M0L4selfS1346->$0;
    void* _M0L7_2abindS1351;
    moonbit_incref(_M0L4dataS3447);
    #line 349 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1351
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3447, _M0L3keyS1347);
    moonbit_decref(_M0L4dataS3447);
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
          struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1354 =
            (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1353;
          moonbit_string_t _M0L8_2afieldS3705 = _M0L9_2aStringS1354->$0;
          int32_t _M0L6_2acntS4078 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1354));
          moonbit_string_t _M0L4_2asS1355;
          if (_M0L6_2acntS4078 > 1) {
            int32_t _M0L11_2anew__cntS4079 = _M0L6_2acntS4078 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1354), _M0L11_2anew__cntS4079);
            moonbit_incref(_M0L8_2afieldS3705);
          } else if (_M0L6_2acntS4078 == 1) {
            #line 349 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L9_2aStringS1354);
          }
          _M0L4_2asS1355 = _M0L8_2afieldS3705;
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

int32_t _M0MP19moonbitDB8Database4mdel(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1343,
  struct _M0TPB5ArrayGsE* _M0L4keysS1340
) {
  struct _M0TPB8MutLocalGiE* _M0L5countS1338;
  int32_t _M0L7_2abindS1339;
  int32_t _M0L2__S1341;
  int32_t _result_4199;
  #line 291 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5countS1338
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5countS1338)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5countS1338->$0 = 0;
  _M0L7_2abindS1339 = _M0L4keysS1340->$1;
  _M0L2__S1341 = 0;
  while (1) {
    if (_M0L2__S1341 < _M0L7_2abindS1339) {
      moonbit_string_t* _M0L3bufS3446 = _M0L4keysS1340->$0;
      moonbit_string_t _M0L3keyS1342 =
        (moonbit_string_t)_M0L3bufS3446[_M0L2__S1341];
      int32_t _M0L6_2atmpS3439;
      int32_t _M0L6_2atmpS3445;
      moonbit_incref(_M0L3keyS1342);
      #line 294 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3439
      = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1343, _M0L3keyS1342);
      if (!_M0L6_2atmpS3439) {
        struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3444 =
          _M0L4selfS1343->$0;
        int32_t _M0L7existedS1344;
        moonbit_incref(_M0L4dataS3444);
        #line 295 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L7existedS1344
        = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS3444, _M0L3keyS1342);
        moonbit_decref(_M0L4dataS3444);
        if (_M0L7existedS1344) {
          struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3440 =
            _M0L4selfS1343->$0;
          struct _M0TPB3MapGsiE* _M0L7expiresS3441;
          int32_t _M0L3valS3443;
          int32_t _M0L6_2atmpS3442;
          moonbit_incref(_M0L4dataS3440);
          #line 297 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(_M0L4dataS3440, _M0L3keyS1342);
          moonbit_decref(_M0L4dataS3440);
          _M0L7expiresS3441 = _M0L4selfS1343->$1;
          moonbit_incref(_M0L7expiresS3441);
          #line 298 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0MPB3Map6removeGsiE(_M0L7expiresS3441, _M0L3keyS1342);
          moonbit_decref(_M0L7expiresS3441);
          moonbit_decref(_M0L3keyS1342);
          _M0L3valS3443 = _M0L5countS1338->$0;
          _M0L6_2atmpS3442 = _M0L3valS3443 + 1;
          _M0L5countS1338->$0 = _M0L6_2atmpS3442;
        } else {
          moonbit_decref(_M0L3keyS1342);
        }
      } else {
        moonbit_decref(_M0L3keyS1342);
      }
      _M0L6_2atmpS3445 = _M0L2__S1341 + 1;
      _M0L2__S1341 = _M0L6_2atmpS3445;
      continue;
    }
    break;
  }
  _result_4199 = _M0L5countS1338->$0;
  moonbit_decref(_M0L5countS1338);
  return _result_4199;
}

struct _M0TPB5ArrayGOsE* _M0MP19moonbitDB8Database4mget(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1328,
  struct _M0TPB5ArrayGsE* _M0L4keysS1325
) {
  moonbit_string_t* _M0L6_2atmpS3438;
  struct _M0TPB5ArrayGOsE* _M0L6resultS1323;
  int32_t _M0L7_2abindS1324;
  int32_t _M0L2__S1326;
  #line 276 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3438 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1323
  = (struct _M0TPB5ArrayGOsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGOsE));
  Moonbit_object_header(_M0L6resultS1323)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 24, 0);
  _M0L6resultS1323->$0 = _M0L6_2atmpS3438;
  _M0L6resultS1323->$1 = 0;
  _M0L7_2abindS1324 = _M0L4keysS1325->$1;
  _M0L2__S1326 = 0;
  while (1) {
    if (_M0L2__S1326 < _M0L7_2abindS1324) {
      moonbit_string_t* _M0L3bufS3437 = _M0L4keysS1325->$0;
      moonbit_string_t _M0L3keyS1327 =
        (moonbit_string_t)_M0L3bufS3437[_M0L2__S1326];
      int32_t _M0L6_2atmpS3436;
      moonbit_incref(_M0L3keyS1327);
      #line 279 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      if (
        _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1328, _M0L3keyS1327)
      ) {
        moonbit_string_t _M0L6_2atmpS3432;
        moonbit_decref(_M0L3keyS1327);
        _M0L6_2atmpS3432 = 0;
        #line 280 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1323, _M0L6_2atmpS3432);
        if (_M0L6_2atmpS3432) {
          moonbit_decref(_M0L6_2atmpS3432);
        }
      } else {
        moonbit_string_t _M0L1sS1331;
        struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3435 =
          _M0L4selfS1328->$0;
        void* _M0L7_2abindS1332;
        moonbit_string_t _M0L6_2atmpS3434;
        moonbit_string_t _M0L6_2atmpS3433;
        moonbit_incref(_M0L4dataS3435);
        #line 282 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L7_2abindS1332
        = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3435, _M0L3keyS1327);
        moonbit_decref(_M0L4dataS3435);
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
              struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1335 =
                (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1334;
              moonbit_string_t _M0L8_2afieldS3712 = _M0L9_2aStringS1335->$0;
              int32_t _M0L6_2acntS4080 =
                Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1335));
              moonbit_string_t _M0L4_2asS1336;
              if (_M0L6_2acntS4080 > 1) {
                int32_t _M0L11_2anew__cntS4081 = _M0L6_2acntS4080 - 1;
                Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1335), _M0L11_2anew__cntS4081);
                moonbit_incref(_M0L8_2afieldS3712);
              } else if (_M0L6_2acntS4080 == 1) {
                #line 282 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
                moonbit_free(_M0L9_2aStringS1335);
              }
              _M0L4_2asS1336 = _M0L8_2afieldS3712;
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
        goto joinlet_4202;
        join_1330:;
        _M0L6_2atmpS3434 = _M0L1sS1331;
        #line 283 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1323, _M0L6_2atmpS3434);
        if (_M0L6_2atmpS3434) {
          moonbit_decref(_M0L6_2atmpS3434);
        }
        joinlet_4202:;
        goto joinlet_4201;
        join_1329:;
        _M0L6_2atmpS3433 = 0;
        #line 284 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1323, _M0L6_2atmpS3433);
        if (_M0L6_2atmpS3433) {
          moonbit_decref(_M0L6_2atmpS3433);
        }
        joinlet_4201:;
      }
      _M0L6_2atmpS3436 = _M0L2__S1326 + 1;
      _M0L2__S1326 = _M0L6_2atmpS3436;
      continue;
    }
    break;
  }
  return _M0L6resultS1323;
}

int32_t _M0MP19moonbitDB8Database4mset(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1321,
  struct _M0TPB5ArrayGsE* _M0L4keysS1319,
  struct _M0TPB5ArrayGsE* _M0L6valuesS1320
) {
  int32_t _M0L1iS1318;
  #line 269 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L1iS1318 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3424;
    int32_t _if__result_4204;
    #line 270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3424 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1319);
    if (_M0L1iS1318 < _M0L6_2atmpS3424) {
      int32_t _M0L6_2atmpS3423;
      #line 270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3423 = _M0MPC15array5Array6lengthGsE(_M0L6valuesS1320);
      _if__result_4204 = _M0L1iS1318 < _M0L6_2atmpS3423;
    } else {
      _if__result_4204 = 0;
    }
    if (_if__result_4204) {
      struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3425 =
        _M0L4selfS1321->$0;
      moonbit_string_t _M0L6_2atmpS3426;
      moonbit_string_t _M0L6_2atmpS3428;
      void* _M0L6StringS3427;
      struct _M0TPB3MapGsiE* _M0L7expiresS3429;
      moonbit_string_t _M0L6_2atmpS3430;
      int32_t _M0L6_2atmpS3431;
      moonbit_incref(_M0L4dataS3425);
      #line 271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3426
      = _M0MPC15array5Array2atGsE(_M0L4keysS1319, _M0L1iS1318);
      #line 271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3428
      = _M0MPC15array5Array2atGsE(_M0L6valuesS1320, _M0L1iS1318);
      _M0L6StringS3427
      = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
      Moonbit_object_header(_M0L6StringS3427)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
      ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS3427)->$0
      = _M0L6_2atmpS3428;
      #line 271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3425, _M0L6_2atmpS3426, _M0L6StringS3427);
      moonbit_decref(_M0L4dataS3425);
      moonbit_decref(_M0L6_2atmpS3426);
      moonbit_decref(_M0L6StringS3427);
      _M0L7expiresS3429 = _M0L4selfS1321->$1;
      moonbit_incref(_M0L7expiresS3429);
      #line 272 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3430
      = _M0MPC15array5Array2atGsE(_M0L4keysS1319, _M0L1iS1318);
      #line 272 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map6removeGsiE(_M0L7expiresS3429, _M0L6_2atmpS3430);
      moonbit_decref(_M0L7expiresS3429);
      moonbit_decref(_M0L6_2atmpS3430);
      _M0L6_2atmpS3431 = _M0L1iS1318 + 1;
      _M0L1iS1318 = _M0L6_2atmpS3431;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MP19moonbitDB8Database3ttl(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1315,
  moonbit_string_t _M0L3keyS1316
) {
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3415;
  int32_t _M0L6_2atmpS3414;
  #line 226 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4dataS3415 = _M0L4selfS1315->$0;
  moonbit_incref(_M0L4dataS3415);
  #line 227 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3414
  = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS3415, _M0L3keyS1316);
  moonbit_decref(_M0L4dataS3415);
  if (!_M0L6_2atmpS3414) {
    return -2;
  } else {
    struct _M0TPB3MapGsiE* _M0L7expiresS3416 = _M0L4selfS1315->$1;
    int32_t _result_4205;
    moonbit_incref(_M0L7expiresS3416);
    #line 229 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4205 = _M0MPB3Map8containsGsiE(_M0L7expiresS3416, _M0L3keyS1316);
    moonbit_decref(_M0L7expiresS3416);
    if (_result_4205) {
      struct _M0TPB3MapGsiE* _M0L7expiresS3422 = _M0L4selfS1315->$1;
      int64_t _M0L6_2atmpS3421;
      int32_t _M0L6_2atmpS3419;
      int32_t _M0L13current__timeS3420;
      int32_t _M0L9remainingS1317;
      moonbit_incref(_M0L7expiresS3422);
      #line 230 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3421 = _M0MPB3Map3getGsiE(_M0L7expiresS3422, _M0L3keyS1316);
      moonbit_decref(_M0L7expiresS3422);
      #line 230 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3419 = _M0MPC16option6Option6unwrapGiE(_M0L6_2atmpS3421);
      _M0L13current__timeS3420 = _M0L4selfS1315->$2;
      _M0L9remainingS1317 = _M0L6_2atmpS3419 - _M0L13current__timeS3420;
      if (_M0L9remainingS1317 <= 0) {
        struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3417 =
          _M0L4selfS1315->$0;
        struct _M0TPB3MapGsiE* _M0L7expiresS3418;
        moonbit_incref(_M0L4dataS3417);
        #line 232 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(_M0L4dataS3417, _M0L3keyS1316);
        moonbit_decref(_M0L4dataS3417);
        _M0L7expiresS3418 = _M0L4selfS1315->$1;
        moonbit_incref(_M0L7expiresS3418);
        #line 233 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPB3Map6removeGsiE(_M0L7expiresS3418, _M0L3keyS1316);
        moonbit_decref(_M0L7expiresS3418);
        return -2;
      } else {
        return _M0L9remainingS1317 / 1000;
      }
    } else {
      return -1;
    }
  }
}

int32_t _M0MP19moonbitDB8Database6expire(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1312,
  moonbit_string_t _M0L3keyS1313,
  int32_t _M0L7secondsS1314
) {
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3409;
  int32_t _result_4206;
  #line 208 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4dataS3409 = _M0L4selfS1312->$0;
  moonbit_incref(_M0L4dataS3409);
  #line 209 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_4206
  = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS3409, _M0L3keyS1313);
  moonbit_decref(_M0L4dataS3409);
  if (_result_4206) {
    struct _M0TPB3MapGsiE* _M0L7expiresS3410 = _M0L4selfS1312->$1;
    int32_t _M0L13current__timeS3412 = _M0L4selfS1312->$2;
    int32_t _M0L6_2atmpS3413 = _M0L7secondsS1314 * 1000;
    int32_t _M0L6_2atmpS3411 = _M0L13current__timeS3412 + _M0L6_2atmpS3413;
    moonbit_incref(_M0L7expiresS3410);
    #line 210 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsiE(_M0L7expiresS3410, _M0L3keyS1313, _M0L6_2atmpS3411);
    moonbit_decref(_M0L7expiresS3410);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database3set(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1309,
  moonbit_string_t _M0L3keyS1310,
  moonbit_string_t _M0L5valueS1311
) {
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3406;
  void* _M0L6StringS3407;
  struct _M0TPB3MapGsiE* _M0L7expiresS3408;
  #line 203 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4dataS3406 = _M0L4selfS1309->$0;
  moonbit_incref(_M0L5valueS1311);
  _M0L6StringS3407
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3407)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 21, 0);
  ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS3407)->$0
  = _M0L5valueS1311;
  moonbit_incref(_M0L4dataS3406);
  #line 204 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3406, _M0L3keyS1310, _M0L6StringS3407);
  moonbit_decref(_M0L4dataS3406);
  moonbit_decref(_M0L6StringS3407);
  _M0L7expiresS3408 = _M0L4selfS1309->$1;
  moonbit_incref(_M0L7expiresS3408);
  #line 205 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3408, _M0L3keyS1310);
  moonbit_decref(_M0L7expiresS3408);
  return 0;
}

int32_t _M0MP19moonbitDB8Database14check__expired(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1304,
  moonbit_string_t _M0L3keyS1305
) {
  int32_t _M0L12expire__timeS1303;
  struct _M0TPB3MapGsiE* _M0L7expiresS3405;
  int64_t _M0L7_2abindS1306;
  int32_t _M0L13current__timeS3402;
  #line 188 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7expiresS3405 = _M0L4selfS1304->$1;
  moonbit_incref(_M0L7expiresS3405);
  #line 189 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1306 = _M0MPB3Map3getGsiE(_M0L7expiresS3405, _M0L3keyS1305);
  moonbit_decref(_M0L7expiresS3405);
  if (_M0L7_2abindS1306 == 4294967296ll) {
    return 0;
  } else {
    int64_t _M0L7_2aSomeS1307 = _M0L7_2abindS1306;
    int32_t _M0L15_2aexpire__timeS1308 = (int32_t)_M0L7_2aSomeS1307;
    _M0L12expire__timeS1303 = _M0L15_2aexpire__timeS1308;
    goto join_1302;
  }
  join_1302:;
  _M0L13current__timeS3402 = _M0L4selfS1304->$2;
  if (_M0L12expire__timeS1303 <= _M0L13current__timeS3402) {
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3403 =
      _M0L4selfS1304->$0;
    struct _M0TPB3MapGsiE* _M0L7expiresS3404;
    moonbit_incref(_M0L4dataS3403);
    #line 192 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(_M0L4dataS3403, _M0L3keyS1305);
    moonbit_decref(_M0L4dataS3403);
    _M0L7expiresS3404 = _M0L4selfS1304->$1;
    moonbit_incref(_M0L7expiresS3404);
    #line 193 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map6removeGsiE(_M0L7expiresS3404, _M0L3keyS1305);
    moonbit_decref(_M0L7expiresS3404);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database13advance__time(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1300,
  int32_t _M0L2msS1301
) {
  int32_t _M0L13current__timeS3401;
  int32_t _M0L6_2atmpS3400;
  #line 184 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L13current__timeS3401 = _M0L4selfS1300->$2;
  _M0L6_2atmpS3400 = _M0L13current__timeS3401 + _M0L2msS1301;
  _M0L4selfS1300->$2 = _M0L6_2atmpS3400;
  return 0;
}

struct _M0TP19moonbitDB8Database* _M0MP19moonbitDB8Database3new() {
  struct _M0TUsRP19moonbitDB10RedisValueE** _M0L7_2abindS1298;
  struct _M0TUsRP19moonbitDB10RedisValueE** _M0L6_2atmpS3399;
  struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE _M0L6_2atmpS3398;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3394;
  struct _M0TUsiE** _M0L7_2abindS1299;
  struct _M0TUsiE** _M0L6_2atmpS3397;
  struct _M0TPB9ArrayViewGUsiEE _M0L6_2atmpS3396;
  struct _M0TPB3MapGsiE* _M0L6_2atmpS3395;
  struct _M0TP19moonbitDB8Database* _block_4208;
  #line 176 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1298
  = (struct _M0TUsRP19moonbitDB10RedisValueE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3399 = _M0L7_2abindS1298;
  _M0L6_2atmpS3398
  = (struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE){
    .$0 = _M0L6_2atmpS3399, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3394
  = _M0MPB3Map3MapGsRP19moonbitDB10RedisValueE(_M0L6_2atmpS3398, 1000ll);
  moonbit_decref(_M0L6_2atmpS3398.$0);
  _M0L7_2abindS1299 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3397 = _M0L7_2abindS1299;
  _M0L6_2atmpS3396
  = (struct _M0TPB9ArrayViewGUsiEE){
    .$0 = _M0L6_2atmpS3397, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3395 = _M0MPB3Map3MapGsiE(_M0L6_2atmpS3396, 1000ll);
  moonbit_decref(_M0L6_2atmpS3396.$0);
  _block_4208
  = (struct _M0TP19moonbitDB8Database*)moonbit_malloc(sizeof(struct _M0TP19moonbitDB8Database));
  Moonbit_object_header(_block_4208)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 27, 0);
  _block_4208->$0 = _M0L6_2atmpS3394;
  _block_4208->$1 = _M0L6_2atmpS3395;
  _block_4208->$2 = 0;
  return _block_4208;
}

moonbit_string_t _M0MP19moonbitDB5Deque7get__at(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1295,
  int32_t _M0L5indexS1296
) {
  int32_t _M0L3lenS1294;
  int32_t _if__result_4209;
  #line 59 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 60 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L3lenS1294 = _M0MP19moonbitDB5Deque6length(_M0L4selfS1295);
  if (_M0L5indexS1296 < 0) {
    _if__result_4209 = 1;
  } else {
    _if__result_4209 = _M0L5indexS1296 >= _M0L3lenS1294;
  }
  if (_if__result_4209) {
    return 0;
  } else {
    struct _M0TPB5ArrayGsE* _M0L5frontS3393 = _M0L4selfS1295->$0;
    int32_t _M0L10front__lenS1297;
    moonbit_incref(_M0L5frontS3393);
    #line 64 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L10front__lenS1297 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3393);
    moonbit_decref(_M0L5frontS3393);
    if (_M0L5indexS1296 < _M0L10front__lenS1297) {
      struct _M0TPB5ArrayGsE* _M0L5frontS3387 = _M0L4selfS1295->$0;
      int32_t _M0L6_2atmpS3389 = _M0L10front__lenS1297 - 1;
      int32_t _M0L6_2atmpS3388 = _M0L6_2atmpS3389 - _M0L5indexS1296;
      moonbit_string_t _M0L6_2atmpS3386;
      moonbit_incref(_M0L5frontS3387);
      #line 66 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3386
      = _M0MPC15array5Array2atGsE(_M0L5frontS3387, _M0L6_2atmpS3388);
      moonbit_decref(_M0L5frontS3387);
      return _M0L6_2atmpS3386;
    } else {
      struct _M0TPB5ArrayGsE* _M0L4backS3391 = _M0L4selfS1295->$1;
      int32_t _M0L6_2atmpS3392 = _M0L5indexS1296 - _M0L10front__lenS1297;
      moonbit_string_t _M0L6_2atmpS3390;
      moonbit_incref(_M0L4backS3391);
      #line 68 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3390
      = _M0MPC15array5Array2atGsE(_M0L4backS3391, _M0L6_2atmpS3392);
      moonbit_decref(_M0L4backS3391);
      return _M0L6_2atmpS3390;
    }
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB5Deque9to__array(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1287
) {
  moonbit_string_t* _M0L6_2atmpS3385;
  struct _M0TPB5ArrayGsE* _M0L6resultS1285;
  struct _M0TPB5ArrayGsE* _M0L5frontS3382;
  int32_t _M0L6_2atmpS3381;
  int32_t _M0L6_2atmpS3380;
  int32_t _M0L1iS1286;
  struct _M0TPB5ArrayGsE* _M0L7_2abindS1289;
  int32_t _M0L7_2abindS1290;
  int32_t _M0L2__S1291;
  #line 48 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3385 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1285
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1285)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6resultS1285->$0 = _M0L6_2atmpS3385;
  _M0L6resultS1285->$1 = 0;
  _M0L5frontS3382 = _M0L4selfS1287->$0;
  moonbit_incref(_M0L5frontS3382);
  #line 50 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3381 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3382);
  moonbit_decref(_M0L5frontS3382);
  _M0L6_2atmpS3380 = _M0L6_2atmpS3381 - 1;
  _M0L1iS1286 = _M0L6_2atmpS3380;
  while (1) {
    if (_M0L1iS1286 >= 0) {
      struct _M0TPB5ArrayGsE* _M0L5frontS3378 = _M0L4selfS1287->$0;
      moonbit_string_t _M0L6_2atmpS3377;
      int32_t _M0L6_2atmpS3379;
      moonbit_incref(_M0L5frontS3378);
      #line 51 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3377
      = _M0MPC15array5Array2atGsE(_M0L5frontS3378, _M0L1iS1286);
      moonbit_decref(_M0L5frontS3378);
      #line 51 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1285, _M0L6_2atmpS3377);
      moonbit_decref(_M0L6_2atmpS3377);
      _M0L6_2atmpS3379 = _M0L1iS1286 - 1;
      _M0L1iS1286 = _M0L6_2atmpS3379;
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
      moonbit_string_t* _M0L3bufS3384 = _M0L7_2abindS1289->$0;
      moonbit_string_t _M0L4itemS1292 =
        (moonbit_string_t)_M0L3bufS3384[_M0L2__S1291];
      int32_t _M0L6_2atmpS3383;
      moonbit_incref(_M0L4itemS1292);
      #line 54 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1285, _M0L4itemS1292);
      moonbit_decref(_M0L4itemS1292);
      _M0L6_2atmpS3383 = _M0L2__S1291 + 1;
      _M0L2__S1291 = _M0L6_2atmpS3383;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1289);
    }
    break;
  }
  return _M0L6resultS1285;
}

int32_t _M0MP19moonbitDB5Deque6length(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1284
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3376;
  int32_t _M0L6_2atmpS3373;
  struct _M0TPB5ArrayGsE* _M0L4backS3375;
  int32_t _M0L6_2atmpS3374;
  #line 44 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5frontS3376 = _M0L4selfS1284->$0;
  moonbit_incref(_M0L5frontS3376);
  #line 45 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3373 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3376);
  moonbit_decref(_M0L5frontS3376);
  _M0L4backS3375 = _M0L4selfS1284->$1;
  moonbit_incref(_M0L4backS3375);
  #line 45 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3374 = _M0MPC15array5Array6lengthGsE(_M0L4backS3375);
  moonbit_decref(_M0L4backS3375);
  return _M0L6_2atmpS3373 + _M0L6_2atmpS3374;
}

moonbit_string_t _M0MP19moonbitDB5Deque9pop__back(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1277
) {
  struct _M0TPB5ArrayGsE* _M0L4backS3367;
  int32_t _M0L6_2atmpS3366;
  struct _M0TPB5ArrayGsE* _M0L4backS3372;
  moonbit_string_t _result_4214;
  #line 31 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4backS3367 = _M0L4selfS1277->$1;
  moonbit_incref(_M0L4backS3367);
  #line 32 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3366 = _M0MPC15array5Array6lengthGsE(_M0L4backS3367);
  moonbit_decref(_M0L4backS3367);
  if (_M0L6_2atmpS3366 == 0) {
    while (1) {
      struct _M0TPB5ArrayGsE* _M0L5frontS3369 = _M0L4selfS1277->$0;
      int32_t _M0L6_2atmpS3368;
      moonbit_incref(_M0L5frontS3369);
      #line 33 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3368 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3369);
      moonbit_decref(_M0L5frontS3369);
      if (_M0L6_2atmpS3368 > 0) {
        struct _M0TPB5ArrayGsE* _M0L5frontS3371 = _M0L4selfS1277->$0;
        moonbit_string_t _M0L4itemS1278;
        moonbit_string_t _M0L1vS1280;
        struct _M0TPB5ArrayGsE* _M0L4backS3370;
        moonbit_incref(_M0L5frontS3371);
        #line 34 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L4itemS1278 = _M0MPC15array5Array3popGsE(_M0L5frontS3371);
        moonbit_decref(_M0L5frontS3371);
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
        goto joinlet_4213;
        join_1279:;
        _M0L4backS3370 = _M0L4selfS1277->$1;
        moonbit_incref(_M0L4backS3370);
        #line 36 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L4backS3370, _M0L1vS1280);
        moonbit_decref(_M0L4backS3370);
        moonbit_decref(_M0L1vS1280);
        joinlet_4213:;
        continue;
      }
      break;
    }
  }
  _M0L4backS3372 = _M0L4selfS1277->$1;
  moonbit_incref(_M0L4backS3372);
  #line 41 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_4214 = _M0MPC15array5Array3popGsE(_M0L4backS3372);
  moonbit_decref(_M0L4backS3372);
  return _result_4214;
}

moonbit_string_t _M0MP19moonbitDB5Deque10pop__front(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1270
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3360;
  int32_t _M0L6_2atmpS3359;
  struct _M0TPB5ArrayGsE* _M0L5frontS3365;
  moonbit_string_t _result_4217;
  #line 18 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5frontS3360 = _M0L4selfS1270->$0;
  moonbit_incref(_M0L5frontS3360);
  #line 19 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3359 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3360);
  moonbit_decref(_M0L5frontS3360);
  if (_M0L6_2atmpS3359 == 0) {
    while (1) {
      struct _M0TPB5ArrayGsE* _M0L4backS3362 = _M0L4selfS1270->$1;
      int32_t _M0L6_2atmpS3361;
      moonbit_incref(_M0L4backS3362);
      #line 20 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3361 = _M0MPC15array5Array6lengthGsE(_M0L4backS3362);
      moonbit_decref(_M0L4backS3362);
      if (_M0L6_2atmpS3361 > 0) {
        struct _M0TPB5ArrayGsE* _M0L4backS3364 = _M0L4selfS1270->$1;
        moonbit_string_t _M0L4itemS1271;
        moonbit_string_t _M0L1vS1273;
        struct _M0TPB5ArrayGsE* _M0L5frontS3363;
        moonbit_incref(_M0L4backS3364);
        #line 21 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L4itemS1271 = _M0MPC15array5Array3popGsE(_M0L4backS3364);
        moonbit_decref(_M0L4backS3364);
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
        goto joinlet_4216;
        join_1272:;
        _M0L5frontS3363 = _M0L4selfS1270->$0;
        moonbit_incref(_M0L5frontS3363);
        #line 23 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L5frontS3363, _M0L1vS1273);
        moonbit_decref(_M0L5frontS3363);
        moonbit_decref(_M0L1vS1273);
        joinlet_4216:;
        continue;
      }
      break;
    }
  }
  _M0L5frontS3365 = _M0L4selfS1270->$0;
  moonbit_incref(_M0L5frontS3365);
  #line 28 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_4217 = _M0MPC15array5Array3popGsE(_M0L5frontS3365);
  moonbit_decref(_M0L5frontS3365);
  return _result_4217;
}

int32_t _M0MP19moonbitDB5Deque10push__back(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1268,
  moonbit_string_t _M0L5valueS1269
) {
  struct _M0TPB5ArrayGsE* _M0L4backS3358;
  #line 14 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4backS3358 = _M0L4selfS1268->$1;
  moonbit_incref(_M0L4backS3358);
  #line 15 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPC15array5Array4pushGsE(_M0L4backS3358, _M0L5valueS1269);
  moonbit_decref(_M0L4backS3358);
  return 0;
}

struct _M0TP19moonbitDB5Deque* _M0MP19moonbitDB5Deque3new() {
  moonbit_string_t* _M0L6_2atmpS3357;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3354;
  moonbit_string_t* _M0L6_2atmpS3356;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3355;
  struct _M0TP19moonbitDB5Deque* _block_4218;
  #line 6 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3357 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3354
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3354)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6_2atmpS3354->$0 = _M0L6_2atmpS3357;
  _M0L6_2atmpS3354->$1 = 0;
  _M0L6_2atmpS3356 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3355
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3355)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6_2atmpS3355->$0 = _M0L6_2atmpS3356;
  _M0L6_2atmpS3355->$1 = 0;
  _block_4218
  = (struct _M0TP19moonbitDB5Deque*)moonbit_malloc(sizeof(struct _M0TP19moonbitDB5Deque));
  Moonbit_object_header(_block_4218)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 31, 0);
  _block_4218->$0 = _M0L6_2atmpS3354;
  _block_4218->$1 = _M0L6_2atmpS3355;
  return _block_4218;
}

moonbit_string_t _M0IPC15float5FloatPB4Show10to__string(float _M0L4selfS1267) {
  double _M0L6_2atmpS3353;
  #line 16 "/home/developer/.moon/lib/core/float/methods.mbt"
  _M0L6_2atmpS3353 = (double)_M0L4selfS1267;
  #line 17 "/home/developer/.moon/lib/core/float/methods.mbt"
  return _M0MPC16double6Double10to__string(_M0L6_2atmpS3353);
}

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1265,
  struct _M0TPC16string10StringView _M0L9separatorS1266
) {
  moonbit_string_t* _M0L3bufS3351;
  int32_t _M0L3lenS3352;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS3350;
  moonbit_string_t _result_4219;
  #line 2184 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3bufS3351 = _M0L4selfS1265->$0;
  _M0L3lenS3352 = _M0L4selfS1265->$1;
  moonbit_incref(_M0L3bufS3351);
  _M0L6_2atmpS3350
  = (struct _M0TPB9ArrayViewGsE){
    .$0 = _M0L3bufS3351, .$1 = 0, .$2 = _M0L3lenS3352
  };
  #line 2188 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _result_4219
  = _M0MPC15array9ArrayView4joinGsE(_M0L6_2atmpS3350, _M0L9separatorS1266);
  moonbit_decref(_M0L6_2atmpS3350.$0);
  return _result_4219;
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
    moonbit_string_t* _M0L3bufS3349 = _M0L4selfS1262->$0;
    moonbit_string_t _M0L1vS1264 =
      (moonbit_string_t)_M0L3bufS3349[_M0L5indexS1263];
    moonbit_string_t* _M0L3bufS3348 = _M0L4selfS1262->$0;
    moonbit_string_t _M0L6_2aoldS3752;
    if (
      _M0L5indexS1263 < 0
      || _M0L5indexS1263 >= Moonbit_array_length(_M0L3bufS3348)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3752 = (moonbit_string_t)_M0L3bufS3348[_M0L5indexS1263];
    moonbit_incref(_M0L1vS1264);
    moonbit_decref(_M0L6_2aoldS3752);
    if (
      _M0L5indexS1263 < 0
      || _M0L5indexS1263 >= Moonbit_array_length(_M0L3bufS3348)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS3348[_M0L5indexS1263]
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
  int32_t _if__result_4220;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1252 = _M0L4selfS1253->$1;
  if (_M0L5indexS1254 >= 0) {
    _if__result_4220 = _M0L5indexS1254 < _M0L3lenS1252;
  } else {
    _if__result_4220 = 0;
  }
  if (_if__result_4220) {
    moonbit_string_t* _M0L6_2atmpS3345;
    moonbit_string_t _M0L6_2atmpS3756;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3345 = _M0MPC15array5Array6bufferGsE(_M0L4selfS1253);
    _M0L6_2atmpS3756 = (moonbit_string_t)_M0L6_2atmpS3345[_M0L5indexS1254];
    moonbit_incref(_M0L6_2atmpS3756);
    moonbit_decref(_M0L6_2atmpS3345);
    return _M0L6_2atmpS3756;
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
  int32_t _if__result_4221;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1255 = _M0L4selfS1256->$1;
  if (_M0L5indexS1257 >= 0) {
    _if__result_4221 = _M0L5indexS1257 < _M0L3lenS1255;
  } else {
    _if__result_4221 = 0;
  }
  if (_if__result_4221) {
    moonbit_string_t* _M0L6_2atmpS3346;
    moonbit_string_t _M0L6_2atmpS3757;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3346 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS1256);
    _M0L6_2atmpS3757 = (moonbit_string_t)_M0L6_2atmpS3346[_M0L5indexS1257];
    if (_M0L6_2atmpS3757) {
      moonbit_incref(_M0L6_2atmpS3757);
    }
    moonbit_decref(_M0L6_2atmpS3346);
    return _M0L6_2atmpS3757;
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
  int32_t _if__result_4222;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1258 = _M0L4selfS1259->$1;
  if (_M0L5indexS1260 >= 0) {
    _if__result_4222 = _M0L5indexS1260 < _M0L3lenS1258;
  } else {
    _if__result_4222 = 0;
  }
  if (_if__result_4222) {
    struct _M0TUsfE** _M0L6_2atmpS3347;
    struct _M0TUsfE* _M0L6_2atmpS3758;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3347 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS1259);
    _M0L6_2atmpS3758 = (struct _M0TUsfE*)_M0L6_2atmpS3347[_M0L5indexS1260];
    if (_M0L6_2atmpS3758) {
      moonbit_incref(_M0L6_2atmpS3758);
    }
    moonbit_decref(_M0L6_2atmpS3347);
    return _M0L6_2atmpS3758;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1251) {
  moonbit_string_t _M0L6_2atmpS3344;
  #line 36 "/home/developer/.moon/lib/core/builtin/console.mbt"
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS3344
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1251);
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS3344);
  moonbit_decref(_M0L6_2atmpS3344);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1250) {
  #line 282 "/home/developer/.moon/lib/core/builtin/double.mbt"
  #line 284 "/home/developer/.moon/lib/core/builtin/double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1250);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1237) {
  uint64_t _M0L4bitsS1238;
  uint64_t _M0L6_2atmpS3343;
  uint64_t _M0L6_2atmpS3342;
  int32_t _M0L8ieeeSignS1239;
  uint64_t _M0L12ieeeMantissaS1240;
  uint64_t _M0L6_2atmpS3341;
  uint64_t _M0L6_2atmpS3340;
  int32_t _M0L12ieeeExponentS1241;
  int32_t _if__result_4223;
  struct _M0TPB17FloatingDecimal64* _M0L7_2abindS1242;
  struct _M0TPB17FloatingDecimal64* _M0L1vS1243;
  moonbit_string_t _result_4225;
  #line 659 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L3valS1237 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_84.data;
  }
  _M0L4bitsS1238 = *(int64_t*)&_M0L3valS1237;
  _M0L6_2atmpS3343 = _M0L4bitsS1238 >> 63;
  _M0L6_2atmpS3342 = _M0L6_2atmpS3343 & 1ull;
  _M0L8ieeeSignS1239 = _M0L6_2atmpS3342 != 0ull;
  _M0L12ieeeMantissaS1240 = _M0L4bitsS1238 & 4503599627370495ull;
  _M0L6_2atmpS3341 = _M0L4bitsS1238 >> 52;
  _M0L6_2atmpS3340 = _M0L6_2atmpS3341 & 2047ull;
  _M0L12ieeeExponentS1241 = (int32_t)_M0L6_2atmpS3340;
  if (_M0L12ieeeExponentS1241 == 2047) {
    _if__result_4223 = 1;
  } else if (_M0L12ieeeExponentS1241 == 0) {
    _if__result_4223 = _M0L12ieeeMantissaS1240 == 0ull;
  } else {
    _if__result_4223 = 0;
  }
  if (_if__result_4223) {
    int32_t _M0L6_2atmpS3331 = _M0L12ieeeExponentS1241 != 0;
    int32_t _M0L6_2atmpS3332 = _M0L12ieeeMantissaS1240 != 0ull;
    #line 676 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1239, _M0L6_2atmpS3331, _M0L6_2atmpS3332);
  }
  #line 678 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1242
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1240, _M0L12ieeeExponentS1241);
  if (_M0L7_2abindS1242 == 0) {
    uint32_t _M0L6_2atmpS3333;
    if (_M0L7_2abindS1242) {
      moonbit_decref(_M0L7_2abindS1242);
    }
    _M0L6_2atmpS3333 = *(uint32_t*)&_M0L12ieeeExponentS1241;
    #line 688 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L1vS1243 = _M0FPB3d2d(_M0L12ieeeMantissaS1240, _M0L6_2atmpS3333);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1244 = _M0L7_2abindS1242;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1245 = _M0L7_2aSomeS1244;
    struct _M0TPB17FloatingDecimal64* _M0L1xS1246 = _M0L4_2afS1245;
    while (1) {
      uint64_t _M0L8mantissaS3339 = _M0L1xS1246->$0;
      uint64_t _M0L1qS1247 = _M0L8mantissaS3339 / 10ull;
      uint64_t _M0L8mantissaS3337 = _M0L1xS1246->$0;
      uint64_t _M0L6_2atmpS3338 = 10ull * _M0L1qS1247;
      uint64_t _M0L1rS1248 = _M0L8mantissaS3337 - _M0L6_2atmpS3338;
      int32_t _M0L8exponentS3336;
      int32_t _M0L6_2atmpS3335;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3334;
      if (_M0L1rS1248 != 0ull) {
        _M0L1vS1243 = _M0L1xS1246;
        break;
      }
      _M0L8exponentS3336 = _M0L1xS1246->$1;
      moonbit_decref(_M0L1xS1246);
      _M0L6_2atmpS3335 = _M0L8exponentS3336 + 1;
      _M0L6_2atmpS3334
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0L6_2atmpS3334)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L6_2atmpS3334->$0 = _M0L1qS1247;
      _M0L6_2atmpS3334->$1 = _M0L6_2atmpS3335;
      _M0L1xS1246 = _M0L6_2atmpS3334;
      continue;
      break;
    }
  }
  #line 690 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_4225 = _M0FPB9to__chars(_M0L1vS1243, _M0L8ieeeSignS1239);
  moonbit_decref(_M0L1vS1243);
  return _result_4225;
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1232,
  int32_t _M0L12ieeeExponentS1234
) {
  uint64_t _M0L2m2S1231;
  int32_t _M0L6_2atmpS3330;
  int32_t _M0L2e2S1233;
  int32_t _M0L6_2atmpS3329;
  uint64_t _M0L6_2atmpS3328;
  uint64_t _M0L4maskS1235;
  uint64_t _M0L8fractionS1236;
  int32_t _M0L6_2atmpS3327;
  uint64_t _M0L6_2atmpS3326;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3325;
  #line 637 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2m2S1231 = 4503599627370496ull | _M0L12ieeeMantissaS1232;
  _M0L6_2atmpS3330 = _M0L12ieeeExponentS1234 - 1023;
  _M0L2e2S1233 = _M0L6_2atmpS3330 - 52;
  if (_M0L2e2S1233 > 0) {
    return 0;
  }
  if (_M0L2e2S1233 < -52) {
    return 0;
  }
  _M0L6_2atmpS3329 = -_M0L2e2S1233;
  _M0L6_2atmpS3328 = 1ull << (_M0L6_2atmpS3329 & 63);
  _M0L4maskS1235 = _M0L6_2atmpS3328 - 1ull;
  _M0L8fractionS1236 = _M0L2m2S1231 & _M0L4maskS1235;
  if (_M0L8fractionS1236 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3327 = -_M0L2e2S1233;
  _M0L6_2atmpS3326 = _M0L2m2S1231 >> (_M0L6_2atmpS3327 & 63);
  _M0L6_2atmpS3325
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS3325)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6_2atmpS3325->$0 = _M0L6_2atmpS3326;
  _M0L6_2atmpS3325->$1 = 0;
  return _M0L6_2atmpS3325;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1199,
  int32_t _M0L4signS1197
) {
  int32_t _M0L6_2atmpS3324;
  moonbit_bytes_t _M0L6resultS1195;
  int32_t _M0Lm5indexS1196;
  uint64_t _M0L6outputS1198;
  int32_t _M0L7olengthS1200;
  int32_t _M0L8exponentS3323;
  int32_t _M0L6_2atmpS3322;
  int32_t _M0Lm3expS1201;
  int32_t _M0L6_2atmpS3321;
  int32_t _M0L6_2atmpS3319;
  int32_t _M0L18scientificNotationS1202;
  #line 530 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 532 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3324 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1195
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS3324);
  _M0Lm5indexS1196 = 0;
  if (_M0L4signS1197) {
    int32_t _M0L6_2atmpS3193 = _M0Lm5indexS1196;
    int32_t _M0L6_2atmpS3194;
    if (
      _M0L6_2atmpS3193 < 0
      || _M0L6_2atmpS3193 >= Moonbit_array_length(_M0L6resultS1195)
    ) {
      #line 535 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1195[_M0L6_2atmpS3193] = 45;
    _M0L6_2atmpS3194 = _M0Lm5indexS1196;
    _M0Lm5indexS1196 = _M0L6_2atmpS3194 + 1;
  }
  _M0L6outputS1198 = _M0L1vS1199->$0;
  #line 539 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7olengthS1200 = _M0FPB17decimal__length17(_M0L6outputS1198);
  _M0L8exponentS3323 = _M0L1vS1199->$1;
  _M0L6_2atmpS3322 = _M0L8exponentS3323 + _M0L7olengthS1200;
  _M0Lm3expS1201 = _M0L6_2atmpS3322 - 1;
  _M0L6_2atmpS3321 = _M0Lm3expS1201;
  if (_M0L6_2atmpS3321 >= -6) {
    int32_t _M0L6_2atmpS3320 = _M0Lm3expS1201;
    _M0L6_2atmpS3319 = _M0L6_2atmpS3320 < 21;
  } else {
    _M0L6_2atmpS3319 = 0;
  }
  _M0L18scientificNotationS1202 = !_M0L6_2atmpS3319;
  if (_M0L18scientificNotationS1202) {
    int32_t _M0L7_2abindS1203 = _M0L7olengthS1200 - 1;
    uint64_t _M0L6outputS1204;
    int32_t _M0L1iS1205 = 0;
    uint64_t _M0L6outputS1206 = _M0L6outputS1198;
    int32_t _M0L6_2atmpS3195;
    int32_t _M0L6_2atmpS3199;
    int32_t _M0L6_2atmpS3198;
    int32_t _M0L6_2atmpS3197;
    int32_t _M0L6_2atmpS3196;
    int32_t _M0L6_2atmpS3203;
    int32_t _M0L6_2atmpS3204;
    int32_t _M0L6_2atmpS3205;
    int32_t _M0L6_2atmpS3206;
    int32_t _M0L6_2atmpS3207;
    int32_t _M0L6_2atmpS3213;
    int32_t _M0L6_2atmpS3246;
    moonbit_string_t _result_4227;
    while (1) {
      if (_M0L1iS1205 < _M0L7_2abindS1203) {
        uint64_t _M0L1cS1207 = _M0L6outputS1206 % 10ull;
        int32_t _M0L6_2atmpS3252 = _M0Lm5indexS1196;
        int32_t _M0L6_2atmpS3251 = _M0L6_2atmpS3252 + _M0L7olengthS1200;
        int32_t _M0L6_2atmpS3247 = _M0L6_2atmpS3251 - _M0L1iS1205;
        int32_t _M0L6_2atmpS3250 = (int32_t)_M0L1cS1207;
        int32_t _M0L6_2atmpS3249 = 48 + _M0L6_2atmpS3250;
        int32_t _M0L6_2atmpS3248 = _M0L6_2atmpS3249 & 0xff;
        int32_t _M0L6_2atmpS3253;
        uint64_t _M0L6_2atmpS3254;
        if (
          _M0L6_2atmpS3247 < 0
          || _M0L6_2atmpS3247 >= Moonbit_array_length(_M0L6resultS1195)
        ) {
          #line 547 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1195[_M0L6_2atmpS3247] = _M0L6_2atmpS3248;
        _M0L6_2atmpS3253 = _M0L1iS1205 + 1;
        _M0L6_2atmpS3254 = _M0L6outputS1206 / 10ull;
        _M0L1iS1205 = _M0L6_2atmpS3253;
        _M0L6outputS1206 = _M0L6_2atmpS3254;
        continue;
      } else {
        _M0L6outputS1204 = _M0L6outputS1206;
      }
      break;
    }
    _M0L6_2atmpS3195 = _M0Lm5indexS1196;
    _M0L6_2atmpS3199 = (int32_t)_M0L6outputS1204;
    _M0L6_2atmpS3198 = _M0L6_2atmpS3199 % 10;
    _M0L6_2atmpS3197 = 48 + _M0L6_2atmpS3198;
    _M0L6_2atmpS3196 = _M0L6_2atmpS3197 & 0xff;
    if (
      _M0L6_2atmpS3195 < 0
      || _M0L6_2atmpS3195 >= Moonbit_array_length(_M0L6resultS1195)
    ) {
      #line 552 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1195[_M0L6_2atmpS3195] = _M0L6_2atmpS3196;
    if (_M0L7olengthS1200 > 1) {
      int32_t _M0L6_2atmpS3201 = _M0Lm5indexS1196;
      int32_t _M0L6_2atmpS3200 = _M0L6_2atmpS3201 + 1;
      if (
        _M0L6_2atmpS3200 < 0
        || _M0L6_2atmpS3200 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 554 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3200] = 46;
    } else {
      int32_t _M0L6_2atmpS3202 = _M0Lm5indexS1196;
      _M0Lm5indexS1196 = _M0L6_2atmpS3202 - 1;
    }
    _M0L6_2atmpS3203 = _M0Lm5indexS1196;
    _M0L6_2atmpS3204 = _M0L7olengthS1200 + 1;
    _M0Lm5indexS1196 = _M0L6_2atmpS3203 + _M0L6_2atmpS3204;
    _M0L6_2atmpS3205 = _M0Lm5indexS1196;
    if (
      _M0L6_2atmpS3205 < 0
      || _M0L6_2atmpS3205 >= Moonbit_array_length(_M0L6resultS1195)
    ) {
      #line 562 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1195[_M0L6_2atmpS3205] = 101;
    _M0L6_2atmpS3206 = _M0Lm5indexS1196;
    _M0Lm5indexS1196 = _M0L6_2atmpS3206 + 1;
    _M0L6_2atmpS3207 = _M0Lm3expS1201;
    if (_M0L6_2atmpS3207 < 0) {
      int32_t _M0L6_2atmpS3208 = _M0Lm5indexS1196;
      int32_t _M0L6_2atmpS3209;
      int32_t _M0L6_2atmpS3210;
      if (
        _M0L6_2atmpS3208 < 0
        || _M0L6_2atmpS3208 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 565 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3208] = 45;
      _M0L6_2atmpS3209 = _M0Lm5indexS1196;
      _M0Lm5indexS1196 = _M0L6_2atmpS3209 + 1;
      _M0L6_2atmpS3210 = _M0Lm3expS1201;
      _M0Lm3expS1201 = -_M0L6_2atmpS3210;
    } else {
      int32_t _M0L6_2atmpS3211 = _M0Lm5indexS1196;
      int32_t _M0L6_2atmpS3212;
      if (
        _M0L6_2atmpS3211 < 0
        || _M0L6_2atmpS3211 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 569 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3211] = 43;
      _M0L6_2atmpS3212 = _M0Lm5indexS1196;
      _M0Lm5indexS1196 = _M0L6_2atmpS3212 + 1;
    }
    _M0L6_2atmpS3213 = _M0Lm3expS1201;
    if (_M0L6_2atmpS3213 >= 100) {
      int32_t _M0L6_2atmpS3229 = _M0Lm3expS1201;
      int32_t _M0L1aS1209 = _M0L6_2atmpS3229 / 100;
      int32_t _M0L6_2atmpS3228 = _M0Lm3expS1201;
      int32_t _M0L6_2atmpS3227 = _M0L6_2atmpS3228 / 10;
      int32_t _M0L1bS1210 = _M0L6_2atmpS3227 % 10;
      int32_t _M0L6_2atmpS3226 = _M0Lm3expS1201;
      int32_t _M0L1cS1211 = _M0L6_2atmpS3226 % 10;
      int32_t _M0L6_2atmpS3214 = _M0Lm5indexS1196;
      int32_t _M0L6_2atmpS3216 = 48 + _M0L1aS1209;
      int32_t _M0L6_2atmpS3215 = _M0L6_2atmpS3216 & 0xff;
      int32_t _M0L6_2atmpS3220;
      int32_t _M0L6_2atmpS3217;
      int32_t _M0L6_2atmpS3219;
      int32_t _M0L6_2atmpS3218;
      int32_t _M0L6_2atmpS3224;
      int32_t _M0L6_2atmpS3221;
      int32_t _M0L6_2atmpS3223;
      int32_t _M0L6_2atmpS3222;
      int32_t _M0L6_2atmpS3225;
      if (
        _M0L6_2atmpS3214 < 0
        || _M0L6_2atmpS3214 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 576 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3214] = _M0L6_2atmpS3215;
      _M0L6_2atmpS3220 = _M0Lm5indexS1196;
      _M0L6_2atmpS3217 = _M0L6_2atmpS3220 + 1;
      _M0L6_2atmpS3219 = 48 + _M0L1bS1210;
      _M0L6_2atmpS3218 = _M0L6_2atmpS3219 & 0xff;
      if (
        _M0L6_2atmpS3217 < 0
        || _M0L6_2atmpS3217 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 577 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3217] = _M0L6_2atmpS3218;
      _M0L6_2atmpS3224 = _M0Lm5indexS1196;
      _M0L6_2atmpS3221 = _M0L6_2atmpS3224 + 2;
      _M0L6_2atmpS3223 = 48 + _M0L1cS1211;
      _M0L6_2atmpS3222 = _M0L6_2atmpS3223 & 0xff;
      if (
        _M0L6_2atmpS3221 < 0
        || _M0L6_2atmpS3221 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 578 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3221] = _M0L6_2atmpS3222;
      _M0L6_2atmpS3225 = _M0Lm5indexS1196;
      _M0Lm5indexS1196 = _M0L6_2atmpS3225 + 3;
    } else {
      int32_t _M0L6_2atmpS3230 = _M0Lm3expS1201;
      if (_M0L6_2atmpS3230 >= 10) {
        int32_t _M0L6_2atmpS3240 = _M0Lm3expS1201;
        int32_t _M0L1aS1212 = _M0L6_2atmpS3240 / 10;
        int32_t _M0L6_2atmpS3239 = _M0Lm3expS1201;
        int32_t _M0L1bS1213 = _M0L6_2atmpS3239 % 10;
        int32_t _M0L6_2atmpS3231 = _M0Lm5indexS1196;
        int32_t _M0L6_2atmpS3233 = 48 + _M0L1aS1212;
        int32_t _M0L6_2atmpS3232 = _M0L6_2atmpS3233 & 0xff;
        int32_t _M0L6_2atmpS3237;
        int32_t _M0L6_2atmpS3234;
        int32_t _M0L6_2atmpS3236;
        int32_t _M0L6_2atmpS3235;
        int32_t _M0L6_2atmpS3238;
        if (
          _M0L6_2atmpS3231 < 0
          || _M0L6_2atmpS3231 >= Moonbit_array_length(_M0L6resultS1195)
        ) {
          #line 583 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1195[_M0L6_2atmpS3231] = _M0L6_2atmpS3232;
        _M0L6_2atmpS3237 = _M0Lm5indexS1196;
        _M0L6_2atmpS3234 = _M0L6_2atmpS3237 + 1;
        _M0L6_2atmpS3236 = 48 + _M0L1bS1213;
        _M0L6_2atmpS3235 = _M0L6_2atmpS3236 & 0xff;
        if (
          _M0L6_2atmpS3234 < 0
          || _M0L6_2atmpS3234 >= Moonbit_array_length(_M0L6resultS1195)
        ) {
          #line 584 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1195[_M0L6_2atmpS3234] = _M0L6_2atmpS3235;
        _M0L6_2atmpS3238 = _M0Lm5indexS1196;
        _M0Lm5indexS1196 = _M0L6_2atmpS3238 + 2;
      } else {
        int32_t _M0L6_2atmpS3241 = _M0Lm5indexS1196;
        int32_t _M0L6_2atmpS3244 = _M0Lm3expS1201;
        int32_t _M0L6_2atmpS3243 = 48 + _M0L6_2atmpS3244;
        int32_t _M0L6_2atmpS3242 = _M0L6_2atmpS3243 & 0xff;
        int32_t _M0L6_2atmpS3245;
        if (
          _M0L6_2atmpS3241 < 0
          || _M0L6_2atmpS3241 >= Moonbit_array_length(_M0L6resultS1195)
        ) {
          #line 587 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1195[_M0L6_2atmpS3241] = _M0L6_2atmpS3242;
        _M0L6_2atmpS3245 = _M0Lm5indexS1196;
        _M0Lm5indexS1196 = _M0L6_2atmpS3245 + 1;
      }
    }
    _M0L6_2atmpS3246 = _M0Lm5indexS1196;
    #line 590 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4227
    = _M0FPB19string__from__bytes(_M0L6resultS1195, 0, _M0L6_2atmpS3246);
    moonbit_decref(_M0L6resultS1195);
    return _result_4227;
  } else {
    int32_t _M0L6_2atmpS3255 = _M0Lm3expS1201;
    int32_t _M0L6_2atmpS3318;
    moonbit_string_t _result_4233;
    if (_M0L6_2atmpS3255 < 0) {
      int32_t _M0L6_2atmpS3256 = _M0Lm5indexS1196;
      int32_t _M0L6_2atmpS3258;
      int32_t _M0L6_2atmpS3257;
      int32_t _M0L6_2atmpS3259;
      int32_t _M0L1iS1214;
      int32_t _M0L6_2atmpS3274;
      int32_t _M0L6_2atmpS3276;
      int32_t _M0L6_2atmpS3275;
      int32_t _M0L7currentS1216;
      int32_t _M0L1iS1217;
      uint64_t _M0L6outputS1218;
      if (
        _M0L6_2atmpS3256 < 0
        || _M0L6_2atmpS3256 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 595 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3256] = 48;
      _M0L6_2atmpS3258 = _M0Lm5indexS1196;
      _M0L6_2atmpS3257 = _M0L6_2atmpS3258 + 1;
      if (
        _M0L6_2atmpS3257 < 0
        || _M0L6_2atmpS3257 >= Moonbit_array_length(_M0L6resultS1195)
      ) {
        #line 596 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1195[_M0L6_2atmpS3257] = 46;
      _M0L6_2atmpS3259 = _M0Lm5indexS1196;
      _M0Lm5indexS1196 = _M0L6_2atmpS3259 + 2;
      _M0L1iS1214 = -1;
      while (1) {
        int32_t _M0L6_2atmpS3260 = _M0Lm3expS1201;
        if (_M0L1iS1214 > _M0L6_2atmpS3260) {
          int32_t _M0L6_2atmpS3263 = _M0Lm5indexS1196;
          int32_t _M0L6_2atmpS3262 = _M0L6_2atmpS3263 - _M0L1iS1214;
          int32_t _M0L6_2atmpS3261 = _M0L6_2atmpS3262 - 1;
          int32_t _M0L6_2atmpS3264;
          if (
            _M0L6_2atmpS3261 < 0
            || _M0L6_2atmpS3261 >= Moonbit_array_length(_M0L6resultS1195)
          ) {
            #line 599 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1195[_M0L6_2atmpS3261] = 48;
          _M0L6_2atmpS3264 = _M0L1iS1214 - 1;
          _M0L1iS1214 = _M0L6_2atmpS3264;
          continue;
        }
        break;
      }
      _M0L6_2atmpS3274 = _M0Lm5indexS1196;
      _M0L6_2atmpS3276 = _M0Lm3expS1201;
      _M0L6_2atmpS3275 = -1 - _M0L6_2atmpS3276;
      _M0L7currentS1216 = _M0L6_2atmpS3274 + _M0L6_2atmpS3275;
      _M0L1iS1217 = 0;
      _M0L6outputS1218 = _M0L6outputS1198;
      while (1) {
        if (_M0L1iS1217 < _M0L7olengthS1200) {
          int32_t _M0L6_2atmpS3271 = _M0L7currentS1216 + _M0L7olengthS1200;
          int32_t _M0L6_2atmpS3270 = _M0L6_2atmpS3271 - _M0L1iS1217;
          int32_t _M0L6_2atmpS3265 = _M0L6_2atmpS3270 - 1;
          uint64_t _M0L6_2atmpS3269 = _M0L6outputS1218 % 10ull;
          int32_t _M0L6_2atmpS3268 = (int32_t)_M0L6_2atmpS3269;
          int32_t _M0L6_2atmpS3267 = 48 + _M0L6_2atmpS3268;
          int32_t _M0L6_2atmpS3266 = _M0L6_2atmpS3267 & 0xff;
          int32_t _M0L6_2atmpS3272;
          uint64_t _M0L6_2atmpS3273;
          if (
            _M0L6_2atmpS3265 < 0
            || _M0L6_2atmpS3265 >= Moonbit_array_length(_M0L6resultS1195)
          ) {
            #line 603 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1195[_M0L6_2atmpS3265] = _M0L6_2atmpS3266;
          _M0L6_2atmpS3272 = _M0L1iS1217 + 1;
          _M0L6_2atmpS3273 = _M0L6outputS1218 / 10ull;
          _M0L1iS1217 = _M0L6_2atmpS3272;
          _M0L6outputS1218 = _M0L6_2atmpS3273;
          continue;
        }
        break;
      }
      _M0Lm5indexS1196 = _M0L7currentS1216 + _M0L7olengthS1200;
    } else {
      int32_t _M0L6_2atmpS3278 = _M0Lm3expS1201;
      int32_t _M0L6_2atmpS3277 = _M0L6_2atmpS3278 + 1;
      if (_M0L6_2atmpS3277 >= _M0L7olengthS1200) {
        int32_t _M0L1iS1220 = 0;
        uint64_t _M0L6outputS1221 = _M0L6outputS1198;
        int32_t _M0L6_2atmpS3289;
        int32_t _M0L6_2atmpS3294;
        int32_t _M0L7_2abindS1223;
        int32_t _M0L1iS1224;
        int32_t _M0L6_2atmpS3295;
        int32_t _M0L6_2atmpS3298;
        int32_t _M0L6_2atmpS3297;
        int32_t _M0L6_2atmpS3296;
        while (1) {
          if (_M0L1iS1220 < _M0L7olengthS1200) {
            int32_t _M0L6_2atmpS3286 = _M0Lm5indexS1196;
            int32_t _M0L6_2atmpS3285 = _M0L6_2atmpS3286 + _M0L7olengthS1200;
            int32_t _M0L6_2atmpS3284 = _M0L6_2atmpS3285 - _M0L1iS1220;
            int32_t _M0L6_2atmpS3279 = _M0L6_2atmpS3284 - 1;
            uint64_t _M0L6_2atmpS3283 = _M0L6outputS1221 % 10ull;
            int32_t _M0L6_2atmpS3282 = (int32_t)_M0L6_2atmpS3283;
            int32_t _M0L6_2atmpS3281 = 48 + _M0L6_2atmpS3282;
            int32_t _M0L6_2atmpS3280 = _M0L6_2atmpS3281 & 0xff;
            int32_t _M0L6_2atmpS3287;
            uint64_t _M0L6_2atmpS3288;
            if (
              _M0L6_2atmpS3279 < 0
              || _M0L6_2atmpS3279 >= Moonbit_array_length(_M0L6resultS1195)
            ) {
              #line 610 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1195[_M0L6_2atmpS3279] = _M0L6_2atmpS3280;
            _M0L6_2atmpS3287 = _M0L1iS1220 + 1;
            _M0L6_2atmpS3288 = _M0L6outputS1221 / 10ull;
            _M0L1iS1220 = _M0L6_2atmpS3287;
            _M0L6outputS1221 = _M0L6_2atmpS3288;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3289 = _M0Lm5indexS1196;
        _M0Lm5indexS1196 = _M0L6_2atmpS3289 + _M0L7olengthS1200;
        _M0L6_2atmpS3294 = _M0Lm3expS1201;
        _M0L7_2abindS1223 = _M0L6_2atmpS3294 + 1;
        _M0L1iS1224 = _M0L7olengthS1200;
        while (1) {
          if (_M0L1iS1224 < _M0L7_2abindS1223) {
            int32_t _M0L6_2atmpS3292 = _M0Lm5indexS1196;
            int32_t _M0L6_2atmpS3291 = _M0L6_2atmpS3292 + _M0L1iS1224;
            int32_t _M0L6_2atmpS3290 = _M0L6_2atmpS3291 - _M0L7olengthS1200;
            int32_t _M0L6_2atmpS3293;
            if (
              _M0L6_2atmpS3290 < 0
              || _M0L6_2atmpS3290 >= Moonbit_array_length(_M0L6resultS1195)
            ) {
              #line 615 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1195[_M0L6_2atmpS3290] = 48;
            _M0L6_2atmpS3293 = _M0L1iS1224 + 1;
            _M0L1iS1224 = _M0L6_2atmpS3293;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3295 = _M0Lm5indexS1196;
        _M0L6_2atmpS3298 = _M0Lm3expS1201;
        _M0L6_2atmpS3297 = _M0L6_2atmpS3298 + 1;
        _M0L6_2atmpS3296 = _M0L6_2atmpS3297 - _M0L7olengthS1200;
        _M0Lm5indexS1196 = _M0L6_2atmpS3295 + _M0L6_2atmpS3296;
      } else {
        int32_t _M0L6_2atmpS3315 = _M0Lm5indexS1196;
        int32_t _M0L6_2atmpS3314 = _M0L6_2atmpS3315 + 1;
        int32_t _M0L1iS1226 = 0;
        int32_t _M0L7currentS1227 = _M0L6_2atmpS3314;
        uint64_t _M0L6outputS1228 = _M0L6outputS1198;
        int32_t _M0L6_2atmpS3316;
        int32_t _M0L6_2atmpS3317;
        while (1) {
          if (_M0L1iS1226 < _M0L7olengthS1200) {
            int32_t _M0L6_2atmpS3310 = _M0L7olengthS1200 - _M0L1iS1226;
            int32_t _M0L6_2atmpS3308 = _M0L6_2atmpS3310 - 1;
            int32_t _M0L6_2atmpS3309 = _M0Lm3expS1201;
            int32_t _M0L7currentS1229;
            int32_t _M0L6_2atmpS3305;
            int32_t _M0L6_2atmpS3304;
            int32_t _M0L6_2atmpS3299;
            uint64_t _M0L6_2atmpS3303;
            int32_t _M0L6_2atmpS3302;
            int32_t _M0L6_2atmpS3301;
            int32_t _M0L6_2atmpS3300;
            int32_t _M0L6_2atmpS3306;
            uint64_t _M0L6_2atmpS3307;
            if (_M0L6_2atmpS3308 == _M0L6_2atmpS3309) {
              int32_t _M0L6_2atmpS3313 =
                _M0L7currentS1227 + _M0L7olengthS1200;
              int32_t _M0L6_2atmpS3312 = _M0L6_2atmpS3313 - _M0L1iS1226;
              int32_t _M0L6_2atmpS3311 = _M0L6_2atmpS3312 - 1;
              if (
                _M0L6_2atmpS3311 < 0
                || _M0L6_2atmpS3311 >= Moonbit_array_length(_M0L6resultS1195)
              ) {
                #line 622 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1195[_M0L6_2atmpS3311] = 46;
              _M0L7currentS1229 = _M0L7currentS1227 - 1;
            } else {
              _M0L7currentS1229 = _M0L7currentS1227;
            }
            _M0L6_2atmpS3305 = _M0L7currentS1229 + _M0L7olengthS1200;
            _M0L6_2atmpS3304 = _M0L6_2atmpS3305 - _M0L1iS1226;
            _M0L6_2atmpS3299 = _M0L6_2atmpS3304 - 1;
            _M0L6_2atmpS3303 = _M0L6outputS1228 % 10ull;
            _M0L6_2atmpS3302 = (int32_t)_M0L6_2atmpS3303;
            _M0L6_2atmpS3301 = 48 + _M0L6_2atmpS3302;
            _M0L6_2atmpS3300 = _M0L6_2atmpS3301 & 0xff;
            if (
              _M0L6_2atmpS3299 < 0
              || _M0L6_2atmpS3299 >= Moonbit_array_length(_M0L6resultS1195)
            ) {
              #line 627 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1195[_M0L6_2atmpS3299] = _M0L6_2atmpS3300;
            _M0L6_2atmpS3306 = _M0L1iS1226 + 1;
            _M0L6_2atmpS3307 = _M0L6outputS1228 / 10ull;
            _M0L1iS1226 = _M0L6_2atmpS3306;
            _M0L7currentS1227 = _M0L7currentS1229;
            _M0L6outputS1228 = _M0L6_2atmpS3307;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3316 = _M0Lm5indexS1196;
        _M0L6_2atmpS3317 = _M0L7olengthS1200 + 1;
        _M0Lm5indexS1196 = _M0L6_2atmpS3316 + _M0L6_2atmpS3317;
      }
    }
    _M0L6_2atmpS3318 = _M0Lm5indexS1196;
    #line 632 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4233
    = _M0FPB19string__from__bytes(_M0L6resultS1195, 0, _M0L6_2atmpS3318);
    moonbit_decref(_M0L6resultS1195);
    return _result_4233;
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1141,
  uint32_t _M0L12ieeeExponentS1140
) {
  int32_t _M0Lm2e2S1138;
  uint64_t _M0Lm2m2S1139;
  uint64_t _M0L6_2atmpS3192;
  uint64_t _M0L6_2atmpS3191;
  int32_t _M0L4evenS1142;
  uint64_t _M0L6_2atmpS3190;
  uint64_t _M0L2mvS1143;
  int32_t _M0L7mmShiftS1144;
  uint64_t _M0Lm2vrS1145;
  uint64_t _M0Lm2vpS1146;
  uint64_t _M0Lm2vmS1147;
  int32_t _M0Lm3e10S1148;
  int32_t _M0Lm17vmIsTrailingZerosS1149;
  int32_t _M0Lm17vrIsTrailingZerosS1150;
  int32_t _M0L6_2atmpS3092;
  int32_t _M0Lm7removedS1169;
  int32_t _M0Lm16lastRemovedDigitS1170;
  uint64_t _M0Lm6outputS1171;
  int32_t _M0L6_2atmpS3188;
  int32_t _M0L6_2atmpS3189;
  int32_t _M0L3expS1194;
  uint64_t _M0L6_2atmpS3187;
  struct _M0TPB17FloatingDecimal64* _block_4239;
  #line 347 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0Lm2e2S1138 = 0;
  _M0Lm2m2S1139 = 0ull;
  if (_M0L12ieeeExponentS1140 == 0u) {
    _M0Lm2e2S1138 = -1076;
    _M0Lm2m2S1139 = _M0L12ieeeMantissaS1141;
  } else {
    int32_t _M0L6_2atmpS3091 = *(int32_t*)&_M0L12ieeeExponentS1140;
    int32_t _M0L6_2atmpS3090 = _M0L6_2atmpS3091 - 1023;
    int32_t _M0L6_2atmpS3089 = _M0L6_2atmpS3090 - 52;
    _M0Lm2e2S1138 = _M0L6_2atmpS3089 - 2;
    _M0Lm2m2S1139 = 4503599627370496ull | _M0L12ieeeMantissaS1141;
  }
  _M0L6_2atmpS3192 = _M0Lm2m2S1139;
  _M0L6_2atmpS3191 = _M0L6_2atmpS3192 & 1ull;
  _M0L4evenS1142 = _M0L6_2atmpS3191 == 0ull;
  _M0L6_2atmpS3190 = _M0Lm2m2S1139;
  _M0L2mvS1143 = 4ull * _M0L6_2atmpS3190;
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
  _M0L6_2atmpS3092 = _M0Lm2e2S1138;
  if (_M0L6_2atmpS3092 >= 0) {
    int32_t _M0L6_2atmpS3114 = _M0Lm2e2S1138;
    int32_t _M0L6_2atmpS3110;
    int32_t _M0L6_2atmpS3113;
    int32_t _M0L6_2atmpS3112;
    int32_t _M0L6_2atmpS3111;
    int32_t _M0L1qS1151;
    int32_t _M0L6_2atmpS3109;
    int32_t _M0L6_2atmpS3108;
    int32_t _M0L1kS1152;
    int32_t _M0L6_2atmpS3107;
    int32_t _M0L6_2atmpS3106;
    int32_t _M0L6_2atmpS3105;
    int32_t _M0L1iS1153;
    struct _M0TPB8Pow5Pair _M0L4pow5S1154;
    uint64_t _M0L6_2atmpS3104;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1155;
    uint64_t _M0L8_2avrOutS1156;
    uint64_t _M0L8_2avpOutS1157;
    uint64_t _M0L8_2avmOutS1158;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3110 = _M0FPB9log10Pow2(_M0L6_2atmpS3114);
    _M0L6_2atmpS3113 = _M0Lm2e2S1138;
    _M0L6_2atmpS3112 = _M0L6_2atmpS3113 > 3;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3111 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS3112);
    _M0L1qS1151 = _M0L6_2atmpS3110 - _M0L6_2atmpS3111;
    _M0Lm3e10S1148 = _M0L1qS1151;
    #line 385 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3109 = _M0FPB8pow5bits(_M0L1qS1151);
    _M0L6_2atmpS3108 = 125 + _M0L6_2atmpS3109;
    _M0L1kS1152 = _M0L6_2atmpS3108 - 1;
    _M0L6_2atmpS3107 = _M0Lm2e2S1138;
    _M0L6_2atmpS3106 = -_M0L6_2atmpS3107;
    _M0L6_2atmpS3105 = _M0L6_2atmpS3106 + _M0L1qS1151;
    _M0L1iS1153 = _M0L6_2atmpS3105 + _M0L1kS1152;
    #line 387 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1154 = _M0FPB22double__computeInvPow5(_M0L1qS1151);
    _M0L6_2atmpS3104 = _M0Lm2m2S1139;
    #line 388 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1155
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS3104, _M0L4pow5S1154, _M0L1iS1153, _M0L7mmShiftS1144);
    _M0L8_2avrOutS1156 = _M0L7_2abindS1155.$0;
    _M0L8_2avpOutS1157 = _M0L7_2abindS1155.$1;
    _M0L8_2avmOutS1158 = _M0L7_2abindS1155.$2;
    _M0Lm2vrS1145 = _M0L8_2avrOutS1156;
    _M0Lm2vpS1146 = _M0L8_2avpOutS1157;
    _M0Lm2vmS1147 = _M0L8_2avmOutS1158;
    if (_M0L1qS1151 <= 21) {
      int32_t _M0L6_2atmpS3100 = (int32_t)_M0L2mvS1143;
      uint64_t _M0L6_2atmpS3103 = _M0L2mvS1143 / 5ull;
      int32_t _M0L6_2atmpS3102 = (int32_t)_M0L6_2atmpS3103;
      int32_t _M0L6_2atmpS3101 = 5 * _M0L6_2atmpS3102;
      int32_t _M0L6mvMod5S1159 = _M0L6_2atmpS3100 - _M0L6_2atmpS3101;
      if (_M0L6mvMod5S1159 == 0) {
        #line 400 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1150
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1143, _M0L1qS1151);
      } else if (_M0L4evenS1142) {
        uint64_t _M0L6_2atmpS3094 = _M0L2mvS1143 - 1ull;
        uint64_t _M0L6_2atmpS3095;
        uint64_t _M0L6_2atmpS3093;
        #line 406 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3095 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1144);
        _M0L6_2atmpS3093 = _M0L6_2atmpS3094 - _M0L6_2atmpS3095;
        #line 405 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1149
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS3093, _M0L1qS1151);
      } else {
        uint64_t _M0L6_2atmpS3096 = _M0Lm2vpS1146;
        uint64_t _M0L6_2atmpS3099 = _M0L2mvS1143 + 2ull;
        int32_t _M0L6_2atmpS3098;
        uint64_t _M0L6_2atmpS3097;
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3098
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS3099, _M0L1qS1151);
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3097 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS3098);
        _M0Lm2vpS1146 = _M0L6_2atmpS3096 - _M0L6_2atmpS3097;
      }
    }
  } else {
    int32_t _M0L6_2atmpS3128 = _M0Lm2e2S1138;
    int32_t _M0L6_2atmpS3127 = -_M0L6_2atmpS3128;
    int32_t _M0L6_2atmpS3122;
    int32_t _M0L6_2atmpS3126;
    int32_t _M0L6_2atmpS3125;
    int32_t _M0L6_2atmpS3124;
    int32_t _M0L6_2atmpS3123;
    int32_t _M0L1qS1160;
    int32_t _M0L6_2atmpS3115;
    int32_t _M0L6_2atmpS3121;
    int32_t _M0L6_2atmpS3120;
    int32_t _M0L1iS1161;
    int32_t _M0L6_2atmpS3119;
    int32_t _M0L1kS1162;
    int32_t _M0L1jS1163;
    struct _M0TPB8Pow5Pair _M0L4pow5S1164;
    uint64_t _M0L6_2atmpS3118;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1165;
    uint64_t _M0L8_2avrOutS1166;
    uint64_t _M0L8_2avpOutS1167;
    uint64_t _M0L8_2avmOutS1168;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3122 = _M0FPB9log10Pow5(_M0L6_2atmpS3127);
    _M0L6_2atmpS3126 = _M0Lm2e2S1138;
    _M0L6_2atmpS3125 = -_M0L6_2atmpS3126;
    _M0L6_2atmpS3124 = _M0L6_2atmpS3125 > 1;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3123 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS3124);
    _M0L1qS1160 = _M0L6_2atmpS3122 - _M0L6_2atmpS3123;
    _M0L6_2atmpS3115 = _M0Lm2e2S1138;
    _M0Lm3e10S1148 = _M0L1qS1160 + _M0L6_2atmpS3115;
    _M0L6_2atmpS3121 = _M0Lm2e2S1138;
    _M0L6_2atmpS3120 = -_M0L6_2atmpS3121;
    _M0L1iS1161 = _M0L6_2atmpS3120 - _M0L1qS1160;
    #line 418 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3119 = _M0FPB8pow5bits(_M0L1iS1161);
    _M0L1kS1162 = _M0L6_2atmpS3119 - 125;
    _M0L1jS1163 = _M0L1qS1160 - _M0L1kS1162;
    #line 420 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1164 = _M0FPB19double__computePow5(_M0L1iS1161);
    _M0L6_2atmpS3118 = _M0Lm2m2S1139;
    #line 421 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1165
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS3118, _M0L4pow5S1164, _M0L1jS1163, _M0L7mmShiftS1144);
    _M0L8_2avrOutS1166 = _M0L7_2abindS1165.$0;
    _M0L8_2avpOutS1167 = _M0L7_2abindS1165.$1;
    _M0L8_2avmOutS1168 = _M0L7_2abindS1165.$2;
    _M0Lm2vrS1145 = _M0L8_2avrOutS1166;
    _M0Lm2vpS1146 = _M0L8_2avpOutS1167;
    _M0Lm2vmS1147 = _M0L8_2avmOutS1168;
    if (_M0L1qS1160 <= 1) {
      _M0Lm17vrIsTrailingZerosS1150 = 1;
      if (_M0L4evenS1142) {
        int32_t _M0L6_2atmpS3116;
        #line 432 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3116 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1144);
        _M0Lm17vmIsTrailingZerosS1149 = _M0L6_2atmpS3116 == 1;
      } else {
        uint64_t _M0L6_2atmpS3117 = _M0Lm2vpS1146;
        _M0Lm2vpS1146 = _M0L6_2atmpS3117 - 1ull;
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
    int32_t _if__result_4236;
    uint64_t _M0L6_2atmpS3158;
    uint64_t _M0L6_2atmpS3164;
    uint64_t _M0L6_2atmpS3165;
    int32_t _if__result_4237;
    int32_t _M0L6_2atmpS3161;
    int64_t _M0L6_2atmpS3160;
    uint64_t _M0L6_2atmpS3159;
    while (1) {
      uint64_t _M0L6_2atmpS3141 = _M0Lm2vpS1146;
      uint64_t _M0L7vpDiv10S1172 = _M0L6_2atmpS3141 / 10ull;
      uint64_t _M0L6_2atmpS3140 = _M0Lm2vmS1147;
      uint64_t _M0L7vmDiv10S1173 = _M0L6_2atmpS3140 / 10ull;
      uint64_t _M0L6_2atmpS3139;
      int32_t _M0L6_2atmpS3136;
      int32_t _M0L6_2atmpS3138;
      int32_t _M0L6_2atmpS3137;
      int32_t _M0L7vmMod10S1175;
      uint64_t _M0L6_2atmpS3135;
      uint64_t _M0L7vrDiv10S1176;
      uint64_t _M0L6_2atmpS3134;
      int32_t _M0L6_2atmpS3131;
      int32_t _M0L6_2atmpS3133;
      int32_t _M0L6_2atmpS3132;
      int32_t _M0L7vrMod10S1177;
      int32_t _M0L6_2atmpS3130;
      if (_M0L7vpDiv10S1172 <= _M0L7vmDiv10S1173) {
        break;
      }
      _M0L6_2atmpS3139 = _M0Lm2vmS1147;
      _M0L6_2atmpS3136 = (int32_t)_M0L6_2atmpS3139;
      _M0L6_2atmpS3138 = (int32_t)_M0L7vmDiv10S1173;
      _M0L6_2atmpS3137 = 10 * _M0L6_2atmpS3138;
      _M0L7vmMod10S1175 = _M0L6_2atmpS3136 - _M0L6_2atmpS3137;
      _M0L6_2atmpS3135 = _M0Lm2vrS1145;
      _M0L7vrDiv10S1176 = _M0L6_2atmpS3135 / 10ull;
      _M0L6_2atmpS3134 = _M0Lm2vrS1145;
      _M0L6_2atmpS3131 = (int32_t)_M0L6_2atmpS3134;
      _M0L6_2atmpS3133 = (int32_t)_M0L7vrDiv10S1176;
      _M0L6_2atmpS3132 = 10 * _M0L6_2atmpS3133;
      _M0L7vrMod10S1177 = _M0L6_2atmpS3131 - _M0L6_2atmpS3132;
      if (_M0Lm17vmIsTrailingZerosS1149) {
        _M0Lm17vmIsTrailingZerosS1149 = _M0L7vmMod10S1175 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1149 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1150) {
        int32_t _M0L6_2atmpS3129 = _M0Lm16lastRemovedDigitS1170;
        _M0Lm17vrIsTrailingZerosS1150 = _M0L6_2atmpS3129 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1150 = 0;
      }
      _M0Lm16lastRemovedDigitS1170 = _M0L7vrMod10S1177;
      _M0Lm2vrS1145 = _M0L7vrDiv10S1176;
      _M0Lm2vpS1146 = _M0L7vpDiv10S1172;
      _M0Lm2vmS1147 = _M0L7vmDiv10S1173;
      _M0L6_2atmpS3130 = _M0Lm7removedS1169;
      _M0Lm7removedS1169 = _M0L6_2atmpS3130 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1149) {
      while (1) {
        uint64_t _M0L6_2atmpS3154 = _M0Lm2vmS1147;
        uint64_t _M0L7vmDiv10S1178 = _M0L6_2atmpS3154 / 10ull;
        uint64_t _M0L6_2atmpS3153 = _M0Lm2vmS1147;
        int32_t _M0L6_2atmpS3150 = (int32_t)_M0L6_2atmpS3153;
        int32_t _M0L6_2atmpS3152 = (int32_t)_M0L7vmDiv10S1178;
        int32_t _M0L6_2atmpS3151 = 10 * _M0L6_2atmpS3152;
        int32_t _M0L7vmMod10S1179 = _M0L6_2atmpS3150 - _M0L6_2atmpS3151;
        uint64_t _M0L6_2atmpS3149;
        uint64_t _M0L7vpDiv10S1181;
        uint64_t _M0L6_2atmpS3148;
        uint64_t _M0L7vrDiv10S1182;
        uint64_t _M0L6_2atmpS3147;
        int32_t _M0L6_2atmpS3144;
        int32_t _M0L6_2atmpS3146;
        int32_t _M0L6_2atmpS3145;
        int32_t _M0L7vrMod10S1183;
        int32_t _M0L6_2atmpS3143;
        if (_M0L7vmMod10S1179 != 0) {
          break;
        }
        _M0L6_2atmpS3149 = _M0Lm2vpS1146;
        _M0L7vpDiv10S1181 = _M0L6_2atmpS3149 / 10ull;
        _M0L6_2atmpS3148 = _M0Lm2vrS1145;
        _M0L7vrDiv10S1182 = _M0L6_2atmpS3148 / 10ull;
        _M0L6_2atmpS3147 = _M0Lm2vrS1145;
        _M0L6_2atmpS3144 = (int32_t)_M0L6_2atmpS3147;
        _M0L6_2atmpS3146 = (int32_t)_M0L7vrDiv10S1182;
        _M0L6_2atmpS3145 = 10 * _M0L6_2atmpS3146;
        _M0L7vrMod10S1183 = _M0L6_2atmpS3144 - _M0L6_2atmpS3145;
        if (_M0Lm17vrIsTrailingZerosS1150) {
          int32_t _M0L6_2atmpS3142 = _M0Lm16lastRemovedDigitS1170;
          _M0Lm17vrIsTrailingZerosS1150 = _M0L6_2atmpS3142 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1150 = 0;
        }
        _M0Lm16lastRemovedDigitS1170 = _M0L7vrMod10S1183;
        _M0Lm2vrS1145 = _M0L7vrDiv10S1182;
        _M0Lm2vpS1146 = _M0L7vpDiv10S1181;
        _M0Lm2vmS1147 = _M0L7vmDiv10S1178;
        _M0L6_2atmpS3143 = _M0Lm7removedS1169;
        _M0Lm7removedS1169 = _M0L6_2atmpS3143 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1150) {
      int32_t _M0L6_2atmpS3157 = _M0Lm16lastRemovedDigitS1170;
      if (_M0L6_2atmpS3157 == 5) {
        uint64_t _M0L6_2atmpS3156 = _M0Lm2vrS1145;
        uint64_t _M0L6_2atmpS3155 = _M0L6_2atmpS3156 % 2ull;
        _if__result_4236 = _M0L6_2atmpS3155 == 0ull;
      } else {
        _if__result_4236 = 0;
      }
    } else {
      _if__result_4236 = 0;
    }
    if (_if__result_4236) {
      _M0Lm16lastRemovedDigitS1170 = 4;
    }
    _M0L6_2atmpS3158 = _M0Lm2vrS1145;
    _M0L6_2atmpS3164 = _M0Lm2vrS1145;
    _M0L6_2atmpS3165 = _M0Lm2vmS1147;
    if (_M0L6_2atmpS3164 == _M0L6_2atmpS3165) {
      if (!_M0L4evenS1142) {
        _if__result_4237 = 1;
      } else {
        int32_t _M0L6_2atmpS3163 = _M0Lm17vmIsTrailingZerosS1149;
        _if__result_4237 = !_M0L6_2atmpS3163;
      }
    } else {
      _if__result_4237 = 0;
    }
    if (_if__result_4237) {
      _M0L6_2atmpS3161 = 1;
    } else {
      int32_t _M0L6_2atmpS3162 = _M0Lm16lastRemovedDigitS1170;
      _M0L6_2atmpS3161 = _M0L6_2atmpS3162 >= 5;
    }
    #line 487 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3160 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS3161);
    _M0L6_2atmpS3159 = *(uint64_t*)&_M0L6_2atmpS3160;
    _M0Lm6outputS1171 = _M0L6_2atmpS3158 + _M0L6_2atmpS3159;
  } else {
    int32_t _M0Lm7roundUpS1184 = 0;
    uint64_t _M0L6_2atmpS3186 = _M0Lm2vpS1146;
    uint64_t _M0L8vpDiv100S1185 = _M0L6_2atmpS3186 / 100ull;
    uint64_t _M0L6_2atmpS3185 = _M0Lm2vmS1147;
    uint64_t _M0L8vmDiv100S1186 = _M0L6_2atmpS3185 / 100ull;
    uint64_t _M0L6_2atmpS3180;
    uint64_t _M0L6_2atmpS3183;
    uint64_t _M0L6_2atmpS3184;
    int32_t _M0L6_2atmpS3182;
    uint64_t _M0L6_2atmpS3181;
    if (_M0L8vpDiv100S1185 > _M0L8vmDiv100S1186) {
      uint64_t _M0L6_2atmpS3171 = _M0Lm2vrS1145;
      uint64_t _M0L8vrDiv100S1187 = _M0L6_2atmpS3171 / 100ull;
      uint64_t _M0L6_2atmpS3170 = _M0Lm2vrS1145;
      int32_t _M0L6_2atmpS3167 = (int32_t)_M0L6_2atmpS3170;
      int32_t _M0L6_2atmpS3169 = (int32_t)_M0L8vrDiv100S1187;
      int32_t _M0L6_2atmpS3168 = 100 * _M0L6_2atmpS3169;
      int32_t _M0L8vrMod100S1188 = _M0L6_2atmpS3167 - _M0L6_2atmpS3168;
      int32_t _M0L6_2atmpS3166;
      _M0Lm7roundUpS1184 = _M0L8vrMod100S1188 >= 50;
      _M0Lm2vrS1145 = _M0L8vrDiv100S1187;
      _M0Lm2vpS1146 = _M0L8vpDiv100S1185;
      _M0Lm2vmS1147 = _M0L8vmDiv100S1186;
      _M0L6_2atmpS3166 = _M0Lm7removedS1169;
      _M0Lm7removedS1169 = _M0L6_2atmpS3166 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS3179 = _M0Lm2vpS1146;
      uint64_t _M0L7vpDiv10S1189 = _M0L6_2atmpS3179 / 10ull;
      uint64_t _M0L6_2atmpS3178 = _M0Lm2vmS1147;
      uint64_t _M0L7vmDiv10S1190 = _M0L6_2atmpS3178 / 10ull;
      uint64_t _M0L6_2atmpS3177;
      uint64_t _M0L7vrDiv10S1192;
      uint64_t _M0L6_2atmpS3176;
      int32_t _M0L6_2atmpS3173;
      int32_t _M0L6_2atmpS3175;
      int32_t _M0L6_2atmpS3174;
      int32_t _M0L7vrMod10S1193;
      int32_t _M0L6_2atmpS3172;
      if (_M0L7vpDiv10S1189 <= _M0L7vmDiv10S1190) {
        break;
      }
      _M0L6_2atmpS3177 = _M0Lm2vrS1145;
      _M0L7vrDiv10S1192 = _M0L6_2atmpS3177 / 10ull;
      _M0L6_2atmpS3176 = _M0Lm2vrS1145;
      _M0L6_2atmpS3173 = (int32_t)_M0L6_2atmpS3176;
      _M0L6_2atmpS3175 = (int32_t)_M0L7vrDiv10S1192;
      _M0L6_2atmpS3174 = 10 * _M0L6_2atmpS3175;
      _M0L7vrMod10S1193 = _M0L6_2atmpS3173 - _M0L6_2atmpS3174;
      _M0Lm7roundUpS1184 = _M0L7vrMod10S1193 >= 5;
      _M0Lm2vrS1145 = _M0L7vrDiv10S1192;
      _M0Lm2vpS1146 = _M0L7vpDiv10S1189;
      _M0Lm2vmS1147 = _M0L7vmDiv10S1190;
      _M0L6_2atmpS3172 = _M0Lm7removedS1169;
      _M0Lm7removedS1169 = _M0L6_2atmpS3172 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS3180 = _M0Lm2vrS1145;
    _M0L6_2atmpS3183 = _M0Lm2vrS1145;
    _M0L6_2atmpS3184 = _M0Lm2vmS1147;
    _M0L6_2atmpS3182
    = _M0L6_2atmpS3183 == _M0L6_2atmpS3184 || _M0Lm7roundUpS1184;
    #line 522 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3181 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS3182);
    _M0Lm6outputS1171 = _M0L6_2atmpS3180 + _M0L6_2atmpS3181;
  }
  _M0L6_2atmpS3188 = _M0Lm3e10S1148;
  _M0L6_2atmpS3189 = _M0Lm7removedS1169;
  _M0L3expS1194 = _M0L6_2atmpS3188 + _M0L6_2atmpS3189;
  _M0L6_2atmpS3187 = _M0Lm6outputS1171;
  _block_4239
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_4239)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _block_4239->$0 = _M0L6_2atmpS3187;
  _block_4239->$1 = _M0L3expS1194;
  return _block_4239;
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
  int32_t _M0L6_2atmpS3088;
  int32_t _M0L6_2atmpS3087;
  int32_t _M0L4baseS1116;
  int32_t _M0L5base2S1118;
  int32_t _M0L6offsetS1119;
  int32_t _M0L6_2atmpS3086;
  uint64_t _M0L4mul0S1120;
  int32_t _M0L6_2atmpS3085;
  int32_t _M0L6_2atmpS3084;
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
  int32_t _M0L6_2atmpS3082;
  int32_t _M0L6_2atmpS3083;
  int32_t _M0L5deltaS1131;
  uint64_t _M0L6_2atmpS3081;
  uint64_t _M0L6_2atmpS3073;
  int32_t _M0L6_2atmpS3080;
  uint32_t _M0L6_2atmpS3077;
  int32_t _M0L6_2atmpS3079;
  int32_t _M0L6_2atmpS3078;
  uint32_t _M0L6_2atmpS3076;
  uint32_t _M0L6_2atmpS3075;
  uint64_t _M0L6_2atmpS3074;
  uint64_t _M0L1aS1132;
  uint64_t _M0L6_2atmpS3072;
  uint64_t _M0L1bS1133;
  #line 239 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3088 = _M0L1iS1117 + 26;
  _M0L6_2atmpS3087 = _M0L6_2atmpS3088 - 1;
  _M0L4baseS1116 = _M0L6_2atmpS3087 / 26;
  _M0L5base2S1118 = _M0L4baseS1116 * 26;
  _M0L6offsetS1119 = _M0L5base2S1118 - _M0L1iS1117;
  _M0L6_2atmpS3086 = _M0L4baseS1116 * 2;
  #line 243 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1120
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS3086);
  _M0L6_2atmpS3085 = _M0L4baseS1116 * 2;
  _M0L6_2atmpS3084 = _M0L6_2atmpS3085 + 1;
  #line 244 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1121
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS3084);
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
    uint64_t _M0L6_2atmpS3071 = _M0Lm5high1S1130;
    _M0Lm5high1S1130 = _M0L6_2atmpS3071 + 1ull;
  }
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3082 = _M0FPB8pow5bits(_M0L5base2S1118);
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3083 = _M0FPB8pow5bits(_M0L1iS1117);
  _M0L5deltaS1131 = _M0L6_2atmpS3082 - _M0L6_2atmpS3083;
  #line 257 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3081
  = _M0FPB13shiftright128(_M0L7_2alow0S1127, _M0L3sumS1129, _M0L5deltaS1131);
  _M0L6_2atmpS3073 = _M0L6_2atmpS3081 + 1ull;
  _M0L6_2atmpS3080 = _M0L1iS1117 / 16;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3077
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS3080);
  _M0L6_2atmpS3079 = _M0L1iS1117 % 16;
  _M0L6_2atmpS3078 = _M0L6_2atmpS3079 << 1;
  _M0L6_2atmpS3076 = _M0L6_2atmpS3077 >> (_M0L6_2atmpS3078 & 31);
  _M0L6_2atmpS3075 = _M0L6_2atmpS3076 & 3u;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3074 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS3075);
  _M0L1aS1132 = _M0L6_2atmpS3073 + _M0L6_2atmpS3074;
  _M0L6_2atmpS3072 = _M0Lm5high1S1130;
  #line 260 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1133
  = _M0FPB13shiftright128(_M0L3sumS1129, _M0L6_2atmpS3072, _M0L5deltaS1131);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1132, .$1 = _M0L1bS1133};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS1099) {
  int32_t _M0L4baseS1098;
  int32_t _M0L5base2S1100;
  int32_t _M0L6offsetS1101;
  int32_t _M0L6_2atmpS3070;
  uint64_t _M0L4mul0S1102;
  int32_t _M0L6_2atmpS3069;
  int32_t _M0L6_2atmpS3068;
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
  int32_t _M0L6_2atmpS3066;
  int32_t _M0L6_2atmpS3067;
  int32_t _M0L5deltaS1113;
  uint64_t _M0L6_2atmpS3058;
  int32_t _M0L6_2atmpS3065;
  uint32_t _M0L6_2atmpS3062;
  int32_t _M0L6_2atmpS3064;
  int32_t _M0L6_2atmpS3063;
  uint32_t _M0L6_2atmpS3061;
  uint32_t _M0L6_2atmpS3060;
  uint64_t _M0L6_2atmpS3059;
  uint64_t _M0L1aS1114;
  uint64_t _M0L6_2atmpS3057;
  uint64_t _M0L1bS1115;
  #line 213 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4baseS1098 = _M0L1iS1099 / 26;
  _M0L5base2S1100 = _M0L4baseS1098 * 26;
  _M0L6offsetS1101 = _M0L1iS1099 - _M0L5base2S1100;
  _M0L6_2atmpS3070 = _M0L4baseS1098 * 2;
  #line 217 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1102
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS3070);
  _M0L6_2atmpS3069 = _M0L4baseS1098 * 2;
  _M0L6_2atmpS3068 = _M0L6_2atmpS3069 + 1;
  #line 218 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1103
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS3068);
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
    uint64_t _M0L6_2atmpS3056 = _M0Lm5high1S1112;
    _M0Lm5high1S1112 = _M0L6_2atmpS3056 + 1ull;
  }
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3066 = _M0FPB8pow5bits(_M0L1iS1099);
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3067 = _M0FPB8pow5bits(_M0L5base2S1100);
  _M0L5deltaS1113 = _M0L6_2atmpS3066 - _M0L6_2atmpS3067;
  #line 231 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3058
  = _M0FPB13shiftright128(_M0L7_2alow0S1109, _M0L3sumS1111, _M0L5deltaS1113);
  _M0L6_2atmpS3065 = _M0L1iS1099 / 16;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3062
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS3065);
  _M0L6_2atmpS3064 = _M0L1iS1099 % 16;
  _M0L6_2atmpS3063 = _M0L6_2atmpS3064 << 1;
  _M0L6_2atmpS3061 = _M0L6_2atmpS3062 >> (_M0L6_2atmpS3063 & 31);
  _M0L6_2atmpS3060 = _M0L6_2atmpS3061 & 3u;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3059 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS3060);
  _M0L1aS1114 = _M0L6_2atmpS3058 + _M0L6_2atmpS3059;
  _M0L6_2atmpS3057 = _M0Lm5high1S1112;
  #line 233 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1115
  = _M0FPB13shiftright128(_M0L3sumS1111, _M0L6_2atmpS3057, _M0L5deltaS1113);
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
  uint64_t _M0L6_2atmpS3055;
  uint64_t _M0L2hiS1080;
  uint64_t _M0L3lo2S1081;
  uint64_t _M0L6_2atmpS3053;
  uint64_t _M0L6_2atmpS3054;
  uint64_t _M0L4mid2S1082;
  uint64_t _M0L6_2atmpS3052;
  uint64_t _M0L3hi2S1083;
  int32_t _M0L6_2atmpS3051;
  int32_t _M0L6_2atmpS3050;
  uint64_t _M0L2vpS1084;
  uint64_t _M0Lm2vmS1086;
  int32_t _M0L6_2atmpS3049;
  int32_t _M0L6_2atmpS3048;
  uint64_t _M0L2vrS1097;
  uint64_t _M0L6_2atmpS3047;
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
    _M0L6_2atmpS3055 = 1ull;
  } else {
    _M0L6_2atmpS3055 = 0ull;
  }
  _M0L2hiS1080 = _M0L6_2ahi2S1078 + _M0L6_2atmpS3055;
  _M0L3lo2S1081 = _M0L5_2aloS1074 + _M0L7_2amul0S1068;
  _M0L6_2atmpS3053 = _M0L3midS1079 + _M0L7_2amul1S1070;
  if (_M0L3lo2S1081 < _M0L5_2aloS1074) {
    _M0L6_2atmpS3054 = 1ull;
  } else {
    _M0L6_2atmpS3054 = 0ull;
  }
  _M0L4mid2S1082 = _M0L6_2atmpS3053 + _M0L6_2atmpS3054;
  if (_M0L4mid2S1082 < _M0L3midS1079) {
    _M0L6_2atmpS3052 = 1ull;
  } else {
    _M0L6_2atmpS3052 = 0ull;
  }
  _M0L3hi2S1083 = _M0L2hiS1080 + _M0L6_2atmpS3052;
  _M0L6_2atmpS3051 = _M0L1jS1085 - 64;
  _M0L6_2atmpS3050 = _M0L6_2atmpS3051 - 1;
  #line 144 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vpS1084
  = _M0FPB13shiftright128(_M0L4mid2S1082, _M0L3hi2S1083, _M0L6_2atmpS3050);
  _M0Lm2vmS1086 = 0ull;
  if (_M0L7mmShiftS1087) {
    uint64_t _M0L3lo3S1088 = _M0L5_2aloS1074 - _M0L7_2amul0S1068;
    uint64_t _M0L6_2atmpS3037 = _M0L3midS1079 - _M0L7_2amul1S1070;
    uint64_t _M0L6_2atmpS3038;
    uint64_t _M0L4mid3S1089;
    uint64_t _M0L6_2atmpS3036;
    uint64_t _M0L3hi3S1090;
    int32_t _M0L6_2atmpS3035;
    int32_t _M0L6_2atmpS3034;
    if (_M0L5_2aloS1074 < _M0L3lo3S1088) {
      _M0L6_2atmpS3038 = 1ull;
    } else {
      _M0L6_2atmpS3038 = 0ull;
    }
    _M0L4mid3S1089 = _M0L6_2atmpS3037 - _M0L6_2atmpS3038;
    if (_M0L3midS1079 < _M0L4mid3S1089) {
      _M0L6_2atmpS3036 = 1ull;
    } else {
      _M0L6_2atmpS3036 = 0ull;
    }
    _M0L3hi3S1090 = _M0L2hiS1080 - _M0L6_2atmpS3036;
    _M0L6_2atmpS3035 = _M0L1jS1085 - 64;
    _M0L6_2atmpS3034 = _M0L6_2atmpS3035 - 1;
    #line 150 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1086
    = _M0FPB13shiftright128(_M0L4mid3S1089, _M0L3hi3S1090, _M0L6_2atmpS3034);
  } else {
    uint64_t _M0L3lo3S1091 = _M0L5_2aloS1074 + _M0L5_2aloS1074;
    uint64_t _M0L6_2atmpS3045 = _M0L3midS1079 + _M0L3midS1079;
    uint64_t _M0L6_2atmpS3046;
    uint64_t _M0L4mid3S1092;
    uint64_t _M0L6_2atmpS3043;
    uint64_t _M0L6_2atmpS3044;
    uint64_t _M0L3hi3S1093;
    uint64_t _M0L3lo4S1094;
    uint64_t _M0L6_2atmpS3041;
    uint64_t _M0L6_2atmpS3042;
    uint64_t _M0L4mid4S1095;
    uint64_t _M0L6_2atmpS3040;
    uint64_t _M0L3hi4S1096;
    int32_t _M0L6_2atmpS3039;
    if (_M0L3lo3S1091 < _M0L5_2aloS1074) {
      _M0L6_2atmpS3046 = 1ull;
    } else {
      _M0L6_2atmpS3046 = 0ull;
    }
    _M0L4mid3S1092 = _M0L6_2atmpS3045 + _M0L6_2atmpS3046;
    _M0L6_2atmpS3043 = _M0L2hiS1080 + _M0L2hiS1080;
    if (_M0L4mid3S1092 < _M0L3midS1079) {
      _M0L6_2atmpS3044 = 1ull;
    } else {
      _M0L6_2atmpS3044 = 0ull;
    }
    _M0L3hi3S1093 = _M0L6_2atmpS3043 + _M0L6_2atmpS3044;
    _M0L3lo4S1094 = _M0L3lo3S1091 - _M0L7_2amul0S1068;
    _M0L6_2atmpS3041 = _M0L4mid3S1092 - _M0L7_2amul1S1070;
    if (_M0L3lo3S1091 < _M0L3lo4S1094) {
      _M0L6_2atmpS3042 = 1ull;
    } else {
      _M0L6_2atmpS3042 = 0ull;
    }
    _M0L4mid4S1095 = _M0L6_2atmpS3041 - _M0L6_2atmpS3042;
    if (_M0L4mid3S1092 < _M0L4mid4S1095) {
      _M0L6_2atmpS3040 = 1ull;
    } else {
      _M0L6_2atmpS3040 = 0ull;
    }
    _M0L3hi4S1096 = _M0L3hi3S1093 - _M0L6_2atmpS3040;
    _M0L6_2atmpS3039 = _M0L1jS1085 - 64;
    #line 158 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1086
    = _M0FPB13shiftright128(_M0L4mid4S1095, _M0L3hi4S1096, _M0L6_2atmpS3039);
  }
  _M0L6_2atmpS3049 = _M0L1jS1085 - 64;
  _M0L6_2atmpS3048 = _M0L6_2atmpS3049 - 1;
  #line 160 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vrS1097
  = _M0FPB13shiftright128(_M0L3midS1079, _M0L2hiS1080, _M0L6_2atmpS3048);
  _M0L6_2atmpS3047 = _M0Lm2vmS1086;
  return (struct _M0TPB19MulShiftAll64Result){.$0 = _M0L2vrS1097,
                                                .$1 = _M0L2vpS1084,
                                                .$2 = _M0L6_2atmpS3047};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS1066,
  int32_t _M0L1pS1067
) {
  uint64_t _M0L6_2atmpS3033;
  uint64_t _M0L6_2atmpS3032;
  uint64_t _M0L6_2atmpS3031;
  #line 124 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3033 = 1ull << (_M0L1pS1067 & 63);
  _M0L6_2atmpS3032 = _M0L6_2atmpS3033 - 1ull;
  _M0L6_2atmpS3031 = _M0L5valueS1066 & _M0L6_2atmpS3032;
  return _M0L6_2atmpS3031 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS1064,
  int32_t _M0L1pS1065
) {
  int32_t _M0L6_2atmpS3030;
  #line 119 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 120 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3030 = _M0FPB10pow5Factor(_M0L5valueS1064);
  return _M0L6_2atmpS3030 >= _M0L1pS1065;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS1059) {
  uint64_t _M0L6_2atmpS3021;
  uint64_t _M0L6_2atmpS3022;
  uint64_t _M0L6_2atmpS3023;
  uint64_t _M0L6_2atmpS3024;
  uint64_t _M0L6_2atmpS3029;
  int32_t _M0L5countS1060;
  uint64_t _M0L1vS1061;
  #line 94 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3021 = _M0L5valueS1059 % 5ull;
  if (_M0L6_2atmpS3021 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3022 = _M0L5valueS1059 % 25ull;
  if (_M0L6_2atmpS3022 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS3023 = _M0L5valueS1059 % 125ull;
  if (_M0L6_2atmpS3023 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS3024 = _M0L5valueS1059 % 625ull;
  if (_M0L6_2atmpS3024 != 0ull) {
    return 3;
  }
  _M0L6_2atmpS3029 = _M0L5valueS1059 / 625ull;
  _M0L5countS1060 = 4;
  _M0L1vS1061 = _M0L6_2atmpS3029;
  while (1) {
    if (_M0L1vS1061 > 0ull) {
      uint64_t _M0L6_2atmpS3025 = _M0L1vS1061 % 5ull;
      int32_t _M0L6_2atmpS3026;
      uint64_t _M0L6_2atmpS3027;
      if (_M0L6_2atmpS3025 != 0ull) {
        return _M0L5countS1060;
      }
      _M0L6_2atmpS3026 = _M0L5countS1060 + 1;
      _M0L6_2atmpS3027 = _M0L1vS1061 / 5ull;
      _M0L5countS1060 = _M0L6_2atmpS3026;
      _M0L1vS1061 = _M0L6_2atmpS3027;
      continue;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1063;
      moonbit_string_t _M0L6_2atmpS3028;
      int32_t _result_4241;
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L18_2astring__builderS1063
      = _M0MPB13StringBuilder21StringBuilder_2einner(25);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1063, (moonbit_string_t)moonbit_string_literal_102.data);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0MPB13StringBuilder13write__objectGmE(_M0L18_2astring__builderS1063, _M0L5valueS1059);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L6_2atmpS3028
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1063);
      moonbit_decref(_M0L18_2astring__builderS1063);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _result_4241 = _M0FPC15abort5abortGiE(_M0L6_2atmpS3028);
      moonbit_decref(_M0L6_2atmpS3028);
      return _result_4241;
    }
    break;
  }
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS1058,
  uint64_t _M0L2hiS1056,
  int32_t _M0L4distS1057
) {
  int32_t _M0L6_2atmpS3020;
  uint64_t _M0L6_2atmpS3018;
  uint64_t _M0L6_2atmpS3019;
  #line 89 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3020 = 64 - _M0L4distS1057;
  _M0L6_2atmpS3018 = _M0L2hiS1056 << (_M0L6_2atmpS3020 & 63);
  _M0L6_2atmpS3019 = _M0L2loS1058 >> (_M0L4distS1057 & 63);
  return _M0L6_2atmpS3018 | _M0L6_2atmpS3019;
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
  uint64_t _M0L6_2atmpS3016;
  uint64_t _M0L6_2atmpS3017;
  uint64_t _M0L1yS1052;
  uint64_t _M0L6_2atmpS3014;
  uint64_t _M0L6_2atmpS3015;
  uint64_t _M0L1zS1053;
  uint64_t _M0L6_2atmpS3012;
  uint64_t _M0L6_2atmpS3013;
  uint64_t _M0L6_2atmpS3010;
  uint64_t _M0L6_2atmpS3011;
  uint64_t _M0L1wS1054;
  uint64_t _M0L2loS1055;
  #line 74 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3aLoS1045 = _M0L1aS1046 & 4294967295ull;
  _M0L3aHiS1047 = _M0L1aS1046 >> 32;
  _M0L3bLoS1048 = _M0L1bS1049 & 4294967295ull;
  _M0L3bHiS1050 = _M0L1bS1049 >> 32;
  _M0L1xS1051 = _M0L3aLoS1045 * _M0L3bLoS1048;
  _M0L6_2atmpS3016 = _M0L3aHiS1047 * _M0L3bLoS1048;
  _M0L6_2atmpS3017 = _M0L1xS1051 >> 32;
  _M0L1yS1052 = _M0L6_2atmpS3016 + _M0L6_2atmpS3017;
  _M0L6_2atmpS3014 = _M0L3aLoS1045 * _M0L3bHiS1050;
  _M0L6_2atmpS3015 = _M0L1yS1052 & 4294967295ull;
  _M0L1zS1053 = _M0L6_2atmpS3014 + _M0L6_2atmpS3015;
  _M0L6_2atmpS3012 = _M0L3aHiS1047 * _M0L3bHiS1050;
  _M0L6_2atmpS3013 = _M0L1yS1052 >> 32;
  _M0L6_2atmpS3010 = _M0L6_2atmpS3012 + _M0L6_2atmpS3013;
  _M0L6_2atmpS3011 = _M0L1zS1053 >> 32;
  _M0L1wS1054 = _M0L6_2atmpS3010 + _M0L6_2atmpS3011;
  _M0L2loS1055 = _M0L1aS1046 * _M0L1bS1049;
  return (struct _M0TPB7Umul128){.$0 = _M0L2loS1055, .$1 = _M0L1wS1054};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS1043,
  int32_t _M0L4fromS1040,
  int32_t _M0L2toS1039
) {
  int32_t _M0L3lenS1038;
  int32_t _M0L6_2atmpS3009;
  uint16_t* _M0L6bufferS1041;
  int32_t _M0L1iS1042;
  #line 52 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3lenS1038 = _M0L2toS1039 - _M0L4fromS1040;
  #line 54 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3009 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L6bufferS1041
  = (uint16_t*)moonbit_make_string(_M0L3lenS1038, _M0L6_2atmpS3009);
  _M0L1iS1042 = 0;
  while (1) {
    if (_M0L1iS1042 < _M0L3lenS1038) {
      int32_t _M0L6_2atmpS3007 = _M0L4fromS1040 + _M0L1iS1042;
      int32_t _M0L6_2atmpS3006;
      int32_t _M0L6_2atmpS3005;
      int32_t _M0L6_2atmpS3008;
      if (
        _M0L6_2atmpS3007 < 0
        || _M0L6_2atmpS3007 >= Moonbit_array_length(_M0L5bytesS1043)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3006 = (int32_t)_M0L5bytesS1043[_M0L6_2atmpS3007];
      _M0L6_2atmpS3005 = (uint16_t)_M0L6_2atmpS3006;
      if (
        _M0L1iS1042 < 0
        || _M0L1iS1042 >= Moonbit_array_length(_M0L6bufferS1041)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6bufferS1041[_M0L1iS1042] = _M0L6_2atmpS3005;
      _M0L6_2atmpS3008 = _M0L1iS1042 + 1;
      _M0L1iS1042 = _M0L6_2atmpS3008;
      continue;
    }
    break;
  }
  return _M0L6bufferS1041;
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS1037) {
  int32_t _M0L6_2atmpS3004;
  uint32_t _M0L6_2atmpS3003;
  uint32_t _M0L6_2atmpS3002;
  #line 44 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3004 = _M0L1eS1037 * 78913;
  _M0L6_2atmpS3003 = *(uint32_t*)&_M0L6_2atmpS3004;
  _M0L6_2atmpS3002 = _M0L6_2atmpS3003 >> 18;
  return *(int32_t*)&_M0L6_2atmpS3002;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS1036) {
  int32_t _M0L6_2atmpS3001;
  uint32_t _M0L6_2atmpS3000;
  uint32_t _M0L6_2atmpS2999;
  #line 37 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3001 = _M0L1eS1036 * 732923;
  _M0L6_2atmpS3000 = *(uint32_t*)&_M0L6_2atmpS3001;
  _M0L6_2atmpS2999 = _M0L6_2atmpS3000 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2999;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS1034,
  int32_t _M0L8exponentS1035,
  int32_t _M0L8mantissaS1032
) {
  moonbit_string_t _M0L1sS1033;
  moonbit_string_t _result_4244;
  #line 23 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L8mantissaS1032) {
    return (moonbit_string_t)moonbit_string_literal_103.data;
  }
  if (_M0L4signS1034) {
    _M0L1sS1033 = (moonbit_string_t)moonbit_string_literal_94.data;
  } else {
    _M0L1sS1033 = (moonbit_string_t)moonbit_string_literal_95.data;
  }
  if (_M0L8exponentS1035) {
    moonbit_string_t _result_4243;
    #line 29 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4243
    = moonbit_add_string(_M0L1sS1033, (moonbit_string_t)moonbit_string_literal_104.data);
    moonbit_decref(_M0L1sS1033);
    return _result_4243;
  }
  #line 31 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_4244
  = moonbit_add_string(_M0L1sS1033, (moonbit_string_t)moonbit_string_literal_105.data);
  moonbit_decref(_M0L1sS1033);
  return _result_4244;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS1031) {
  int32_t _M0L6_2atmpS2998;
  uint32_t _M0L6_2atmpS2997;
  uint32_t _M0L6_2atmpS2996;
  int32_t _M0L6_2atmpS2995;
  #line 18 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS2998 = _M0L1eS1031 * 1217359;
  _M0L6_2atmpS2997 = *(uint32_t*)&_M0L6_2atmpS2998;
  _M0L6_2atmpS2996 = _M0L6_2atmpS2997 >> 19;
  _M0L6_2atmpS2995 = *(int32_t*)&_M0L6_2atmpS2996;
  return _M0L6_2atmpS2995 + 1;
}

int32_t _M0IPC16string6StringPB4Hash4hash(moonbit_string_t _M0L4selfS1027) {
  int32_t _tmp_4245;
  uint32_t _M0L6_2atmpS2994;
  uint32_t _M0Lm3accS1025;
  int32_t _M0L7_2abindS1026;
  int32_t _M0L1iS1028;
  uint32_t _M0L6_2atmpS2993;
  #line 522 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _tmp_4245 = 0;
  _M0L6_2atmpS2994 = *(uint32_t*)&_tmp_4245;
  _M0Lm3accS1025 = _M0L6_2atmpS2994 + 374761393u;
  _M0L7_2abindS1026 = Moonbit_array_length(_M0L4selfS1027);
  _M0L1iS1028 = 0;
  while (1) {
    if (_M0L1iS1028 < _M0L7_2abindS1026) {
      uint32_t _M0L6_2atmpS2988 = _M0Lm3accS1025;
      int32_t _M0L6_2atmpS2991;
      int32_t _M0L6_2atmpS2990;
      uint32_t _M0L1vS1029;
      uint32_t _M0L6_2atmpS2989;
      int32_t _M0L6_2atmpS2992;
      _M0Lm3accS1025 = _M0L6_2atmpS2988 + 4u;
      _M0L6_2atmpS2991 = _M0L4selfS1027[_M0L1iS1028];
      _M0L6_2atmpS2990 = (int32_t)_M0L6_2atmpS2991;
      _M0L1vS1029 = *(uint32_t*)&_M0L6_2atmpS2990;
      _M0L6_2atmpS2989 = _M0Lm3accS1025;
      #line 527 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
      _M0Lm3accS1025 = _M0FPB13consume4__acc(_M0L6_2atmpS2989, _M0L1vS1029);
      _M0L6_2atmpS2992 = _M0L1iS1028 + 1;
      _M0L1iS1028 = _M0L6_2atmpS2992;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2993 = _M0Lm3accS1025;
  #line 529 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  return _M0FPB13finalize__acc(_M0L6_2atmpS2993);
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

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L4selfS1023
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsRP19moonbitDB10RedisValueEE(_M0L4selfS1023);
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

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS1019
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsRP19moonbitDB10RedisValueE(_M0L4selfS1019);
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
  struct _M0TPB5EntryGssE* _M0L4headS2957;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS973;
  int32_t _M0L3lenS975;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS976;
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2950__l711__* _closure_4247;
  struct _M0TWEOUssE* _M0L6_2atmpS2948;
  int64_t _M0L6_2atmpS2949;
  struct _M0TPB4IterGUssEE* _result_4248;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2957 = _M0L4selfS974->$5;
  if (_M0L4headS2957) {
    moonbit_incref(_M0L4headS2957);
  }
  _M0L11curr__entryS973
  = (struct _M0TPB8MutLocalGORPB5EntryGssEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGssEE));
  Moonbit_object_header(_M0L11curr__entryS973)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 35, 0);
  _M0L11curr__entryS973->$0 = _M0L4headS2957;
  _M0L3lenS975 = _M0L4selfS974->$1;
  _M0L9remainingS976
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS976)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS976->$0 = _M0L3lenS975;
  _closure_4247
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2950__l711__*)moonbit_malloc(sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2950__l711__));
  Moonbit_object_header(_closure_4247)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 38, 0);
  _closure_4247->code = &_M0MPB3Map4iterGssEC2950l711;
  _closure_4247->$0 = _M0L9remainingS976;
  _closure_4247->$1 = _M0L11curr__entryS973;
  _M0L6_2atmpS2948 = (struct _M0TWEOUssE*)_closure_4247;
  _M0L6_2atmpS2949 = (int64_t)_M0L3lenS975;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4248 = _M0MPB4Iter3newGUssEE(_M0L6_2atmpS2948, _M0L6_2atmpS2949);
  moonbit_decref(_M0L6_2atmpS2948);
  return _result_4248;
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS985
) {
  struct _M0TPB5EntryGsbE* _M0L4headS2967;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS984;
  int32_t _M0L3lenS986;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS987;
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2960__l711__* _closure_4249;
  struct _M0TWEOUsbE* _M0L6_2atmpS2958;
  int64_t _M0L6_2atmpS2959;
  struct _M0TPB4IterGUsbEE* _result_4250;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2967 = _M0L4selfS985->$5;
  if (_M0L4headS2967) {
    moonbit_incref(_M0L4headS2967);
  }
  _M0L11curr__entryS984
  = (struct _M0TPB8MutLocalGORPB5EntryGsbEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE));
  Moonbit_object_header(_M0L11curr__entryS984)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 42, 0);
  _M0L11curr__entryS984->$0 = _M0L4headS2967;
  _M0L3lenS986 = _M0L4selfS985->$1;
  _M0L9remainingS987
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS987)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS987->$0 = _M0L3lenS986;
  _closure_4249
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2960__l711__*)moonbit_malloc(sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2960__l711__));
  Moonbit_object_header(_closure_4249)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 45, 0);
  _closure_4249->code = &_M0MPB3Map4iterGsbEC2960l711;
  _closure_4249->$0 = _M0L9remainingS987;
  _closure_4249->$1 = _M0L11curr__entryS984;
  _M0L6_2atmpS2958 = (struct _M0TWEOUsbE*)_closure_4249;
  _M0L6_2atmpS2959 = (int64_t)_M0L3lenS986;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4250 = _M0MPB4Iter3newGUsbEE(_M0L6_2atmpS2958, _M0L6_2atmpS2959);
  moonbit_decref(_M0L6_2atmpS2958);
  return _result_4250;
}

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB3Map4iterGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS996
) {
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4headS2977;
  struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE* _M0L11curr__entryS995;
  int32_t _M0L3lenS997;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS998;
  struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u2970__l711__* _closure_4251;
  struct _M0TWEOUsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2968;
  int64_t _M0L6_2atmpS2969;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _result_4252;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2977 = _M0L4selfS996->$5;
  if (_M0L4headS2977) {
    moonbit_incref(_M0L4headS2977);
  }
  _M0L11curr__entryS995
  = (struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE));
  Moonbit_object_header(_M0L11curr__entryS995)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 49, 0);
  _M0L11curr__entryS995->$0 = _M0L4headS2977;
  _M0L3lenS997 = _M0L4selfS996->$1;
  _M0L9remainingS998
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS998)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS998->$0 = _M0L3lenS997;
  _closure_4251
  = (struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u2970__l711__*)moonbit_malloc(sizeof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u2970__l711__));
  Moonbit_object_header(_closure_4251)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 52, 0);
  _closure_4251->code = &_M0MPB3Map4iterGsRP19moonbitDB10RedisValueEC2970l711;
  _closure_4251->$0 = _M0L9remainingS998;
  _closure_4251->$1 = _M0L11curr__entryS995;
  _M0L6_2atmpS2968
  = (struct _M0TWEOUsRP19moonbitDB10RedisValueE*)_closure_4251;
  _M0L6_2atmpS2969 = (int64_t)_M0L3lenS997;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4252
  = _M0MPB4Iter3newGUsRP19moonbitDB10RedisValueEE(_M0L6_2atmpS2968, _M0L6_2atmpS2969);
  moonbit_decref(_M0L6_2atmpS2968);
  return _result_4252;
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1007
) {
  struct _M0TPB5EntryGsfE* _M0L4headS2987;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS1006;
  int32_t _M0L3lenS1008;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1009;
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2980__l711__* _closure_4253;
  struct _M0TWEOUsfE* _M0L6_2atmpS2978;
  int64_t _M0L6_2atmpS2979;
  struct _M0TPB4IterGUsfEE* _result_4254;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS2987 = _M0L4selfS1007->$5;
  if (_M0L4headS2987) {
    moonbit_incref(_M0L4headS2987);
  }
  _M0L11curr__entryS1006
  = (struct _M0TPB8MutLocalGORPB5EntryGsfEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE));
  Moonbit_object_header(_M0L11curr__entryS1006)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 56, 0);
  _M0L11curr__entryS1006->$0 = _M0L4headS2987;
  _M0L3lenS1008 = _M0L4selfS1007->$1;
  _M0L9remainingS1009
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1009)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1009->$0 = _M0L3lenS1008;
  _closure_4253
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2980__l711__*)moonbit_malloc(sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2980__l711__));
  Moonbit_object_header(_closure_4253)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 59, 0);
  _closure_4253->code = &_M0MPB3Map4iterGsfEC2980l711;
  _closure_4253->$0 = _M0L9remainingS1009;
  _closure_4253->$1 = _M0L11curr__entryS1006;
  _M0L6_2atmpS2978 = (struct _M0TWEOUsfE*)_closure_4253;
  _M0L6_2atmpS2979 = (int64_t)_M0L3lenS1008;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4254 = _M0MPB4Iter3newGUsfEE(_M0L6_2atmpS2978, _M0L6_2atmpS2979);
  moonbit_decref(_M0L6_2atmpS2978);
  return _result_4254;
}

struct _M0TUsfE* _M0MPB3Map4iterGsfEC2980l711(
  struct _M0TWEOUsfE* _M0L6_2aenvS2981
) {
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2980__l711__* _M0L14_2acasted__envS2982;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS1006;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1009;
  int32_t _M0L3valS2983;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2982
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u2980__l711__*)_M0L6_2aenvS2981;
  _M0L11curr__entryS1006 = _M0L14_2acasted__envS2982->$1;
  _M0L9remainingS1009 = _M0L14_2acasted__envS2982->$0;
  _M0L3valS2983 = _M0L9remainingS1009->$0;
  if (_M0L3valS2983 > 0) {
    struct _M0TPB5EntryGsfE* _M0L7_2abindS1011 = _M0L11curr__entryS1006->$0;
    if (_M0L7_2abindS1011 == 0) {
      goto join_1010;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS1012 = _M0L7_2abindS1011;
      struct _M0TPB5EntryGsfE* _M0L4_2axS1013 = _M0L7_2aSomeS1012;
      moonbit_string_t _M0L6_2akeyS1014 = _M0L4_2axS1013->$4;
      float _M0L8_2avalueS1015 = _M0L4_2axS1013->$5;
      struct _M0TPB5EntryGsfE* _M0L7_2anextS1016 = _M0L4_2axS1013->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3763 = _M0L11curr__entryS1006->$0;
      int32_t _M0L3valS2985;
      int32_t _M0L6_2atmpS2984;
      struct _M0TUsfE* _M0L8_2atupleS2986;
      if (_M0L7_2anextS1016) {
        moonbit_incref(_M0L7_2anextS1016);
      }
      moonbit_incref(_M0L6_2akeyS1014);
      if (_M0L6_2aoldS3763) {
        moonbit_decref(_M0L6_2aoldS3763);
      }
      _M0L11curr__entryS1006->$0 = _M0L7_2anextS1016;
      _M0L3valS2985 = _M0L9remainingS1009->$0;
      _M0L6_2atmpS2984 = _M0L3valS2985 - 1;
      _M0L9remainingS1009->$0 = _M0L6_2atmpS2984;
      _M0L8_2atupleS2986
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS2986)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 6, 0);
      _M0L8_2atupleS2986->$0 = _M0L6_2akeyS1014;
      _M0L8_2atupleS2986->$1 = _M0L8_2avalueS1015;
      return _M0L8_2atupleS2986;
    }
  } else {
    goto join_1010;
  }
  join_1010:;
  return 0;
}

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB3Map4iterGsRP19moonbitDB10RedisValueEC2970l711(
  struct _M0TWEOUsRP19moonbitDB10RedisValueE* _M0L6_2aenvS2971
) {
  struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u2970__l711__* _M0L14_2acasted__envS2972;
  struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE* _M0L11curr__entryS995;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS998;
  int32_t _M0L3valS2973;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2972
  = (struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u2970__l711__*)_M0L6_2aenvS2971;
  _M0L11curr__entryS995 = _M0L14_2acasted__envS2972->$1;
  _M0L9remainingS998 = _M0L14_2acasted__envS2972->$0;
  _M0L3valS2973 = _M0L9remainingS998->$0;
  if (_M0L3valS2973 > 0) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS1000 =
      _M0L11curr__entryS995->$0;
    if (_M0L7_2abindS1000 == 0) {
      goto join_999;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1001 =
        _M0L7_2abindS1000;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4_2axS1002 =
        _M0L7_2aSomeS1001;
      moonbit_string_t _M0L6_2akeyS1003 = _M0L4_2axS1002->$4;
      void* _M0L8_2avalueS1004 = _M0L4_2axS1002->$5;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2anextS1005 =
        _M0L4_2axS1002->$1;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS3767 =
        _M0L11curr__entryS995->$0;
      int32_t _M0L3valS2975;
      int32_t _M0L6_2atmpS2974;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L8_2atupleS2976;
      if (_M0L7_2anextS1005) {
        moonbit_incref(_M0L7_2anextS1005);
      }
      moonbit_incref(_M0L8_2avalueS1004);
      moonbit_incref(_M0L6_2akeyS1003);
      if (_M0L6_2aoldS3767) {
        moonbit_decref(_M0L6_2aoldS3767);
      }
      _M0L11curr__entryS995->$0 = _M0L7_2anextS1005;
      _M0L3valS2975 = _M0L9remainingS998->$0;
      _M0L6_2atmpS2974 = _M0L3valS2975 - 1;
      _M0L9remainingS998->$0 = _M0L6_2atmpS2974;
      _M0L8_2atupleS2976
      = (struct _M0TUsRP19moonbitDB10RedisValueE*)moonbit_malloc(sizeof(struct _M0TUsRP19moonbitDB10RedisValueE));
      Moonbit_object_header(_M0L8_2atupleS2976)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 63, 0);
      _M0L8_2atupleS2976->$0 = _M0L6_2akeyS1003;
      _M0L8_2atupleS2976->$1 = _M0L8_2avalueS1004;
      return _M0L8_2atupleS2976;
    }
  } else {
    goto join_999;
  }
  join_999:;
  return 0;
}

struct _M0TUsbE* _M0MPB3Map4iterGsbEC2960l711(
  struct _M0TWEOUsbE* _M0L6_2aenvS2961
) {
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2960__l711__* _M0L14_2acasted__envS2962;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS984;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS987;
  int32_t _M0L3valS2963;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2962
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u2960__l711__*)_M0L6_2aenvS2961;
  _M0L11curr__entryS984 = _M0L14_2acasted__envS2962->$1;
  _M0L9remainingS987 = _M0L14_2acasted__envS2962->$0;
  _M0L3valS2963 = _M0L9remainingS987->$0;
  if (_M0L3valS2963 > 0) {
    struct _M0TPB5EntryGsbE* _M0L7_2abindS989 = _M0L11curr__entryS984->$0;
    if (_M0L7_2abindS989 == 0) {
      goto join_988;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS990 = _M0L7_2abindS989;
      struct _M0TPB5EntryGsbE* _M0L4_2axS991 = _M0L7_2aSomeS990;
      moonbit_string_t _M0L6_2akeyS992 = _M0L4_2axS991->$4;
      int32_t _M0L8_2avalueS993 = _M0L4_2axS991->$5;
      struct _M0TPB5EntryGsbE* _M0L7_2anextS994 = _M0L4_2axS991->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3772 = _M0L11curr__entryS984->$0;
      int32_t _M0L3valS2965;
      int32_t _M0L6_2atmpS2964;
      struct _M0TUsbE* _M0L8_2atupleS2966;
      if (_M0L7_2anextS994) {
        moonbit_incref(_M0L7_2anextS994);
      }
      moonbit_incref(_M0L6_2akeyS992);
      if (_M0L6_2aoldS3772) {
        moonbit_decref(_M0L6_2aoldS3772);
      }
      _M0L11curr__entryS984->$0 = _M0L7_2anextS994;
      _M0L3valS2965 = _M0L9remainingS987->$0;
      _M0L6_2atmpS2964 = _M0L3valS2965 - 1;
      _M0L9remainingS987->$0 = _M0L6_2atmpS2964;
      _M0L8_2atupleS2966
      = (struct _M0TUsbE*)moonbit_malloc(sizeof(struct _M0TUsbE));
      Moonbit_object_header(_M0L8_2atupleS2966)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 67, 0);
      _M0L8_2atupleS2966->$0 = _M0L6_2akeyS992;
      _M0L8_2atupleS2966->$1 = _M0L8_2avalueS993;
      return _M0L8_2atupleS2966;
    }
  } else {
    goto join_988;
  }
  join_988:;
  return 0;
}

struct _M0TUssE* _M0MPB3Map4iterGssEC2950l711(
  struct _M0TWEOUssE* _M0L6_2aenvS2951
) {
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2950__l711__* _M0L14_2acasted__envS2952;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS973;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS976;
  int32_t _M0L3valS2953;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS2952
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u2950__l711__*)_M0L6_2aenvS2951;
  _M0L11curr__entryS973 = _M0L14_2acasted__envS2952->$1;
  _M0L9remainingS976 = _M0L14_2acasted__envS2952->$0;
  _M0L3valS2953 = _M0L9remainingS976->$0;
  if (_M0L3valS2953 > 0) {
    struct _M0TPB5EntryGssE* _M0L7_2abindS978 = _M0L11curr__entryS973->$0;
    if (_M0L7_2abindS978 == 0) {
      goto join_977;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS979 = _M0L7_2abindS978;
      struct _M0TPB5EntryGssE* _M0L4_2axS980 = _M0L7_2aSomeS979;
      moonbit_string_t _M0L6_2akeyS981 = _M0L4_2axS980->$4;
      moonbit_string_t _M0L8_2avalueS982 = _M0L4_2axS980->$5;
      struct _M0TPB5EntryGssE* _M0L7_2anextS983 = _M0L4_2axS980->$1;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3776 = _M0L11curr__entryS973->$0;
      int32_t _M0L3valS2955;
      int32_t _M0L6_2atmpS2954;
      struct _M0TUssE* _M0L8_2atupleS2956;
      if (_M0L7_2anextS983) {
        moonbit_incref(_M0L7_2anextS983);
      }
      moonbit_incref(_M0L8_2avalueS982);
      moonbit_incref(_M0L6_2akeyS981);
      if (_M0L6_2aoldS3776) {
        moonbit_decref(_M0L6_2aoldS3776);
      }
      _M0L11curr__entryS973->$0 = _M0L7_2anextS983;
      _M0L3valS2955 = _M0L9remainingS976->$0;
      _M0L6_2atmpS2954 = _M0L3valS2955 - 1;
      _M0L9remainingS976->$0 = _M0L6_2atmpS2954;
      _M0L8_2atupleS2956
      = (struct _M0TUssE*)moonbit_malloc(sizeof(struct _M0TUssE));
      Moonbit_object_header(_M0L8_2atupleS2956)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 70, 0);
      _M0L8_2atupleS2956->$0 = _M0L6_2akeyS981;
      _M0L8_2atupleS2956->$1 = _M0L8_2avalueS982;
      return _M0L8_2atupleS2956;
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
  int32_t _M0L6_2atmpS2946;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2946 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS967);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsiE(_M0L4selfS966, _M0L3keyS967, _M0L6_2atmpS2946);
  return 0;
}

int32_t _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS968,
  moonbit_string_t _M0L3keyS969
) {
  int32_t _M0L6_2atmpS2947;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2947 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS969);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsRP19moonbitDB10RedisValueE(_M0L4selfS968, _M0L3keyS969, _M0L6_2atmpS2947);
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS951,
  moonbit_string_t _M0L3keyS955,
  int32_t _M0L4hashS954
) {
  int32_t _M0L14capacity__maskS2933;
  int32_t _M0L6_2atmpS2932;
  int32_t _M0L1iS948;
  int32_t _M0L3idxS949;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2933 = _M0L4selfS951->$3;
  _M0L6_2atmpS2932 = _M0L4hashS954 & _M0L14capacity__maskS2933;
  _M0L1iS948 = 0;
  _M0L3idxS949 = _M0L6_2atmpS2932;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2931 = _M0L4selfS951->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS950;
    if (
      _M0L3idxS949 < 0
      || _M0L3idxS949 >= Moonbit_array_length(_M0L7entriesS2931)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS950
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2931[_M0L3idxS949];
    if (_M0L7_2abindS950 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS952 = _M0L7_2abindS950;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS953 = _M0L7_2aSomeS952;
      int32_t _M0L4hashS2923 = _M0L8_2aentryS953->$3;
      int32_t _if__result_4260;
      int32_t _M0L3pslS2926;
      int32_t _M0L6_2atmpS2927;
      int32_t _M0L6_2atmpS2929;
      int32_t _M0L14capacity__maskS2930;
      int32_t _M0L6_2atmpS2928;
      if (_M0L4hashS2923 == _M0L4hashS954) {
        moonbit_string_t _M0L3keyS2922 = _M0L8_2aentryS953->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4260
        = _M0L3keyS2922 == _M0L3keyS955
          || Moonbit_array_length(_M0L3keyS2922)
             == Moonbit_array_length(_M0L3keyS955)
             && 0
                == memcmp(_M0L3keyS2922, _M0L3keyS955, Moonbit_array_length(_M0L3keyS2922) * 2);
      } else {
        _if__result_4260 = 0;
      }
      if (_if__result_4260) {
        int32_t _M0L4sizeS2925;
        int32_t _M0L6_2atmpS2924;
        moonbit_incref(_M0L8_2aentryS953);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsiE(_M0L4selfS951, _M0L8_2aentryS953);
        moonbit_decref(_M0L8_2aentryS953);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsiE(_M0L4selfS951, _M0L3idxS949);
        _M0L4sizeS2925 = _M0L4selfS951->$1;
        _M0L6_2atmpS2924 = _M0L4sizeS2925 - 1;
        _M0L4selfS951->$1 = _M0L6_2atmpS2924;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS953);
      }
      _M0L3pslS2926 = _M0L8_2aentryS953->$2;
      moonbit_decref(_M0L8_2aentryS953);
      if (_M0L1iS948 > _M0L3pslS2926) {
        break;
      }
      _M0L6_2atmpS2927 = _M0L1iS948 + 1;
      _M0L6_2atmpS2929 = _M0L3idxS949 + 1;
      _M0L14capacity__maskS2930 = _M0L4selfS951->$3;
      _M0L6_2atmpS2928 = _M0L6_2atmpS2929 & _M0L14capacity__maskS2930;
      _M0L1iS948 = _M0L6_2atmpS2927;
      _M0L3idxS949 = _M0L6_2atmpS2928;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS960,
  moonbit_string_t _M0L3keyS964,
  int32_t _M0L4hashS963
) {
  int32_t _M0L14capacity__maskS2945;
  int32_t _M0L6_2atmpS2944;
  int32_t _M0L1iS957;
  int32_t _M0L3idxS958;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2945 = _M0L4selfS960->$3;
  _M0L6_2atmpS2944 = _M0L4hashS963 & _M0L14capacity__maskS2945;
  _M0L1iS957 = 0;
  _M0L3idxS958 = _M0L6_2atmpS2944;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2943 =
      _M0L4selfS960->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS959;
    if (
      _M0L3idxS958 < 0
      || _M0L3idxS958 >= Moonbit_array_length(_M0L7entriesS2943)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS959
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2943[
        _M0L3idxS958
      ];
    if (_M0L7_2abindS959 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS961 =
        _M0L7_2abindS959;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L8_2aentryS962 =
        _M0L7_2aSomeS961;
      int32_t _M0L4hashS2935 = _M0L8_2aentryS962->$3;
      int32_t _if__result_4262;
      int32_t _M0L3pslS2938;
      int32_t _M0L6_2atmpS2939;
      int32_t _M0L6_2atmpS2941;
      int32_t _M0L14capacity__maskS2942;
      int32_t _M0L6_2atmpS2940;
      if (_M0L4hashS2935 == _M0L4hashS963) {
        moonbit_string_t _M0L3keyS2934 = _M0L8_2aentryS962->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4262
        = _M0L3keyS2934 == _M0L3keyS964
          || Moonbit_array_length(_M0L3keyS2934)
             == Moonbit_array_length(_M0L3keyS964)
             && 0
                == memcmp(_M0L3keyS2934, _M0L3keyS964, Moonbit_array_length(_M0L3keyS2934) * 2);
      } else {
        _if__result_4262 = 0;
      }
      if (_if__result_4262) {
        int32_t _M0L4sizeS2937;
        int32_t _M0L6_2atmpS2936;
        moonbit_incref(_M0L8_2aentryS962);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS960, _M0L8_2aentryS962);
        moonbit_decref(_M0L8_2aentryS962);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsRP19moonbitDB10RedisValueE(_M0L4selfS960, _M0L3idxS958);
        _M0L4sizeS2937 = _M0L4selfS960->$1;
        _M0L6_2atmpS2936 = _M0L4sizeS2937 - 1;
        _M0L4selfS960->$1 = _M0L6_2atmpS2936;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS962);
      }
      _M0L3pslS2938 = _M0L8_2aentryS962->$2;
      moonbit_decref(_M0L8_2aentryS962);
      if (_M0L1iS957 > _M0L3pslS2938) {
        break;
      }
      _M0L6_2atmpS2939 = _M0L1iS957 + 1;
      _M0L6_2atmpS2941 = _M0L3idxS958 + 1;
      _M0L14capacity__maskS2942 = _M0L4selfS960->$3;
      _M0L6_2atmpS2940 = _M0L6_2atmpS2941 & _M0L14capacity__maskS2942;
      _M0L1iS957 = _M0L6_2atmpS2939;
      _M0L3idxS958 = _M0L6_2atmpS2940;
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
    int32_t _M0L6_2atmpS2913 = _M0L3curS928 + 1;
    int32_t _M0L14capacity__maskS2914 = _M0L4selfS930->$3;
    int32_t _M0L4nextS929 = _M0L6_2atmpS2913 & _M0L14capacity__maskS2914;
    struct _M0TPB5EntryGsiE** _M0L7entriesS2912 = _M0L4selfS930->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS933;
    struct _M0TPB5EntryGsiE** _M0L7entriesS2908;
    struct _M0TPB5EntryGsiE* _M0L6_2atmpS2909;
    struct _M0TPB5EntryGsiE* _M0L6_2aoldS3787;
    int32_t _tmp_4265;
    if (
      _M0L4nextS929 < 0
      || _M0L4nextS929 >= Moonbit_array_length(_M0L7entriesS2912)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS933
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2912[_M0L4nextS929];
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
          int32_t _M0L3pslS2911 = _M0L4_2axS935->$2;
          int32_t _M0L6_2atmpS2910 = _M0L3pslS2911 - 1;
          _M0L4_2axS935->$2 = _M0L6_2atmpS2910;
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
    goto joinlet_4264;
    join_931:;
    _M0L7entriesS2908 = _M0L4selfS930->$0;
    _M0L6_2atmpS2909 = 0;
    if (
      _M0L3curS928 < 0
      || _M0L3curS928 >= Moonbit_array_length(_M0L7entriesS2908)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3787
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2908[_M0L3curS928];
    if (_M0L6_2aoldS3787) {
      moonbit_decref(_M0L6_2aoldS3787);
    }
    _M0L7entriesS2908[_M0L3curS928] = _M0L6_2atmpS2909;
    break;
    joinlet_4264:;
    _tmp_4265 = _M0L3curS928;
    _M0L3curS928 = _tmp_4265;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS940,
  int32_t _M0L3idxS947
) {
  int32_t _M0L3curS938;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS938 = _M0L3idxS947;
  _2afor_942:;
  while (1) {
    int32_t _M0L6_2atmpS2920 = _M0L3curS938 + 1;
    int32_t _M0L14capacity__maskS2921 = _M0L4selfS940->$3;
    int32_t _M0L4nextS939 = _M0L6_2atmpS2920 & _M0L14capacity__maskS2921;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2919 =
      _M0L4selfS940->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS943;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2915;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2916;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS3791;
    int32_t _tmp_4268;
    if (
      _M0L4nextS939 < 0
      || _M0L4nextS939 >= Moonbit_array_length(_M0L7entriesS2919)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS943
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2919[
        _M0L4nextS939
      ];
    if (_M0L7_2abindS943 == 0) {
      goto join_941;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS944 =
        _M0L7_2abindS943;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4_2axS945 =
        _M0L7_2aSomeS944;
      int32_t _M0L4_2axS946 = _M0L4_2axS945->$2;
      switch (_M0L4_2axS946) {
        case 0: {
          goto join_941;
          break;
        }
        default: {
          int32_t _M0L3pslS2918 = _M0L4_2axS945->$2;
          int32_t _M0L6_2atmpS2917 = _M0L3pslS2918 - 1;
          _M0L4_2axS945->$2 = _M0L6_2atmpS2917;
          moonbit_incref(_M0L4_2axS945);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS940, _M0L4_2axS945, _M0L3curS938);
          moonbit_decref(_M0L4_2axS945);
          _M0L3curS938 = _M0L4nextS939;
          goto _2afor_942;
          break;
        }
      }
    }
    goto joinlet_4267;
    join_941:;
    _M0L7entriesS2915 = _M0L4selfS940->$0;
    _M0L6_2atmpS2916 = 0;
    if (
      _M0L3curS938 < 0
      || _M0L3curS938 >= Moonbit_array_length(_M0L7entriesS2915)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3791
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2915[
        _M0L3curS938
      ];
    if (_M0L6_2aoldS3791) {
      moonbit_decref(_M0L6_2aoldS3791);
    }
    _M0L7entriesS2915[_M0L3curS938] = _M0L6_2atmpS2916;
    break;
    joinlet_4267:;
    _tmp_4268 = _M0L3curS938;
    _M0L3curS938 = _tmp_4268;
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
      struct _M0TPB5EntryGsiE* _M0L4nextS2894 = _M0L5entryS917->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3796 = _M0L4selfS918->$5;
      if (_M0L4nextS2894) {
        moonbit_incref(_M0L4nextS2894);
      }
      if (_M0L6_2aoldS3796) {
        moonbit_decref(_M0L6_2aoldS3796);
      }
      _M0L4selfS918->$5 = _M0L4nextS2894;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS2898 = _M0L4selfS918->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2897;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2895;
      struct _M0TPB5EntryGsiE* _M0L4nextS2896;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3798;
      if (
        _M0L7_2abindS916 < 0
        || _M0L7_2abindS916 >= Moonbit_array_length(_M0L7entriesS2898)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2897
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2898[_M0L7_2abindS916];
      if (_M0L6_2atmpS2897) {
        moonbit_incref(_M0L6_2atmpS2897);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2895
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS2897);
      if (_M0L6_2atmpS2897) {
        moonbit_decref(_M0L6_2atmpS2897);
      }
      _M0L4nextS2896 = _M0L5entryS917->$1;
      _M0L6_2aoldS3798 = _M0L6_2atmpS2895->$1;
      if (_M0L4nextS2896) {
        moonbit_incref(_M0L4nextS2896);
      }
      if (_M0L6_2aoldS3798) {
        moonbit_decref(_M0L6_2aoldS3798);
      }
      _M0L6_2atmpS2895->$1 = _M0L4nextS2896;
      moonbit_decref(_M0L6_2atmpS2895);
      break;
    }
  }
  _M0L7_2abindS919 = _M0L5entryS917->$1;
  if (_M0L7_2abindS919 == 0) {
    int32_t _M0L4prevS2899 = _M0L5entryS917->$0;
    _M0L4selfS918->$6 = _M0L4prevS2899;
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS920 = _M0L7_2abindS919;
    struct _M0TPB5EntryGsiE* _M0L7_2anextS921 = _M0L7_2aSomeS920;
    int32_t _M0L4prevS2900 = _M0L5entryS917->$0;
    _M0L7_2anextS921->$0 = _M0L4prevS2900;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS924,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS923
) {
  int32_t _M0L7_2abindS922;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS925;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS922 = _M0L5entryS923->$0;
  switch (_M0L7_2abindS922) {
    case -1: {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4nextS2901 =
        _M0L5entryS923->$1;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS3803 =
        _M0L4selfS924->$5;
      if (_M0L4nextS2901) {
        moonbit_incref(_M0L4nextS2901);
      }
      if (_M0L6_2aoldS3803) {
        moonbit_decref(_M0L6_2aoldS3803);
      }
      _M0L4selfS924->$5 = _M0L4nextS2901;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2905 =
        _M0L4selfS924->$0;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2904;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2902;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4nextS2903;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS3805;
      if (
        _M0L7_2abindS922 < 0
        || _M0L7_2abindS922 >= Moonbit_array_length(_M0L7entriesS2905)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2904
      = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2905[
          _M0L7_2abindS922
        ];
      if (_M0L6_2atmpS2904) {
        moonbit_incref(_M0L6_2atmpS2904);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2902
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP19moonbitDB10RedisValueEE(_M0L6_2atmpS2904);
      if (_M0L6_2atmpS2904) {
        moonbit_decref(_M0L6_2atmpS2904);
      }
      _M0L4nextS2903 = _M0L5entryS923->$1;
      _M0L6_2aoldS3805 = _M0L6_2atmpS2902->$1;
      if (_M0L4nextS2903) {
        moonbit_incref(_M0L4nextS2903);
      }
      if (_M0L6_2aoldS3805) {
        moonbit_decref(_M0L6_2aoldS3805);
      }
      _M0L6_2atmpS2902->$1 = _M0L4nextS2903;
      moonbit_decref(_M0L6_2atmpS2902);
      break;
    }
  }
  _M0L7_2abindS925 = _M0L5entryS923->$1;
  if (_M0L7_2abindS925 == 0) {
    int32_t _M0L4prevS2906 = _M0L5entryS923->$0;
    _M0L4selfS924->$6 = _M0L4prevS2906;
  } else {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS926 =
      _M0L7_2abindS925;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2anextS927 =
      _M0L7_2aSomeS926;
    int32_t _M0L4prevS2907 = _M0L5entryS923->$0;
    _M0L7_2anextS927->$0 = _M0L4prevS2907;
  }
  return 0;
}

int32_t _M0MPB3Map8containsGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS885,
  moonbit_string_t _M0L3keyS881
) {
  int32_t _M0L4hashS880;
  int32_t _M0L14capacity__maskS2863;
  int32_t _M0L6_2atmpS2862;
  int32_t _M0L1iS882;
  int32_t _M0L3idxS883;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS880 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS881);
  _M0L14capacity__maskS2863 = _M0L4selfS885->$3;
  _M0L6_2atmpS2862 = _M0L4hashS880 & _M0L14capacity__maskS2863;
  _M0L1iS882 = 0;
  _M0L3idxS883 = _M0L6_2atmpS2862;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2861 = _M0L4selfS885->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS884;
    if (
      _M0L3idxS883 < 0
      || _M0L3idxS883 >= Moonbit_array_length(_M0L7entriesS2861)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS884
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2861[_M0L3idxS883];
    if (_M0L7_2abindS884 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS886 = _M0L7_2abindS884;
      struct _M0TPB5EntryGsbE* _M0L8_2aentryS887 = _M0L7_2aSomeS886;
      int32_t _M0L4hashS2855 = _M0L8_2aentryS887->$3;
      int32_t _if__result_4270;
      int32_t _M0L3pslS2856;
      int32_t _M0L6_2atmpS2857;
      int32_t _M0L6_2atmpS2859;
      int32_t _M0L14capacity__maskS2860;
      int32_t _M0L6_2atmpS2858;
      if (_M0L4hashS2855 == _M0L4hashS880) {
        moonbit_string_t _M0L3keyS2854 = _M0L8_2aentryS887->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4270
        = _M0L3keyS2854 == _M0L3keyS881
          || Moonbit_array_length(_M0L3keyS2854)
             == Moonbit_array_length(_M0L3keyS881)
             && 0
                == memcmp(_M0L3keyS2854, _M0L3keyS881, Moonbit_array_length(_M0L3keyS2854) * 2);
      } else {
        _if__result_4270 = 0;
      }
      if (_if__result_4270) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS887);
      }
      _M0L3pslS2856 = _M0L8_2aentryS887->$2;
      moonbit_decref(_M0L8_2aentryS887);
      if (_M0L1iS882 > _M0L3pslS2856) {
        return 0;
      }
      _M0L6_2atmpS2857 = _M0L1iS882 + 1;
      _M0L6_2atmpS2859 = _M0L3idxS883 + 1;
      _M0L14capacity__maskS2860 = _M0L4selfS885->$3;
      _M0L6_2atmpS2858 = _M0L6_2atmpS2859 & _M0L14capacity__maskS2860;
      _M0L1iS882 = _M0L6_2atmpS2857;
      _M0L3idxS883 = _M0L6_2atmpS2858;
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
  int32_t _M0L14capacity__maskS2873;
  int32_t _M0L6_2atmpS2872;
  int32_t _M0L1iS891;
  int32_t _M0L3idxS892;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS889 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS890);
  _M0L14capacity__maskS2873 = _M0L4selfS894->$3;
  _M0L6_2atmpS2872 = _M0L4hashS889 & _M0L14capacity__maskS2873;
  _M0L1iS891 = 0;
  _M0L3idxS892 = _M0L6_2atmpS2872;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2871 = _M0L4selfS894->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS893;
    if (
      _M0L3idxS892 < 0
      || _M0L3idxS892 >= Moonbit_array_length(_M0L7entriesS2871)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS893
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2871[_M0L3idxS892];
    if (_M0L7_2abindS893 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS895 = _M0L7_2abindS893;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS896 = _M0L7_2aSomeS895;
      int32_t _M0L4hashS2865 = _M0L8_2aentryS896->$3;
      int32_t _if__result_4272;
      int32_t _M0L3pslS2866;
      int32_t _M0L6_2atmpS2867;
      int32_t _M0L6_2atmpS2869;
      int32_t _M0L14capacity__maskS2870;
      int32_t _M0L6_2atmpS2868;
      if (_M0L4hashS2865 == _M0L4hashS889) {
        moonbit_string_t _M0L3keyS2864 = _M0L8_2aentryS896->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4272
        = _M0L3keyS2864 == _M0L3keyS890
          || Moonbit_array_length(_M0L3keyS2864)
             == Moonbit_array_length(_M0L3keyS890)
             && 0
                == memcmp(_M0L3keyS2864, _M0L3keyS890, Moonbit_array_length(_M0L3keyS2864) * 2);
      } else {
        _if__result_4272 = 0;
      }
      if (_if__result_4272) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS896);
      }
      _M0L3pslS2866 = _M0L8_2aentryS896->$2;
      moonbit_decref(_M0L8_2aentryS896);
      if (_M0L1iS891 > _M0L3pslS2866) {
        return 0;
      }
      _M0L6_2atmpS2867 = _M0L1iS891 + 1;
      _M0L6_2atmpS2869 = _M0L3idxS892 + 1;
      _M0L14capacity__maskS2870 = _M0L4selfS894->$3;
      _M0L6_2atmpS2868 = _M0L6_2atmpS2869 & _M0L14capacity__maskS2870;
      _M0L1iS891 = _M0L6_2atmpS2867;
      _M0L3idxS892 = _M0L6_2atmpS2868;
      continue;
    }
    break;
  }
}

int32_t _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS903,
  moonbit_string_t _M0L3keyS899
) {
  int32_t _M0L4hashS898;
  int32_t _M0L14capacity__maskS2883;
  int32_t _M0L6_2atmpS2882;
  int32_t _M0L1iS900;
  int32_t _M0L3idxS901;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS898 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS899);
  _M0L14capacity__maskS2883 = _M0L4selfS903->$3;
  _M0L6_2atmpS2882 = _M0L4hashS898 & _M0L14capacity__maskS2883;
  _M0L1iS900 = 0;
  _M0L3idxS901 = _M0L6_2atmpS2882;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2881 =
      _M0L4selfS903->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS902;
    if (
      _M0L3idxS901 < 0
      || _M0L3idxS901 >= Moonbit_array_length(_M0L7entriesS2881)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS902
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2881[
        _M0L3idxS901
      ];
    if (_M0L7_2abindS902 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS904 =
        _M0L7_2abindS902;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L8_2aentryS905 =
        _M0L7_2aSomeS904;
      int32_t _M0L4hashS2875 = _M0L8_2aentryS905->$3;
      int32_t _if__result_4274;
      int32_t _M0L3pslS2876;
      int32_t _M0L6_2atmpS2877;
      int32_t _M0L6_2atmpS2879;
      int32_t _M0L14capacity__maskS2880;
      int32_t _M0L6_2atmpS2878;
      if (_M0L4hashS2875 == _M0L4hashS898) {
        moonbit_string_t _M0L3keyS2874 = _M0L8_2aentryS905->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4274
        = _M0L3keyS2874 == _M0L3keyS899
          || Moonbit_array_length(_M0L3keyS2874)
             == Moonbit_array_length(_M0L3keyS899)
             && 0
                == memcmp(_M0L3keyS2874, _M0L3keyS899, Moonbit_array_length(_M0L3keyS2874) * 2);
      } else {
        _if__result_4274 = 0;
      }
      if (_if__result_4274) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS905);
      }
      _M0L3pslS2876 = _M0L8_2aentryS905->$2;
      moonbit_decref(_M0L8_2aentryS905);
      if (_M0L1iS900 > _M0L3pslS2876) {
        return 0;
      }
      _M0L6_2atmpS2877 = _M0L1iS900 + 1;
      _M0L6_2atmpS2879 = _M0L3idxS901 + 1;
      _M0L14capacity__maskS2880 = _M0L4selfS903->$3;
      _M0L6_2atmpS2878 = _M0L6_2atmpS2879 & _M0L14capacity__maskS2880;
      _M0L1iS900 = _M0L6_2atmpS2877;
      _M0L3idxS901 = _M0L6_2atmpS2878;
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
  int32_t _M0L14capacity__maskS2893;
  int32_t _M0L6_2atmpS2892;
  int32_t _M0L1iS909;
  int32_t _M0L3idxS910;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS907 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS908);
  _M0L14capacity__maskS2893 = _M0L4selfS912->$3;
  _M0L6_2atmpS2892 = _M0L4hashS907 & _M0L14capacity__maskS2893;
  _M0L1iS909 = 0;
  _M0L3idxS910 = _M0L6_2atmpS2892;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2891 = _M0L4selfS912->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS911;
    if (
      _M0L3idxS910 < 0
      || _M0L3idxS910 >= Moonbit_array_length(_M0L7entriesS2891)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS911
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2891[_M0L3idxS910];
    if (_M0L7_2abindS911 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS913 = _M0L7_2abindS911;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS914 = _M0L7_2aSomeS913;
      int32_t _M0L4hashS2885 = _M0L8_2aentryS914->$3;
      int32_t _if__result_4276;
      int32_t _M0L3pslS2886;
      int32_t _M0L6_2atmpS2887;
      int32_t _M0L6_2atmpS2889;
      int32_t _M0L14capacity__maskS2890;
      int32_t _M0L6_2atmpS2888;
      if (_M0L4hashS2885 == _M0L4hashS907) {
        moonbit_string_t _M0L3keyS2884 = _M0L8_2aentryS914->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4276
        = _M0L3keyS2884 == _M0L3keyS908
          || Moonbit_array_length(_M0L3keyS2884)
             == Moonbit_array_length(_M0L3keyS908)
             && 0
                == memcmp(_M0L3keyS2884, _M0L3keyS908, Moonbit_array_length(_M0L3keyS2884) * 2);
      } else {
        _if__result_4276 = 0;
      }
      if (_if__result_4276) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS914);
      }
      _M0L3pslS2886 = _M0L8_2aentryS914->$2;
      moonbit_decref(_M0L8_2aentryS914);
      if (_M0L1iS909 > _M0L3pslS2886) {
        return 0;
      }
      _M0L6_2atmpS2887 = _M0L1iS909 + 1;
      _M0L6_2atmpS2889 = _M0L3idxS910 + 1;
      _M0L14capacity__maskS2890 = _M0L4selfS912->$3;
      _M0L6_2atmpS2888 = _M0L6_2atmpS2889 & _M0L14capacity__maskS2890;
      _M0L1iS909 = _M0L6_2atmpS2887;
      _M0L3idxS910 = _M0L6_2atmpS2888;
      continue;
    }
    break;
  }
}

void* _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS849,
  moonbit_string_t _M0L3keyS845
) {
  int32_t _M0L4hashS844;
  int32_t _M0L14capacity__maskS2813;
  int32_t _M0L6_2atmpS2812;
  int32_t _M0L1iS846;
  int32_t _M0L3idxS847;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS844 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS845);
  _M0L14capacity__maskS2813 = _M0L4selfS849->$3;
  _M0L6_2atmpS2812 = _M0L4hashS844 & _M0L14capacity__maskS2813;
  _M0L1iS846 = 0;
  _M0L3idxS847 = _M0L6_2atmpS2812;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2811 =
      _M0L4selfS849->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS848;
    if (
      _M0L3idxS847 < 0
      || _M0L3idxS847 >= Moonbit_array_length(_M0L7entriesS2811)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS848
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2811[
        _M0L3idxS847
      ];
    if (_M0L7_2abindS848 == 0) {
      void* _M0L6_2atmpS2800 = 0;
      return _M0L6_2atmpS2800;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS850 =
        _M0L7_2abindS848;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L8_2aentryS851 =
        _M0L7_2aSomeS850;
      int32_t _M0L4hashS2802 = _M0L8_2aentryS851->$3;
      int32_t _if__result_4278;
      int32_t _M0L3pslS2805;
      int32_t _M0L6_2atmpS2807;
      int32_t _M0L6_2atmpS2809;
      int32_t _M0L14capacity__maskS2810;
      int32_t _M0L6_2atmpS2808;
      if (_M0L4hashS2802 == _M0L4hashS844) {
        moonbit_string_t _M0L3keyS2801 = _M0L8_2aentryS851->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4278
        = _M0L3keyS2801 == _M0L3keyS845
          || Moonbit_array_length(_M0L3keyS2801)
             == Moonbit_array_length(_M0L3keyS845)
             && 0
                == memcmp(_M0L3keyS2801, _M0L3keyS845, Moonbit_array_length(_M0L3keyS2801) * 2);
      } else {
        _if__result_4278 = 0;
      }
      if (_if__result_4278) {
        void* _M0L5valueS2804 = _M0L8_2aentryS851->$5;
        void* _M0L6_2atmpS2803;
        moonbit_incref(_M0L5valueS2804);
        _M0L6_2atmpS2803 = _M0L5valueS2804;
        return _M0L6_2atmpS2803;
      } else {
        moonbit_incref(_M0L8_2aentryS851);
      }
      _M0L3pslS2805 = _M0L8_2aentryS851->$2;
      moonbit_decref(_M0L8_2aentryS851);
      if (_M0L1iS846 > _M0L3pslS2805) {
        void* _M0L6_2atmpS2806 = 0;
        return _M0L6_2atmpS2806;
      }
      _M0L6_2atmpS2807 = _M0L1iS846 + 1;
      _M0L6_2atmpS2809 = _M0L3idxS847 + 1;
      _M0L14capacity__maskS2810 = _M0L4selfS849->$3;
      _M0L6_2atmpS2808 = _M0L6_2atmpS2809 & _M0L14capacity__maskS2810;
      _M0L1iS846 = _M0L6_2atmpS2807;
      _M0L3idxS847 = _M0L6_2atmpS2808;
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
  int32_t _M0L14capacity__maskS2827;
  int32_t _M0L6_2atmpS2826;
  int32_t _M0L1iS855;
  int32_t _M0L3idxS856;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS853 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS854);
  _M0L14capacity__maskS2827 = _M0L4selfS858->$3;
  _M0L6_2atmpS2826 = _M0L4hashS853 & _M0L14capacity__maskS2827;
  _M0L1iS855 = 0;
  _M0L3idxS856 = _M0L6_2atmpS2826;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2825 = _M0L4selfS858->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS857;
    if (
      _M0L3idxS856 < 0
      || _M0L3idxS856 >= Moonbit_array_length(_M0L7entriesS2825)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS857
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2825[_M0L3idxS856];
    if (_M0L7_2abindS857 == 0) {
      moonbit_string_t _M0L6_2atmpS2814 = 0;
      return _M0L6_2atmpS2814;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS859 = _M0L7_2abindS857;
      struct _M0TPB5EntryGssE* _M0L8_2aentryS860 = _M0L7_2aSomeS859;
      int32_t _M0L4hashS2816 = _M0L8_2aentryS860->$3;
      int32_t _if__result_4280;
      int32_t _M0L3pslS2819;
      int32_t _M0L6_2atmpS2821;
      int32_t _M0L6_2atmpS2823;
      int32_t _M0L14capacity__maskS2824;
      int32_t _M0L6_2atmpS2822;
      if (_M0L4hashS2816 == _M0L4hashS853) {
        moonbit_string_t _M0L3keyS2815 = _M0L8_2aentryS860->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4280
        = _M0L3keyS2815 == _M0L3keyS854
          || Moonbit_array_length(_M0L3keyS2815)
             == Moonbit_array_length(_M0L3keyS854)
             && 0
                == memcmp(_M0L3keyS2815, _M0L3keyS854, Moonbit_array_length(_M0L3keyS2815) * 2);
      } else {
        _if__result_4280 = 0;
      }
      if (_if__result_4280) {
        moonbit_string_t _M0L5valueS2818 = _M0L8_2aentryS860->$5;
        moonbit_string_t _M0L6_2atmpS2817;
        moonbit_incref(_M0L5valueS2818);
        _M0L6_2atmpS2817 = _M0L5valueS2818;
        return _M0L6_2atmpS2817;
      } else {
        moonbit_incref(_M0L8_2aentryS860);
      }
      _M0L3pslS2819 = _M0L8_2aentryS860->$2;
      moonbit_decref(_M0L8_2aentryS860);
      if (_M0L1iS855 > _M0L3pslS2819) {
        moonbit_string_t _M0L6_2atmpS2820 = 0;
        return _M0L6_2atmpS2820;
      }
      _M0L6_2atmpS2821 = _M0L1iS855 + 1;
      _M0L6_2atmpS2823 = _M0L3idxS856 + 1;
      _M0L14capacity__maskS2824 = _M0L4selfS858->$3;
      _M0L6_2atmpS2822 = _M0L6_2atmpS2823 & _M0L14capacity__maskS2824;
      _M0L1iS855 = _M0L6_2atmpS2821;
      _M0L3idxS856 = _M0L6_2atmpS2822;
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
  int32_t _M0L14capacity__maskS2841;
  int32_t _M0L6_2atmpS2840;
  int32_t _M0L1iS864;
  int32_t _M0L3idxS865;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS862 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS863);
  _M0L14capacity__maskS2841 = _M0L4selfS867->$3;
  _M0L6_2atmpS2840 = _M0L4hashS862 & _M0L14capacity__maskS2841;
  _M0L1iS864 = 0;
  _M0L3idxS865 = _M0L6_2atmpS2840;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2839 = _M0L4selfS867->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS866;
    if (
      _M0L3idxS865 < 0
      || _M0L3idxS865 >= Moonbit_array_length(_M0L7entriesS2839)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS866
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2839[_M0L3idxS865];
    if (_M0L7_2abindS866 == 0) {
      void* _M0L4NoneS2828 =
        (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      return _M0L4NoneS2828;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS868 = _M0L7_2abindS866;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS869 = _M0L7_2aSomeS868;
      int32_t _M0L4hashS2830 = _M0L8_2aentryS869->$3;
      int32_t _if__result_4282;
      int32_t _M0L3pslS2833;
      int32_t _M0L6_2atmpS2835;
      int32_t _M0L6_2atmpS2837;
      int32_t _M0L14capacity__maskS2838;
      int32_t _M0L6_2atmpS2836;
      if (_M0L4hashS2830 == _M0L4hashS862) {
        moonbit_string_t _M0L3keyS2829 = _M0L8_2aentryS869->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4282
        = _M0L3keyS2829 == _M0L3keyS863
          || Moonbit_array_length(_M0L3keyS2829)
             == Moonbit_array_length(_M0L3keyS863)
             && 0
                == memcmp(_M0L3keyS2829, _M0L3keyS863, Moonbit_array_length(_M0L3keyS2829) * 2);
      } else {
        _if__result_4282 = 0;
      }
      if (_if__result_4282) {
        float _M0L5valueS2832 = _M0L8_2aentryS869->$5;
        void* _M0L4SomeS2831 =
          (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGfE4Some));
        Moonbit_object_header(_M0L4SomeS2831)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 1);
        ((struct _M0DTPC16option6OptionGfE4Some*)_M0L4SomeS2831)->$0
        = _M0L5valueS2832;
        return _M0L4SomeS2831;
      } else {
        moonbit_incref(_M0L8_2aentryS869);
      }
      _M0L3pslS2833 = _M0L8_2aentryS869->$2;
      moonbit_decref(_M0L8_2aentryS869);
      if (_M0L1iS864 > _M0L3pslS2833) {
        void* _M0L4NoneS2834 =
          (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        return _M0L4NoneS2834;
      }
      _M0L6_2atmpS2835 = _M0L1iS864 + 1;
      _M0L6_2atmpS2837 = _M0L3idxS865 + 1;
      _M0L14capacity__maskS2838 = _M0L4selfS867->$3;
      _M0L6_2atmpS2836 = _M0L6_2atmpS2837 & _M0L14capacity__maskS2838;
      _M0L1iS864 = _M0L6_2atmpS2835;
      _M0L3idxS865 = _M0L6_2atmpS2836;
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
  int32_t _M0L14capacity__maskS2853;
  int32_t _M0L6_2atmpS2852;
  int32_t _M0L1iS873;
  int32_t _M0L3idxS874;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS871 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS872);
  _M0L14capacity__maskS2853 = _M0L4selfS876->$3;
  _M0L6_2atmpS2852 = _M0L4hashS871 & _M0L14capacity__maskS2853;
  _M0L1iS873 = 0;
  _M0L3idxS874 = _M0L6_2atmpS2852;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2851 = _M0L4selfS876->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS875;
    if (
      _M0L3idxS874 < 0
      || _M0L3idxS874 >= Moonbit_array_length(_M0L7entriesS2851)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS875
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2851[_M0L3idxS874];
    if (_M0L7_2abindS875 == 0) {
      return 4294967296ll;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS877 = _M0L7_2abindS875;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS878 = _M0L7_2aSomeS877;
      int32_t _M0L4hashS2843 = _M0L8_2aentryS878->$3;
      int32_t _if__result_4284;
      int32_t _M0L3pslS2846;
      int32_t _M0L6_2atmpS2847;
      int32_t _M0L6_2atmpS2849;
      int32_t _M0L14capacity__maskS2850;
      int32_t _M0L6_2atmpS2848;
      if (_M0L4hashS2843 == _M0L4hashS871) {
        moonbit_string_t _M0L3keyS2842 = _M0L8_2aentryS878->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4284
        = _M0L3keyS2842 == _M0L3keyS872
          || Moonbit_array_length(_M0L3keyS2842)
             == Moonbit_array_length(_M0L3keyS872)
             && 0
                == memcmp(_M0L3keyS2842, _M0L3keyS872, Moonbit_array_length(_M0L3keyS2842) * 2);
      } else {
        _if__result_4284 = 0;
      }
      if (_if__result_4284) {
        int32_t _M0L5valueS2845 = _M0L8_2aentryS878->$5;
        int64_t _M0L6_2atmpS2844 = (int64_t)_M0L5valueS2845;
        return _M0L6_2atmpS2844;
      } else {
        moonbit_incref(_M0L8_2aentryS878);
      }
      _M0L3pslS2846 = _M0L8_2aentryS878->$2;
      moonbit_decref(_M0L8_2aentryS878);
      if (_M0L1iS873 > _M0L3pslS2846) {
        return 4294967296ll;
      }
      _M0L6_2atmpS2847 = _M0L1iS873 + 1;
      _M0L6_2atmpS2849 = _M0L3idxS874 + 1;
      _M0L14capacity__maskS2850 = _M0L4selfS876->$3;
      _M0L6_2atmpS2848 = _M0L6_2atmpS2849 & _M0L14capacity__maskS2850;
      _M0L1iS873 = _M0L6_2atmpS2847;
      _M0L3idxS874 = _M0L6_2atmpS2848;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0MPB3Map3MapGsRP19moonbitDB10RedisValueE(
  struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE _M0L3arrS790,
  int64_t _M0L8capacityS792
) {
  int32_t _M0L3endS2754;
  int32_t _M0L5startS2755;
  int32_t _M0L6lengthS789;
  int32_t _M0L8capacityS791;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L1mS795;
  int32_t _M0L3endS2751;
  int32_t _M0L5startS2752;
  int32_t _M0L7_2abindS796;
  int32_t _M0L2__S797;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2754 = _M0L3arrS790.$2;
  _M0L5startS2755 = _M0L3arrS790.$1;
  _M0L6lengthS789 = _M0L3endS2754 - _M0L5startS2755;
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
    int32_t _M0L6_2atmpS2753;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2753 = _M0FPB21capacity__for__length(_M0L6lengthS789);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS791
    = _M0MPC13int3Int3max(_M0L11_2acapacityS794, _M0L6_2atmpS2753);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS795 = _M0FPB8new__mapGsRP19moonbitDB10RedisValueE(_M0L8capacityS791);
  _M0L3endS2751 = _M0L3arrS790.$2;
  _M0L5startS2752 = _M0L3arrS790.$1;
  _M0L7_2abindS796 = _M0L3endS2751 - _M0L5startS2752;
  _M0L2__S797 = 0;
  while (1) {
    if (_M0L2__S797 < _M0L7_2abindS796) {
      struct _M0TUsRP19moonbitDB10RedisValueE** _M0L3bufS2748 =
        _M0L3arrS790.$0;
      int32_t _M0L5startS2750 = _M0L3arrS790.$1;
      int32_t _M0L6_2atmpS2749 = _M0L5startS2750 + _M0L2__S797;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L1eS798 =
        (struct _M0TUsRP19moonbitDB10RedisValueE*)_M0L3bufS2748[
          _M0L6_2atmpS2749
        ];
      moonbit_string_t _M0L6_2atmpS2745 = _M0L1eS798->$0;
      void* _M0L6_2atmpS2746 = _M0L1eS798->$1;
      int32_t _M0L6_2atmpS2747;
      moonbit_incref(_M0L6_2atmpS2746);
      moonbit_incref(_M0L6_2atmpS2745);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L1mS795, _M0L6_2atmpS2745, _M0L6_2atmpS2746);
      moonbit_decref(_M0L6_2atmpS2745);
      moonbit_decref(_M0L6_2atmpS2746);
      _M0L6_2atmpS2747 = _M0L2__S797 + 1;
      _M0L2__S797 = _M0L6_2atmpS2747;
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
  int32_t _M0L3endS2765;
  int32_t _M0L5startS2766;
  int32_t _M0L6lengthS800;
  int32_t _M0L8capacityS802;
  struct _M0TPB3MapGsiE* _M0L1mS806;
  int32_t _M0L3endS2762;
  int32_t _M0L5startS2763;
  int32_t _M0L7_2abindS807;
  int32_t _M0L2__S808;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2765 = _M0L3arrS801.$2;
  _M0L5startS2766 = _M0L3arrS801.$1;
  _M0L6lengthS800 = _M0L3endS2765 - _M0L5startS2766;
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
    int32_t _M0L6_2atmpS2764;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2764 = _M0FPB21capacity__for__length(_M0L6lengthS800);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS802
    = _M0MPC13int3Int3max(_M0L11_2acapacityS805, _M0L6_2atmpS2764);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS806 = _M0FPB8new__mapGsiE(_M0L8capacityS802);
  _M0L3endS2762 = _M0L3arrS801.$2;
  _M0L5startS2763 = _M0L3arrS801.$1;
  _M0L7_2abindS807 = _M0L3endS2762 - _M0L5startS2763;
  _M0L2__S808 = 0;
  while (1) {
    if (_M0L2__S808 < _M0L7_2abindS807) {
      struct _M0TUsiE** _M0L3bufS2759 = _M0L3arrS801.$0;
      int32_t _M0L5startS2761 = _M0L3arrS801.$1;
      int32_t _M0L6_2atmpS2760 = _M0L5startS2761 + _M0L2__S808;
      struct _M0TUsiE* _M0L1eS809 =
        (struct _M0TUsiE*)_M0L3bufS2759[_M0L6_2atmpS2760];
      moonbit_string_t _M0L6_2atmpS2756 = _M0L1eS809->$0;
      int32_t _M0L6_2atmpS2757 = _M0L1eS809->$1;
      int32_t _M0L6_2atmpS2758;
      moonbit_incref(_M0L6_2atmpS2756);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsiE(_M0L1mS806, _M0L6_2atmpS2756, _M0L6_2atmpS2757);
      moonbit_decref(_M0L6_2atmpS2756);
      _M0L6_2atmpS2758 = _M0L2__S808 + 1;
      _M0L2__S808 = _M0L6_2atmpS2758;
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
  int32_t _M0L3endS2776;
  int32_t _M0L5startS2777;
  int32_t _M0L6lengthS811;
  int32_t _M0L8capacityS813;
  struct _M0TPB3MapGssE* _M0L1mS817;
  int32_t _M0L3endS2773;
  int32_t _M0L5startS2774;
  int32_t _M0L7_2abindS818;
  int32_t _M0L2__S819;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2776 = _M0L3arrS812.$2;
  _M0L5startS2777 = _M0L3arrS812.$1;
  _M0L6lengthS811 = _M0L3endS2776 - _M0L5startS2777;
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
    int32_t _M0L6_2atmpS2775;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2775 = _M0FPB21capacity__for__length(_M0L6lengthS811);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS813
    = _M0MPC13int3Int3max(_M0L11_2acapacityS816, _M0L6_2atmpS2775);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS817 = _M0FPB8new__mapGssE(_M0L8capacityS813);
  _M0L3endS2773 = _M0L3arrS812.$2;
  _M0L5startS2774 = _M0L3arrS812.$1;
  _M0L7_2abindS818 = _M0L3endS2773 - _M0L5startS2774;
  _M0L2__S819 = 0;
  while (1) {
    if (_M0L2__S819 < _M0L7_2abindS818) {
      struct _M0TUssE** _M0L3bufS2770 = _M0L3arrS812.$0;
      int32_t _M0L5startS2772 = _M0L3arrS812.$1;
      int32_t _M0L6_2atmpS2771 = _M0L5startS2772 + _M0L2__S819;
      struct _M0TUssE* _M0L1eS820 =
        (struct _M0TUssE*)_M0L3bufS2770[_M0L6_2atmpS2771];
      moonbit_string_t _M0L6_2atmpS2767 = _M0L1eS820->$0;
      moonbit_string_t _M0L6_2atmpS2768 = _M0L1eS820->$1;
      int32_t _M0L6_2atmpS2769;
      moonbit_incref(_M0L6_2atmpS2768);
      moonbit_incref(_M0L6_2atmpS2767);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGssE(_M0L1mS817, _M0L6_2atmpS2767, _M0L6_2atmpS2768);
      moonbit_decref(_M0L6_2atmpS2767);
      moonbit_decref(_M0L6_2atmpS2768);
      _M0L6_2atmpS2769 = _M0L2__S819 + 1;
      _M0L2__S819 = _M0L6_2atmpS2769;
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
  int32_t _M0L3endS2787;
  int32_t _M0L5startS2788;
  int32_t _M0L6lengthS822;
  int32_t _M0L8capacityS824;
  struct _M0TPB3MapGsbE* _M0L1mS828;
  int32_t _M0L3endS2784;
  int32_t _M0L5startS2785;
  int32_t _M0L7_2abindS829;
  int32_t _M0L2__S830;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2787 = _M0L3arrS823.$2;
  _M0L5startS2788 = _M0L3arrS823.$1;
  _M0L6lengthS822 = _M0L3endS2787 - _M0L5startS2788;
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
    int32_t _M0L6_2atmpS2786;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2786 = _M0FPB21capacity__for__length(_M0L6lengthS822);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS824
    = _M0MPC13int3Int3max(_M0L11_2acapacityS827, _M0L6_2atmpS2786);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS828 = _M0FPB8new__mapGsbE(_M0L8capacityS824);
  _M0L3endS2784 = _M0L3arrS823.$2;
  _M0L5startS2785 = _M0L3arrS823.$1;
  _M0L7_2abindS829 = _M0L3endS2784 - _M0L5startS2785;
  _M0L2__S830 = 0;
  while (1) {
    if (_M0L2__S830 < _M0L7_2abindS829) {
      struct _M0TUsbE** _M0L3bufS2781 = _M0L3arrS823.$0;
      int32_t _M0L5startS2783 = _M0L3arrS823.$1;
      int32_t _M0L6_2atmpS2782 = _M0L5startS2783 + _M0L2__S830;
      struct _M0TUsbE* _M0L1eS831 =
        (struct _M0TUsbE*)_M0L3bufS2781[_M0L6_2atmpS2782];
      moonbit_string_t _M0L6_2atmpS2778 = _M0L1eS831->$0;
      int32_t _M0L6_2atmpS2779 = _M0L1eS831->$1;
      int32_t _M0L6_2atmpS2780;
      moonbit_incref(_M0L6_2atmpS2778);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsbE(_M0L1mS828, _M0L6_2atmpS2778, _M0L6_2atmpS2779);
      moonbit_decref(_M0L6_2atmpS2778);
      _M0L6_2atmpS2780 = _M0L2__S830 + 1;
      _M0L2__S830 = _M0L6_2atmpS2780;
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
  int32_t _M0L3endS2798;
  int32_t _M0L5startS2799;
  int32_t _M0L6lengthS833;
  int32_t _M0L8capacityS835;
  struct _M0TPB3MapGsfE* _M0L1mS839;
  int32_t _M0L3endS2795;
  int32_t _M0L5startS2796;
  int32_t _M0L7_2abindS840;
  int32_t _M0L2__S841;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2798 = _M0L3arrS834.$2;
  _M0L5startS2799 = _M0L3arrS834.$1;
  _M0L6lengthS833 = _M0L3endS2798 - _M0L5startS2799;
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
    int32_t _M0L6_2atmpS2797;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2797 = _M0FPB21capacity__for__length(_M0L6lengthS833);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS835
    = _M0MPC13int3Int3max(_M0L11_2acapacityS838, _M0L6_2atmpS2797);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS839 = _M0FPB8new__mapGsfE(_M0L8capacityS835);
  _M0L3endS2795 = _M0L3arrS834.$2;
  _M0L5startS2796 = _M0L3arrS834.$1;
  _M0L7_2abindS840 = _M0L3endS2795 - _M0L5startS2796;
  _M0L2__S841 = 0;
  while (1) {
    if (_M0L2__S841 < _M0L7_2abindS840) {
      struct _M0TUsfE** _M0L3bufS2792 = _M0L3arrS834.$0;
      int32_t _M0L5startS2794 = _M0L3arrS834.$1;
      int32_t _M0L6_2atmpS2793 = _M0L5startS2794 + _M0L2__S841;
      struct _M0TUsfE* _M0L1eS842 =
        (struct _M0TUsfE*)_M0L3bufS2792[_M0L6_2atmpS2793];
      moonbit_string_t _M0L6_2atmpS2789 = _M0L1eS842->$0;
      float _M0L6_2atmpS2790 = _M0L1eS842->$1;
      int32_t _M0L6_2atmpS2791;
      moonbit_incref(_M0L6_2atmpS2789);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsfE(_M0L1mS839, _M0L6_2atmpS2789, _M0L6_2atmpS2790);
      moonbit_decref(_M0L6_2atmpS2789);
      _M0L6_2atmpS2791 = _M0L2__S841 + 1;
      _M0L2__S841 = _M0L6_2atmpS2791;
      continue;
    }
    break;
  }
  return _M0L1mS839;
}

int32_t _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS774,
  moonbit_string_t _M0L3keyS775,
  void* _M0L5valueS776
) {
  int32_t _M0L6_2atmpS2740;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2740 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS775);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRP19moonbitDB10RedisValueE(_M0L4selfS774, _M0L3keyS775, _M0L5valueS776, _M0L6_2atmpS2740);
  return 0;
}

int32_t _M0MPB3Map3setGssE(
  struct _M0TPB3MapGssE* _M0L4selfS777,
  moonbit_string_t _M0L3keyS778,
  moonbit_string_t _M0L5valueS779
) {
  int32_t _M0L6_2atmpS2741;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2741 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS778);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGssE(_M0L4selfS777, _M0L3keyS778, _M0L5valueS779, _M0L6_2atmpS2741);
  return 0;
}

int32_t _M0MPB3Map3setGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS780,
  moonbit_string_t _M0L3keyS781,
  int32_t _M0L5valueS782
) {
  int32_t _M0L6_2atmpS2742;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2742 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS781);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsbE(_M0L4selfS780, _M0L3keyS781, _M0L5valueS782, _M0L6_2atmpS2742);
  return 0;
}

int32_t _M0MPB3Map3setGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS783,
  moonbit_string_t _M0L3keyS784,
  float _M0L5valueS785
) {
  int32_t _M0L6_2atmpS2743;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2743 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS784);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsfE(_M0L4selfS783, _M0L3keyS784, _M0L5valueS785, _M0L6_2atmpS2743);
  return 0;
}

int32_t _M0MPB3Map3setGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS786,
  moonbit_string_t _M0L3keyS787,
  int32_t _M0L5valueS788
) {
  int32_t _M0L6_2atmpS2744;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2744 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS787);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsiE(_M0L4selfS786, _M0L3keyS787, _M0L5valueS788, _M0L6_2atmpS2744);
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS697,
  moonbit_string_t _M0L3keyS703,
  void* _M0L5valueS704,
  int32_t _M0L4hashS699
) {
  int32_t _M0L14capacity__maskS2667;
  int32_t _M0L6_2atmpS2666;
  int32_t _M0L3pslS694;
  int32_t _M0L3idxS695;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2667 = _M0L4selfS697->$3;
  _M0L6_2atmpS2666 = _M0L4hashS699 & _M0L14capacity__maskS2667;
  _M0L3pslS694 = 0;
  _M0L3idxS695 = _M0L6_2atmpS2666;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2665 =
      _M0L4selfS697->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS696;
    if (
      _M0L3idxS695 < 0
      || _M0L3idxS695 >= Moonbit_array_length(_M0L7entriesS2665)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS696
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2665[
        _M0L3idxS695
      ];
    if (_M0L7_2abindS696 == 0) {
      int32_t _M0L4sizeS2650 = _M0L4selfS697->$1;
      int32_t _M0L8grow__atS2651 = _M0L4selfS697->$4;
      int32_t _M0L7_2abindS700;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS701;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS702;
      if (_M0L4sizeS2650 >= _M0L8grow__atS2651) {
        int32_t _M0L14capacity__maskS2653;
        int32_t _M0L6_2atmpS2652;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRP19moonbitDB10RedisValueE(_M0L4selfS697);
        _M0L14capacity__maskS2653 = _M0L4selfS697->$3;
        _M0L6_2atmpS2652 = _M0L4hashS699 & _M0L14capacity__maskS2653;
        _M0L3pslS694 = 0;
        _M0L3idxS695 = _M0L6_2atmpS2652;
        continue;
      }
      _M0L7_2abindS700 = _M0L4selfS697->$6;
      _M0L7_2abindS701 = 0;
      moonbit_incref(_M0L3keyS703);
      moonbit_incref(_M0L5valueS704);
      _M0L5entryS702
      = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE));
      Moonbit_object_header(_M0L5entryS702)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 74, 0);
      _M0L5entryS702->$0 = _M0L7_2abindS700;
      _M0L5entryS702->$1 = _M0L7_2abindS701;
      _M0L5entryS702->$2 = _M0L3pslS694;
      _M0L5entryS702->$3 = _M0L4hashS699;
      _M0L5entryS702->$4 = _M0L3keyS703;
      _M0L5entryS702->$5 = _M0L5valueS704;
      #line 150 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(_M0L4selfS697, _M0L3idxS695, _M0L5entryS702);
      moonbit_decref(_M0L5entryS702);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS705 =
        _M0L7_2abindS696;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L14_2acurr__entryS706 =
        _M0L7_2aSomeS705;
      int32_t _M0L4hashS2655 = _M0L14_2acurr__entryS706->$3;
      int32_t _if__result_4291;
      int32_t _M0L3pslS2656;
      int32_t _M0L6_2atmpS2661;
      int32_t _M0L6_2atmpS2663;
      int32_t _M0L14capacity__maskS2664;
      int32_t _M0L6_2atmpS2662;
      if (_M0L4hashS2655 == _M0L4hashS699) {
        moonbit_string_t _M0L3keyS2654 = _M0L14_2acurr__entryS706->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4291
        = _M0L3keyS2654 == _M0L3keyS703
          || Moonbit_array_length(_M0L3keyS2654)
             == Moonbit_array_length(_M0L3keyS703)
             && 0
                == memcmp(_M0L3keyS2654, _M0L3keyS703, Moonbit_array_length(_M0L3keyS2654) * 2);
      } else {
        _if__result_4291 = 0;
      }
      if (_if__result_4291) {
        void* _M0L6_2aoldS3852 = _M0L14_2acurr__entryS706->$5;
        moonbit_incref(_M0L5valueS704);
        moonbit_decref(_M0L6_2aoldS3852);
        _M0L14_2acurr__entryS706->$5 = _M0L5valueS704;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS706);
      }
      _M0L3pslS2656 = _M0L14_2acurr__entryS706->$2;
      if (_M0L3pslS694 > _M0L3pslS2656) {
        int32_t _M0L4sizeS2657 = _M0L4selfS697->$1;
        int32_t _M0L8grow__atS2658 = _M0L4selfS697->$4;
        int32_t _M0L7_2abindS707;
        struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS708;
        struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS709;
        if (_M0L4sizeS2657 >= _M0L8grow__atS2658) {
          int32_t _M0L14capacity__maskS2660;
          int32_t _M0L6_2atmpS2659;
          moonbit_decref(_M0L14_2acurr__entryS706);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRP19moonbitDB10RedisValueE(_M0L4selfS697);
          _M0L14capacity__maskS2660 = _M0L4selfS697->$3;
          _M0L6_2atmpS2659 = _M0L4hashS699 & _M0L14capacity__maskS2660;
          _M0L3pslS694 = 0;
          _M0L3idxS695 = _M0L6_2atmpS2659;
          continue;
        }
        #line 166 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP19moonbitDB10RedisValueE(_M0L4selfS697, _M0L3idxS695, _M0L14_2acurr__entryS706);
        moonbit_decref(_M0L14_2acurr__entryS706);
        _M0L7_2abindS707 = _M0L4selfS697->$6;
        _M0L7_2abindS708 = 0;
        moonbit_incref(_M0L3keyS703);
        moonbit_incref(_M0L5valueS704);
        _M0L5entryS709
        = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE));
        Moonbit_object_header(_M0L5entryS709)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 74, 0);
        _M0L5entryS709->$0 = _M0L7_2abindS707;
        _M0L5entryS709->$1 = _M0L7_2abindS708;
        _M0L5entryS709->$2 = _M0L3pslS694;
        _M0L5entryS709->$3 = _M0L4hashS699;
        _M0L5entryS709->$4 = _M0L3keyS703;
        _M0L5entryS709->$5 = _M0L5valueS704;
        #line 168 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(_M0L4selfS697, _M0L3idxS695, _M0L5entryS709);
        moonbit_decref(_M0L5entryS709);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS706);
      }
      _M0L6_2atmpS2661 = _M0L3pslS694 + 1;
      _M0L6_2atmpS2663 = _M0L3idxS695 + 1;
      _M0L14capacity__maskS2664 = _M0L4selfS697->$3;
      _M0L6_2atmpS2662 = _M0L6_2atmpS2663 & _M0L14capacity__maskS2664;
      _M0L3pslS694 = _M0L6_2atmpS2661;
      _M0L3idxS695 = _M0L6_2atmpS2662;
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
  int32_t _M0L14capacity__maskS2685;
  int32_t _M0L6_2atmpS2684;
  int32_t _M0L3pslS710;
  int32_t _M0L3idxS711;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2685 = _M0L4selfS713->$3;
  _M0L6_2atmpS2684 = _M0L4hashS715 & _M0L14capacity__maskS2685;
  _M0L3pslS710 = 0;
  _M0L3idxS711 = _M0L6_2atmpS2684;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2683 = _M0L4selfS713->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS712;
    if (
      _M0L3idxS711 < 0
      || _M0L3idxS711 >= Moonbit_array_length(_M0L7entriesS2683)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS712
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2683[_M0L3idxS711];
    if (_M0L7_2abindS712 == 0) {
      int32_t _M0L4sizeS2668 = _M0L4selfS713->$1;
      int32_t _M0L8grow__atS2669 = _M0L4selfS713->$4;
      int32_t _M0L7_2abindS716;
      struct _M0TPB5EntryGssE* _M0L7_2abindS717;
      struct _M0TPB5EntryGssE* _M0L5entryS718;
      if (_M0L4sizeS2668 >= _M0L8grow__atS2669) {
        int32_t _M0L14capacity__maskS2671;
        int32_t _M0L6_2atmpS2670;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGssE(_M0L4selfS713);
        _M0L14capacity__maskS2671 = _M0L4selfS713->$3;
        _M0L6_2atmpS2670 = _M0L4hashS715 & _M0L14capacity__maskS2671;
        _M0L3pslS710 = 0;
        _M0L3idxS711 = _M0L6_2atmpS2670;
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
      int32_t _M0L4hashS2673 = _M0L14_2acurr__entryS722->$3;
      int32_t _if__result_4293;
      int32_t _M0L3pslS2674;
      int32_t _M0L6_2atmpS2679;
      int32_t _M0L6_2atmpS2681;
      int32_t _M0L14capacity__maskS2682;
      int32_t _M0L6_2atmpS2680;
      if (_M0L4hashS2673 == _M0L4hashS715) {
        moonbit_string_t _M0L3keyS2672 = _M0L14_2acurr__entryS722->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4293
        = _M0L3keyS2672 == _M0L3keyS719
          || Moonbit_array_length(_M0L3keyS2672)
             == Moonbit_array_length(_M0L3keyS719)
             && 0
                == memcmp(_M0L3keyS2672, _M0L3keyS719, Moonbit_array_length(_M0L3keyS2672) * 2);
      } else {
        _if__result_4293 = 0;
      }
      if (_if__result_4293) {
        moonbit_string_t _M0L6_2aoldS3856 = _M0L14_2acurr__entryS722->$5;
        moonbit_incref(_M0L5valueS720);
        moonbit_decref(_M0L6_2aoldS3856);
        _M0L14_2acurr__entryS722->$5 = _M0L5valueS720;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS722);
      }
      _M0L3pslS2674 = _M0L14_2acurr__entryS722->$2;
      if (_M0L3pslS710 > _M0L3pslS2674) {
        int32_t _M0L4sizeS2675 = _M0L4selfS713->$1;
        int32_t _M0L8grow__atS2676 = _M0L4selfS713->$4;
        int32_t _M0L7_2abindS723;
        struct _M0TPB5EntryGssE* _M0L7_2abindS724;
        struct _M0TPB5EntryGssE* _M0L5entryS725;
        if (_M0L4sizeS2675 >= _M0L8grow__atS2676) {
          int32_t _M0L14capacity__maskS2678;
          int32_t _M0L6_2atmpS2677;
          moonbit_decref(_M0L14_2acurr__entryS722);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGssE(_M0L4selfS713);
          _M0L14capacity__maskS2678 = _M0L4selfS713->$3;
          _M0L6_2atmpS2677 = _M0L4hashS715 & _M0L14capacity__maskS2678;
          _M0L3pslS710 = 0;
          _M0L3idxS711 = _M0L6_2atmpS2677;
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
      _M0L6_2atmpS2679 = _M0L3pslS710 + 1;
      _M0L6_2atmpS2681 = _M0L3idxS711 + 1;
      _M0L14capacity__maskS2682 = _M0L4selfS713->$3;
      _M0L6_2atmpS2680 = _M0L6_2atmpS2681 & _M0L14capacity__maskS2682;
      _M0L3pslS710 = _M0L6_2atmpS2679;
      _M0L3idxS711 = _M0L6_2atmpS2680;
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
  int32_t _M0L14capacity__maskS2703;
  int32_t _M0L6_2atmpS2702;
  int32_t _M0L3pslS726;
  int32_t _M0L3idxS727;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2703 = _M0L4selfS729->$3;
  _M0L6_2atmpS2702 = _M0L4hashS731 & _M0L14capacity__maskS2703;
  _M0L3pslS726 = 0;
  _M0L3idxS727 = _M0L6_2atmpS2702;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2701 = _M0L4selfS729->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS728;
    if (
      _M0L3idxS727 < 0
      || _M0L3idxS727 >= Moonbit_array_length(_M0L7entriesS2701)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS728
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2701[_M0L3idxS727];
    if (_M0L7_2abindS728 == 0) {
      int32_t _M0L4sizeS2686 = _M0L4selfS729->$1;
      int32_t _M0L8grow__atS2687 = _M0L4selfS729->$4;
      int32_t _M0L7_2abindS732;
      struct _M0TPB5EntryGsbE* _M0L7_2abindS733;
      struct _M0TPB5EntryGsbE* _M0L5entryS734;
      if (_M0L4sizeS2686 >= _M0L8grow__atS2687) {
        int32_t _M0L14capacity__maskS2689;
        int32_t _M0L6_2atmpS2688;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsbE(_M0L4selfS729);
        _M0L14capacity__maskS2689 = _M0L4selfS729->$3;
        _M0L6_2atmpS2688 = _M0L4hashS731 & _M0L14capacity__maskS2689;
        _M0L3pslS726 = 0;
        _M0L3idxS727 = _M0L6_2atmpS2688;
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
      int32_t _M0L4hashS2691 = _M0L14_2acurr__entryS738->$3;
      int32_t _if__result_4295;
      int32_t _M0L3pslS2692;
      int32_t _M0L6_2atmpS2697;
      int32_t _M0L6_2atmpS2699;
      int32_t _M0L14capacity__maskS2700;
      int32_t _M0L6_2atmpS2698;
      if (_M0L4hashS2691 == _M0L4hashS731) {
        moonbit_string_t _M0L3keyS2690 = _M0L14_2acurr__entryS738->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4295
        = _M0L3keyS2690 == _M0L3keyS735
          || Moonbit_array_length(_M0L3keyS2690)
             == Moonbit_array_length(_M0L3keyS735)
             && 0
                == memcmp(_M0L3keyS2690, _M0L3keyS735, Moonbit_array_length(_M0L3keyS2690) * 2);
      } else {
        _if__result_4295 = 0;
      }
      if (_if__result_4295) {
        _M0L14_2acurr__entryS738->$5 = _M0L5valueS736;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS738);
      }
      _M0L3pslS2692 = _M0L14_2acurr__entryS738->$2;
      if (_M0L3pslS726 > _M0L3pslS2692) {
        int32_t _M0L4sizeS2693 = _M0L4selfS729->$1;
        int32_t _M0L8grow__atS2694 = _M0L4selfS729->$4;
        int32_t _M0L7_2abindS739;
        struct _M0TPB5EntryGsbE* _M0L7_2abindS740;
        struct _M0TPB5EntryGsbE* _M0L5entryS741;
        if (_M0L4sizeS2693 >= _M0L8grow__atS2694) {
          int32_t _M0L14capacity__maskS2696;
          int32_t _M0L6_2atmpS2695;
          moonbit_decref(_M0L14_2acurr__entryS738);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsbE(_M0L4selfS729);
          _M0L14capacity__maskS2696 = _M0L4selfS729->$3;
          _M0L6_2atmpS2695 = _M0L4hashS731 & _M0L14capacity__maskS2696;
          _M0L3pslS726 = 0;
          _M0L3idxS727 = _M0L6_2atmpS2695;
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
      _M0L6_2atmpS2697 = _M0L3pslS726 + 1;
      _M0L6_2atmpS2699 = _M0L3idxS727 + 1;
      _M0L14capacity__maskS2700 = _M0L4selfS729->$3;
      _M0L6_2atmpS2698 = _M0L6_2atmpS2699 & _M0L14capacity__maskS2700;
      _M0L3pslS726 = _M0L6_2atmpS2697;
      _M0L3idxS727 = _M0L6_2atmpS2698;
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
  int32_t _M0L14capacity__maskS2721;
  int32_t _M0L6_2atmpS2720;
  int32_t _M0L3pslS742;
  int32_t _M0L3idxS743;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2721 = _M0L4selfS745->$3;
  _M0L6_2atmpS2720 = _M0L4hashS747 & _M0L14capacity__maskS2721;
  _M0L3pslS742 = 0;
  _M0L3idxS743 = _M0L6_2atmpS2720;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2719 = _M0L4selfS745->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS744;
    if (
      _M0L3idxS743 < 0
      || _M0L3idxS743 >= Moonbit_array_length(_M0L7entriesS2719)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS744
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2719[_M0L3idxS743];
    if (_M0L7_2abindS744 == 0) {
      int32_t _M0L4sizeS2704 = _M0L4selfS745->$1;
      int32_t _M0L8grow__atS2705 = _M0L4selfS745->$4;
      int32_t _M0L7_2abindS748;
      struct _M0TPB5EntryGsfE* _M0L7_2abindS749;
      struct _M0TPB5EntryGsfE* _M0L5entryS750;
      if (_M0L4sizeS2704 >= _M0L8grow__atS2705) {
        int32_t _M0L14capacity__maskS2707;
        int32_t _M0L6_2atmpS2706;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsfE(_M0L4selfS745);
        _M0L14capacity__maskS2707 = _M0L4selfS745->$3;
        _M0L6_2atmpS2706 = _M0L4hashS747 & _M0L14capacity__maskS2707;
        _M0L3pslS742 = 0;
        _M0L3idxS743 = _M0L6_2atmpS2706;
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
      int32_t _M0L4hashS2709 = _M0L14_2acurr__entryS754->$3;
      int32_t _if__result_4297;
      int32_t _M0L3pslS2710;
      int32_t _M0L6_2atmpS2715;
      int32_t _M0L6_2atmpS2717;
      int32_t _M0L14capacity__maskS2718;
      int32_t _M0L6_2atmpS2716;
      if (_M0L4hashS2709 == _M0L4hashS747) {
        moonbit_string_t _M0L3keyS2708 = _M0L14_2acurr__entryS754->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4297
        = _M0L3keyS2708 == _M0L3keyS751
          || Moonbit_array_length(_M0L3keyS2708)
             == Moonbit_array_length(_M0L3keyS751)
             && 0
                == memcmp(_M0L3keyS2708, _M0L3keyS751, Moonbit_array_length(_M0L3keyS2708) * 2);
      } else {
        _if__result_4297 = 0;
      }
      if (_if__result_4297) {
        _M0L14_2acurr__entryS754->$5 = _M0L5valueS752;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS754);
      }
      _M0L3pslS2710 = _M0L14_2acurr__entryS754->$2;
      if (_M0L3pslS742 > _M0L3pslS2710) {
        int32_t _M0L4sizeS2711 = _M0L4selfS745->$1;
        int32_t _M0L8grow__atS2712 = _M0L4selfS745->$4;
        int32_t _M0L7_2abindS755;
        struct _M0TPB5EntryGsfE* _M0L7_2abindS756;
        struct _M0TPB5EntryGsfE* _M0L5entryS757;
        if (_M0L4sizeS2711 >= _M0L8grow__atS2712) {
          int32_t _M0L14capacity__maskS2714;
          int32_t _M0L6_2atmpS2713;
          moonbit_decref(_M0L14_2acurr__entryS754);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsfE(_M0L4selfS745);
          _M0L14capacity__maskS2714 = _M0L4selfS745->$3;
          _M0L6_2atmpS2713 = _M0L4hashS747 & _M0L14capacity__maskS2714;
          _M0L3pslS742 = 0;
          _M0L3idxS743 = _M0L6_2atmpS2713;
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
      _M0L6_2atmpS2715 = _M0L3pslS742 + 1;
      _M0L6_2atmpS2717 = _M0L3idxS743 + 1;
      _M0L14capacity__maskS2718 = _M0L4selfS745->$3;
      _M0L6_2atmpS2716 = _M0L6_2atmpS2717 & _M0L14capacity__maskS2718;
      _M0L3pslS742 = _M0L6_2atmpS2715;
      _M0L3idxS743 = _M0L6_2atmpS2716;
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
  int32_t _M0L14capacity__maskS2739;
  int32_t _M0L6_2atmpS2738;
  int32_t _M0L3pslS758;
  int32_t _M0L3idxS759;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2739 = _M0L4selfS761->$3;
  _M0L6_2atmpS2738 = _M0L4hashS763 & _M0L14capacity__maskS2739;
  _M0L3pslS758 = 0;
  _M0L3idxS759 = _M0L6_2atmpS2738;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2737 = _M0L4selfS761->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS760;
    if (
      _M0L3idxS759 < 0
      || _M0L3idxS759 >= Moonbit_array_length(_M0L7entriesS2737)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS760
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2737[_M0L3idxS759];
    if (_M0L7_2abindS760 == 0) {
      int32_t _M0L4sizeS2722 = _M0L4selfS761->$1;
      int32_t _M0L8grow__atS2723 = _M0L4selfS761->$4;
      int32_t _M0L7_2abindS764;
      struct _M0TPB5EntryGsiE* _M0L7_2abindS765;
      struct _M0TPB5EntryGsiE* _M0L5entryS766;
      if (_M0L4sizeS2722 >= _M0L8grow__atS2723) {
        int32_t _M0L14capacity__maskS2725;
        int32_t _M0L6_2atmpS2724;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsiE(_M0L4selfS761);
        _M0L14capacity__maskS2725 = _M0L4selfS761->$3;
        _M0L6_2atmpS2724 = _M0L4hashS763 & _M0L14capacity__maskS2725;
        _M0L3pslS758 = 0;
        _M0L3idxS759 = _M0L6_2atmpS2724;
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
      int32_t _M0L4hashS2727 = _M0L14_2acurr__entryS770->$3;
      int32_t _if__result_4299;
      int32_t _M0L3pslS2728;
      int32_t _M0L6_2atmpS2733;
      int32_t _M0L6_2atmpS2735;
      int32_t _M0L14capacity__maskS2736;
      int32_t _M0L6_2atmpS2734;
      if (_M0L4hashS2727 == _M0L4hashS763) {
        moonbit_string_t _M0L3keyS2726 = _M0L14_2acurr__entryS770->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4299
        = _M0L3keyS2726 == _M0L3keyS767
          || Moonbit_array_length(_M0L3keyS2726)
             == Moonbit_array_length(_M0L3keyS767)
             && 0
                == memcmp(_M0L3keyS2726, _M0L3keyS767, Moonbit_array_length(_M0L3keyS2726) * 2);
      } else {
        _if__result_4299 = 0;
      }
      if (_if__result_4299) {
        _M0L14_2acurr__entryS770->$5 = _M0L5valueS768;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS770);
      }
      _M0L3pslS2728 = _M0L14_2acurr__entryS770->$2;
      if (_M0L3pslS758 > _M0L3pslS2728) {
        int32_t _M0L4sizeS2729 = _M0L4selfS761->$1;
        int32_t _M0L8grow__atS2730 = _M0L4selfS761->$4;
        int32_t _M0L7_2abindS771;
        struct _M0TPB5EntryGsiE* _M0L7_2abindS772;
        struct _M0TPB5EntryGsiE* _M0L5entryS773;
        if (_M0L4sizeS2729 >= _M0L8grow__atS2730) {
          int32_t _M0L14capacity__maskS2732;
          int32_t _M0L6_2atmpS2731;
          moonbit_decref(_M0L14_2acurr__entryS770);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsiE(_M0L4selfS761);
          _M0L14capacity__maskS2732 = _M0L4selfS761->$3;
          _M0L6_2atmpS2731 = _M0L4hashS763 & _M0L14capacity__maskS2732;
          _M0L3pslS758 = 0;
          _M0L3idxS759 = _M0L6_2atmpS2731;
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
      _M0L6_2atmpS2733 = _M0L3pslS758 + 1;
      _M0L6_2atmpS2735 = _M0L3idxS759 + 1;
      _M0L14capacity__maskS2736 = _M0L4selfS761->$3;
      _M0L6_2atmpS2734 = _M0L6_2atmpS2735 & _M0L14capacity__maskS2736;
      _M0L3pslS758 = _M0L6_2atmpS2733;
      _M0L3idxS759 = _M0L6_2atmpS2734;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS655
) {
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L9old__headS654;
  int32_t _M0L8capacityS2617;
  int32_t _M0L13new__capacityS656;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2611;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L6_2atmpS2610;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L6_2aoldS3872;
  int32_t _M0L6_2atmpS2612;
  int32_t _M0L8capacityS2614;
  int32_t _M0L6_2atmpS2613;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2615;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS3871;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L1xS657;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS654 = _M0L4selfS655->$5;
  _M0L8capacityS2617 = _M0L4selfS655->$2;
  _M0L13new__capacityS656 = _M0L8capacityS2617 << 1;
  _M0L6_2atmpS2611 = 0;
  _M0L6_2atmpS2610
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE**)moonbit_make_ref_array(_M0L13new__capacityS656, _M0L6_2atmpS2611);
  _M0L6_2aoldS3872 = _M0L4selfS655->$0;
  if (_M0L9old__headS654) {
    moonbit_incref(_M0L9old__headS654);
  }
  moonbit_decref(_M0L6_2aoldS3872);
  _M0L4selfS655->$0 = _M0L6_2atmpS2610;
  _M0L4selfS655->$2 = _M0L13new__capacityS656;
  _M0L6_2atmpS2612 = _M0L13new__capacityS656 - 1;
  _M0L4selfS655->$3 = _M0L6_2atmpS2612;
  _M0L8capacityS2614 = _M0L4selfS655->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2613 = _M0FPB21calc__grow__threshold(_M0L8capacityS2614);
  _M0L4selfS655->$4 = _M0L6_2atmpS2613;
  _M0L4selfS655->$1 = 0;
  _M0L6_2atmpS2615 = 0;
  _M0L6_2aoldS3871 = _M0L4selfS655->$5;
  if (_M0L6_2aoldS3871) {
    moonbit_decref(_M0L6_2aoldS3871);
  }
  _M0L4selfS655->$5 = _M0L6_2atmpS2615;
  _M0L4selfS655->$6 = -1;
  _M0L1xS657 = _M0L9old__headS654;
  while (1) {
    if (_M0L1xS657 == 0) {
      if (_M0L1xS657) {
        moonbit_decref(_M0L1xS657);
      }
      break;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS659 =
        _M0L1xS657;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4_2aeS660 =
        _M0L7_2aSomeS659;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L15next__in__chainS661 =
        _M0L4_2aeS660->$1;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2616 = 0;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS3869 =
        _M0L4_2aeS660->$1;
      if (_M0L15next__in__chainS661) {
        moonbit_incref(_M0L15next__in__chainS661);
      }
      if (_M0L6_2aoldS3869) {
        moonbit_decref(_M0L6_2aoldS3869);
      }
      _M0L4_2aeS660->$1 = _M0L6_2atmpS2616;
      #line 577 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20rehash__place__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS655, _M0L4_2aeS660);
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
  int32_t _M0L8capacityS2625;
  int32_t _M0L13new__capacityS664;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2619;
  struct _M0TPB5EntryGssE** _M0L6_2atmpS2618;
  struct _M0TPB5EntryGssE** _M0L6_2aoldS3877;
  int32_t _M0L6_2atmpS2620;
  int32_t _M0L8capacityS2622;
  int32_t _M0L6_2atmpS2621;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2623;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3876;
  struct _M0TPB5EntryGssE* _M0L1xS665;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS662 = _M0L4selfS663->$5;
  _M0L8capacityS2625 = _M0L4selfS663->$2;
  _M0L13new__capacityS664 = _M0L8capacityS2625 << 1;
  _M0L6_2atmpS2619 = 0;
  _M0L6_2atmpS2618
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L13new__capacityS664, _M0L6_2atmpS2619);
  _M0L6_2aoldS3877 = _M0L4selfS663->$0;
  if (_M0L9old__headS662) {
    moonbit_incref(_M0L9old__headS662);
  }
  moonbit_decref(_M0L6_2aoldS3877);
  _M0L4selfS663->$0 = _M0L6_2atmpS2618;
  _M0L4selfS663->$2 = _M0L13new__capacityS664;
  _M0L6_2atmpS2620 = _M0L13new__capacityS664 - 1;
  _M0L4selfS663->$3 = _M0L6_2atmpS2620;
  _M0L8capacityS2622 = _M0L4selfS663->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2621 = _M0FPB21calc__grow__threshold(_M0L8capacityS2622);
  _M0L4selfS663->$4 = _M0L6_2atmpS2621;
  _M0L4selfS663->$1 = 0;
  _M0L6_2atmpS2623 = 0;
  _M0L6_2aoldS3876 = _M0L4selfS663->$5;
  if (_M0L6_2aoldS3876) {
    moonbit_decref(_M0L6_2aoldS3876);
  }
  _M0L4selfS663->$5 = _M0L6_2atmpS2623;
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
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2624 = 0;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3874 = _M0L4_2aeS668->$1;
      if (_M0L15next__in__chainS669) {
        moonbit_incref(_M0L15next__in__chainS669);
      }
      if (_M0L6_2aoldS3874) {
        moonbit_decref(_M0L6_2aoldS3874);
      }
      _M0L4_2aeS668->$1 = _M0L6_2atmpS2624;
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
  int32_t _M0L8capacityS2633;
  int32_t _M0L13new__capacityS672;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2627;
  struct _M0TPB5EntryGsbE** _M0L6_2atmpS2626;
  struct _M0TPB5EntryGsbE** _M0L6_2aoldS3882;
  int32_t _M0L6_2atmpS2628;
  int32_t _M0L8capacityS2630;
  int32_t _M0L6_2atmpS2629;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2631;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS3881;
  struct _M0TPB5EntryGsbE* _M0L1xS673;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS670 = _M0L4selfS671->$5;
  _M0L8capacityS2633 = _M0L4selfS671->$2;
  _M0L13new__capacityS672 = _M0L8capacityS2633 << 1;
  _M0L6_2atmpS2627 = 0;
  _M0L6_2atmpS2626
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L13new__capacityS672, _M0L6_2atmpS2627);
  _M0L6_2aoldS3882 = _M0L4selfS671->$0;
  if (_M0L9old__headS670) {
    moonbit_incref(_M0L9old__headS670);
  }
  moonbit_decref(_M0L6_2aoldS3882);
  _M0L4selfS671->$0 = _M0L6_2atmpS2626;
  _M0L4selfS671->$2 = _M0L13new__capacityS672;
  _M0L6_2atmpS2628 = _M0L13new__capacityS672 - 1;
  _M0L4selfS671->$3 = _M0L6_2atmpS2628;
  _M0L8capacityS2630 = _M0L4selfS671->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2629 = _M0FPB21calc__grow__threshold(_M0L8capacityS2630);
  _M0L4selfS671->$4 = _M0L6_2atmpS2629;
  _M0L4selfS671->$1 = 0;
  _M0L6_2atmpS2631 = 0;
  _M0L6_2aoldS3881 = _M0L4selfS671->$5;
  if (_M0L6_2aoldS3881) {
    moonbit_decref(_M0L6_2aoldS3881);
  }
  _M0L4selfS671->$5 = _M0L6_2atmpS2631;
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
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2632 = 0;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3879 = _M0L4_2aeS676->$1;
      if (_M0L15next__in__chainS677) {
        moonbit_incref(_M0L15next__in__chainS677);
      }
      if (_M0L6_2aoldS3879) {
        moonbit_decref(_M0L6_2aoldS3879);
      }
      _M0L4_2aeS676->$1 = _M0L6_2atmpS2632;
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
  int32_t _M0L8capacityS2641;
  int32_t _M0L13new__capacityS680;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2635;
  struct _M0TPB5EntryGsfE** _M0L6_2atmpS2634;
  struct _M0TPB5EntryGsfE** _M0L6_2aoldS3887;
  int32_t _M0L6_2atmpS2636;
  int32_t _M0L8capacityS2638;
  int32_t _M0L6_2atmpS2637;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2639;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS3886;
  struct _M0TPB5EntryGsfE* _M0L1xS681;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS678 = _M0L4selfS679->$5;
  _M0L8capacityS2641 = _M0L4selfS679->$2;
  _M0L13new__capacityS680 = _M0L8capacityS2641 << 1;
  _M0L6_2atmpS2635 = 0;
  _M0L6_2atmpS2634
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L13new__capacityS680, _M0L6_2atmpS2635);
  _M0L6_2aoldS3887 = _M0L4selfS679->$0;
  if (_M0L9old__headS678) {
    moonbit_incref(_M0L9old__headS678);
  }
  moonbit_decref(_M0L6_2aoldS3887);
  _M0L4selfS679->$0 = _M0L6_2atmpS2634;
  _M0L4selfS679->$2 = _M0L13new__capacityS680;
  _M0L6_2atmpS2636 = _M0L13new__capacityS680 - 1;
  _M0L4selfS679->$3 = _M0L6_2atmpS2636;
  _M0L8capacityS2638 = _M0L4selfS679->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2637 = _M0FPB21calc__grow__threshold(_M0L8capacityS2638);
  _M0L4selfS679->$4 = _M0L6_2atmpS2637;
  _M0L4selfS679->$1 = 0;
  _M0L6_2atmpS2639 = 0;
  _M0L6_2aoldS3886 = _M0L4selfS679->$5;
  if (_M0L6_2aoldS3886) {
    moonbit_decref(_M0L6_2aoldS3886);
  }
  _M0L4selfS679->$5 = _M0L6_2atmpS2639;
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
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2640 = 0;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3884 = _M0L4_2aeS684->$1;
      if (_M0L15next__in__chainS685) {
        moonbit_incref(_M0L15next__in__chainS685);
      }
      if (_M0L6_2aoldS3884) {
        moonbit_decref(_M0L6_2aoldS3884);
      }
      _M0L4_2aeS684->$1 = _M0L6_2atmpS2640;
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
  int32_t _M0L8capacityS2649;
  int32_t _M0L13new__capacityS688;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2643;
  struct _M0TPB5EntryGsiE** _M0L6_2atmpS2642;
  struct _M0TPB5EntryGsiE** _M0L6_2aoldS3892;
  int32_t _M0L6_2atmpS2644;
  int32_t _M0L8capacityS2646;
  int32_t _M0L6_2atmpS2645;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2647;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS3891;
  struct _M0TPB5EntryGsiE* _M0L1xS689;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS686 = _M0L4selfS687->$5;
  _M0L8capacityS2649 = _M0L4selfS687->$2;
  _M0L13new__capacityS688 = _M0L8capacityS2649 << 1;
  _M0L6_2atmpS2643 = 0;
  _M0L6_2atmpS2642
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L13new__capacityS688, _M0L6_2atmpS2643);
  _M0L6_2aoldS3892 = _M0L4selfS687->$0;
  if (_M0L9old__headS686) {
    moonbit_incref(_M0L9old__headS686);
  }
  moonbit_decref(_M0L6_2aoldS3892);
  _M0L4selfS687->$0 = _M0L6_2atmpS2642;
  _M0L4selfS687->$2 = _M0L13new__capacityS688;
  _M0L6_2atmpS2644 = _M0L13new__capacityS688 - 1;
  _M0L4selfS687->$3 = _M0L6_2atmpS2644;
  _M0L8capacityS2646 = _M0L4selfS687->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2645 = _M0FPB21calc__grow__threshold(_M0L8capacityS2646);
  _M0L4selfS687->$4 = _M0L6_2atmpS2645;
  _M0L4selfS687->$1 = 0;
  _M0L6_2atmpS2647 = 0;
  _M0L6_2aoldS3891 = _M0L4selfS687->$5;
  if (_M0L6_2aoldS3891) {
    moonbit_decref(_M0L6_2aoldS3891);
  }
  _M0L4selfS687->$5 = _M0L6_2atmpS2647;
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
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2648 = 0;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3889 = _M0L4_2aeS692->$1;
      if (_M0L15next__in__chainS693) {
        moonbit_incref(_M0L15next__in__chainS693);
      }
      if (_M0L6_2aoldS3889) {
        moonbit_decref(_M0L6_2aoldS3889);
      }
      _M0L4_2aeS692->$1 = _M0L6_2atmpS2648;
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

int32_t _M0MPB3Map20rehash__place__entryGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS614,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5outerS610
) {
  int32_t _M0L4hashS609;
  int32_t _M0L14capacity__maskS2569;
  int32_t _M0L6_2atmpS2568;
  int32_t _M0L3pslS611;
  int32_t _M0L3idxS612;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS609 = _M0L5outerS610->$3;
  _M0L14capacity__maskS2569 = _M0L4selfS614->$3;
  _M0L6_2atmpS2568 = _M0L4hashS609 & _M0L14capacity__maskS2569;
  _M0L3pslS611 = 0;
  _M0L3idxS612 = _M0L6_2atmpS2568;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2567 =
      _M0L4selfS614->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS613;
    if (
      _M0L3idxS612 < 0
      || _M0L3idxS612 >= Moonbit_array_length(_M0L7entriesS2567)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS613
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2567[
        _M0L3idxS612
      ];
    if (_M0L7_2abindS613 == 0) {
      int32_t _M0L4tailS2560;
      _M0L5outerS610->$2 = _M0L3pslS611;
      _M0L4tailS2560 = _M0L4selfS614->$6;
      _M0L5outerS610->$0 = _M0L4tailS2560;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(_M0L4selfS614, _M0L3idxS612, _M0L5outerS610);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS615 =
        _M0L7_2abindS613;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2acurrS616 =
        _M0L7_2aSomeS615;
      int32_t _M0L3pslS2561 = _M0L7_2acurrS616->$2;
      if (_M0L3pslS611 > _M0L3pslS2561) {
        int32_t _M0L4tailS2562;
        moonbit_incref(_M0L7_2acurrS616);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP19moonbitDB10RedisValueE(_M0L4selfS614, _M0L3idxS612, _M0L7_2acurrS616);
        moonbit_decref(_M0L7_2acurrS616);
        _M0L5outerS610->$2 = _M0L3pslS611;
        _M0L4tailS2562 = _M0L4selfS614->$6;
        _M0L5outerS610->$0 = _M0L4tailS2562;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(_M0L4selfS614, _M0L3idxS612, _M0L5outerS610);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2563 = _M0L3pslS611 + 1;
        int32_t _M0L6_2atmpS2565 = _M0L3idxS612 + 1;
        int32_t _M0L14capacity__maskS2566 = _M0L4selfS614->$3;
        int32_t _M0L6_2atmpS2564 =
          _M0L6_2atmpS2565 & _M0L14capacity__maskS2566;
        _M0L3pslS611 = _M0L6_2atmpS2563;
        _M0L3idxS612 = _M0L6_2atmpS2564;
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
  int32_t _M0L14capacity__maskS2579;
  int32_t _M0L6_2atmpS2578;
  int32_t _M0L3pslS620;
  int32_t _M0L3idxS621;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS618 = _M0L5outerS619->$3;
  _M0L14capacity__maskS2579 = _M0L4selfS623->$3;
  _M0L6_2atmpS2578 = _M0L4hashS618 & _M0L14capacity__maskS2579;
  _M0L3pslS620 = 0;
  _M0L3idxS621 = _M0L6_2atmpS2578;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2577 = _M0L4selfS623->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS622;
    if (
      _M0L3idxS621 < 0
      || _M0L3idxS621 >= Moonbit_array_length(_M0L7entriesS2577)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS622
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2577[_M0L3idxS621];
    if (_M0L7_2abindS622 == 0) {
      int32_t _M0L4tailS2570;
      _M0L5outerS619->$2 = _M0L3pslS620;
      _M0L4tailS2570 = _M0L4selfS623->$6;
      _M0L5outerS619->$0 = _M0L4tailS2570;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS623, _M0L3idxS621, _M0L5outerS619);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS624 = _M0L7_2abindS622;
      struct _M0TPB5EntryGssE* _M0L7_2acurrS625 = _M0L7_2aSomeS624;
      int32_t _M0L3pslS2571 = _M0L7_2acurrS625->$2;
      if (_M0L3pslS620 > _M0L3pslS2571) {
        int32_t _M0L4tailS2572;
        moonbit_incref(_M0L7_2acurrS625);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS623, _M0L3idxS621, _M0L7_2acurrS625);
        moonbit_decref(_M0L7_2acurrS625);
        _M0L5outerS619->$2 = _M0L3pslS620;
        _M0L4tailS2572 = _M0L4selfS623->$6;
        _M0L5outerS619->$0 = _M0L4tailS2572;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS623, _M0L3idxS621, _M0L5outerS619);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2573 = _M0L3pslS620 + 1;
        int32_t _M0L6_2atmpS2575 = _M0L3idxS621 + 1;
        int32_t _M0L14capacity__maskS2576 = _M0L4selfS623->$3;
        int32_t _M0L6_2atmpS2574 =
          _M0L6_2atmpS2575 & _M0L14capacity__maskS2576;
        _M0L3pslS620 = _M0L6_2atmpS2573;
        _M0L3idxS621 = _M0L6_2atmpS2574;
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
  int32_t _M0L14capacity__maskS2589;
  int32_t _M0L6_2atmpS2588;
  int32_t _M0L3pslS629;
  int32_t _M0L3idxS630;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS627 = _M0L5outerS628->$3;
  _M0L14capacity__maskS2589 = _M0L4selfS632->$3;
  _M0L6_2atmpS2588 = _M0L4hashS627 & _M0L14capacity__maskS2589;
  _M0L3pslS629 = 0;
  _M0L3idxS630 = _M0L6_2atmpS2588;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2587 = _M0L4selfS632->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS631;
    if (
      _M0L3idxS630 < 0
      || _M0L3idxS630 >= Moonbit_array_length(_M0L7entriesS2587)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS631
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2587[_M0L3idxS630];
    if (_M0L7_2abindS631 == 0) {
      int32_t _M0L4tailS2580;
      _M0L5outerS628->$2 = _M0L3pslS629;
      _M0L4tailS2580 = _M0L4selfS632->$6;
      _M0L5outerS628->$0 = _M0L4tailS2580;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS632, _M0L3idxS630, _M0L5outerS628);
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS633 = _M0L7_2abindS631;
      struct _M0TPB5EntryGsbE* _M0L7_2acurrS634 = _M0L7_2aSomeS633;
      int32_t _M0L3pslS2581 = _M0L7_2acurrS634->$2;
      if (_M0L3pslS629 > _M0L3pslS2581) {
        int32_t _M0L4tailS2582;
        moonbit_incref(_M0L7_2acurrS634);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsbE(_M0L4selfS632, _M0L3idxS630, _M0L7_2acurrS634);
        moonbit_decref(_M0L7_2acurrS634);
        _M0L5outerS628->$2 = _M0L3pslS629;
        _M0L4tailS2582 = _M0L4selfS632->$6;
        _M0L5outerS628->$0 = _M0L4tailS2582;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS632, _M0L3idxS630, _M0L5outerS628);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2583 = _M0L3pslS629 + 1;
        int32_t _M0L6_2atmpS2585 = _M0L3idxS630 + 1;
        int32_t _M0L14capacity__maskS2586 = _M0L4selfS632->$3;
        int32_t _M0L6_2atmpS2584 =
          _M0L6_2atmpS2585 & _M0L14capacity__maskS2586;
        _M0L3pslS629 = _M0L6_2atmpS2583;
        _M0L3idxS630 = _M0L6_2atmpS2584;
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
  int32_t _M0L14capacity__maskS2599;
  int32_t _M0L6_2atmpS2598;
  int32_t _M0L3pslS638;
  int32_t _M0L3idxS639;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS636 = _M0L5outerS637->$3;
  _M0L14capacity__maskS2599 = _M0L4selfS641->$3;
  _M0L6_2atmpS2598 = _M0L4hashS636 & _M0L14capacity__maskS2599;
  _M0L3pslS638 = 0;
  _M0L3idxS639 = _M0L6_2atmpS2598;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2597 = _M0L4selfS641->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS640;
    if (
      _M0L3idxS639 < 0
      || _M0L3idxS639 >= Moonbit_array_length(_M0L7entriesS2597)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS640
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2597[_M0L3idxS639];
    if (_M0L7_2abindS640 == 0) {
      int32_t _M0L4tailS2590;
      _M0L5outerS637->$2 = _M0L3pslS638;
      _M0L4tailS2590 = _M0L4selfS641->$6;
      _M0L5outerS637->$0 = _M0L4tailS2590;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS641, _M0L3idxS639, _M0L5outerS637);
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS642 = _M0L7_2abindS640;
      struct _M0TPB5EntryGsfE* _M0L7_2acurrS643 = _M0L7_2aSomeS642;
      int32_t _M0L3pslS2591 = _M0L7_2acurrS643->$2;
      if (_M0L3pslS638 > _M0L3pslS2591) {
        int32_t _M0L4tailS2592;
        moonbit_incref(_M0L7_2acurrS643);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsfE(_M0L4selfS641, _M0L3idxS639, _M0L7_2acurrS643);
        moonbit_decref(_M0L7_2acurrS643);
        _M0L5outerS637->$2 = _M0L3pslS638;
        _M0L4tailS2592 = _M0L4selfS641->$6;
        _M0L5outerS637->$0 = _M0L4tailS2592;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS641, _M0L3idxS639, _M0L5outerS637);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2593 = _M0L3pslS638 + 1;
        int32_t _M0L6_2atmpS2595 = _M0L3idxS639 + 1;
        int32_t _M0L14capacity__maskS2596 = _M0L4selfS641->$3;
        int32_t _M0L6_2atmpS2594 =
          _M0L6_2atmpS2595 & _M0L14capacity__maskS2596;
        _M0L3pslS638 = _M0L6_2atmpS2593;
        _M0L3idxS639 = _M0L6_2atmpS2594;
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
  int32_t _M0L14capacity__maskS2609;
  int32_t _M0L6_2atmpS2608;
  int32_t _M0L3pslS647;
  int32_t _M0L3idxS648;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS645 = _M0L5outerS646->$3;
  _M0L14capacity__maskS2609 = _M0L4selfS650->$3;
  _M0L6_2atmpS2608 = _M0L4hashS645 & _M0L14capacity__maskS2609;
  _M0L3pslS647 = 0;
  _M0L3idxS648 = _M0L6_2atmpS2608;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2607 = _M0L4selfS650->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS649;
    if (
      _M0L3idxS648 < 0
      || _M0L3idxS648 >= Moonbit_array_length(_M0L7entriesS2607)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS649
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2607[_M0L3idxS648];
    if (_M0L7_2abindS649 == 0) {
      int32_t _M0L4tailS2600;
      _M0L5outerS646->$2 = _M0L3pslS647;
      _M0L4tailS2600 = _M0L4selfS650->$6;
      _M0L5outerS646->$0 = _M0L4tailS2600;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS650, _M0L3idxS648, _M0L5outerS646);
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS651 = _M0L7_2abindS649;
      struct _M0TPB5EntryGsiE* _M0L7_2acurrS652 = _M0L7_2aSomeS651;
      int32_t _M0L3pslS2601 = _M0L7_2acurrS652->$2;
      if (_M0L3pslS647 > _M0L3pslS2601) {
        int32_t _M0L4tailS2602;
        moonbit_incref(_M0L7_2acurrS652);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsiE(_M0L4selfS650, _M0L3idxS648, _M0L7_2acurrS652);
        moonbit_decref(_M0L7_2acurrS652);
        _M0L5outerS646->$2 = _M0L3pslS647;
        _M0L4tailS2602 = _M0L4selfS650->$6;
        _M0L5outerS646->$0 = _M0L4tailS2602;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS650, _M0L3idxS648, _M0L5outerS646);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2603 = _M0L3pslS647 + 1;
        int32_t _M0L6_2atmpS2605 = _M0L3idxS648 + 1;
        int32_t _M0L14capacity__maskS2606 = _M0L4selfS650->$3;
        int32_t _M0L6_2atmpS2604 =
          _M0L6_2atmpS2605 & _M0L14capacity__maskS2606;
        _M0L3pslS647 = _M0L6_2atmpS2603;
        _M0L3idxS648 = _M0L6_2atmpS2604;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS563,
  int32_t _M0L3idxS568,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS567
) {
  int32_t _M0L3pslS2495;
  int32_t _M0L6_2atmpS2491;
  int32_t _M0L6_2atmpS2493;
  int32_t _M0L14capacity__maskS2494;
  int32_t _M0L6_2atmpS2492;
  int32_t _M0L3pslS559;
  int32_t _M0L3idxS560;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS561;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2495 = _M0L5entryS567->$2;
  _M0L6_2atmpS2491 = _M0L3pslS2495 + 1;
  _M0L6_2atmpS2493 = _M0L3idxS568 + 1;
  _M0L14capacity__maskS2494 = _M0L4selfS563->$3;
  _M0L6_2atmpS2492 = _M0L6_2atmpS2493 & _M0L14capacity__maskS2494;
  moonbit_incref(_M0L5entryS567);
  _M0L3pslS559 = _M0L6_2atmpS2491;
  _M0L3idxS560 = _M0L6_2atmpS2492;
  _M0L5entryS561 = _M0L5entryS567;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2490 =
      _M0L4selfS563->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS562;
    if (
      _M0L3idxS560 < 0
      || _M0L3idxS560 >= Moonbit_array_length(_M0L7entriesS2490)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS562
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2490[
        _M0L3idxS560
      ];
    if (_M0L7_2abindS562 == 0) {
      _M0L5entryS561->$2 = _M0L3pslS559;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS563, _M0L5entryS561, _M0L3idxS560);
      moonbit_decref(_M0L5entryS561);
      break;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS565 =
        _M0L7_2abindS562;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L14_2acurr__entryS566 =
        _M0L7_2aSomeS565;
      int32_t _M0L3pslS2480 = _M0L14_2acurr__entryS566->$2;
      if (_M0L3pslS559 > _M0L3pslS2480) {
        int32_t _M0L3pslS2485;
        int32_t _M0L6_2atmpS2481;
        int32_t _M0L6_2atmpS2483;
        int32_t _M0L14capacity__maskS2484;
        int32_t _M0L6_2atmpS2482;
        _M0L5entryS561->$2 = _M0L3pslS559;
        moonbit_incref(_M0L14_2acurr__entryS566);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS563, _M0L5entryS561, _M0L3idxS560);
        moonbit_decref(_M0L5entryS561);
        _M0L3pslS2485 = _M0L14_2acurr__entryS566->$2;
        _M0L6_2atmpS2481 = _M0L3pslS2485 + 1;
        _M0L6_2atmpS2483 = _M0L3idxS560 + 1;
        _M0L14capacity__maskS2484 = _M0L4selfS563->$3;
        _M0L6_2atmpS2482 = _M0L6_2atmpS2483 & _M0L14capacity__maskS2484;
        _M0L3pslS559 = _M0L6_2atmpS2481;
        _M0L3idxS560 = _M0L6_2atmpS2482;
        _M0L5entryS561 = _M0L14_2acurr__entryS566;
        continue;
      } else {
        int32_t _M0L6_2atmpS2486 = _M0L3pslS559 + 1;
        int32_t _M0L6_2atmpS2488 = _M0L3idxS560 + 1;
        int32_t _M0L14capacity__maskS2489 = _M0L4selfS563->$3;
        int32_t _M0L6_2atmpS2487 =
          _M0L6_2atmpS2488 & _M0L14capacity__maskS2489;
        struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _tmp_4311 =
          _M0L5entryS561;
        _M0L3pslS559 = _M0L6_2atmpS2486;
        _M0L3idxS560 = _M0L6_2atmpS2487;
        _M0L5entryS561 = _tmp_4311;
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
  int32_t _M0L3pslS2511;
  int32_t _M0L6_2atmpS2507;
  int32_t _M0L6_2atmpS2509;
  int32_t _M0L14capacity__maskS2510;
  int32_t _M0L6_2atmpS2508;
  int32_t _M0L3pslS569;
  int32_t _M0L3idxS570;
  struct _M0TPB5EntryGssE* _M0L5entryS571;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2511 = _M0L5entryS577->$2;
  _M0L6_2atmpS2507 = _M0L3pslS2511 + 1;
  _M0L6_2atmpS2509 = _M0L3idxS578 + 1;
  _M0L14capacity__maskS2510 = _M0L4selfS573->$3;
  _M0L6_2atmpS2508 = _M0L6_2atmpS2509 & _M0L14capacity__maskS2510;
  moonbit_incref(_M0L5entryS577);
  _M0L3pslS569 = _M0L6_2atmpS2507;
  _M0L3idxS570 = _M0L6_2atmpS2508;
  _M0L5entryS571 = _M0L5entryS577;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2506 = _M0L4selfS573->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS572;
    if (
      _M0L3idxS570 < 0
      || _M0L3idxS570 >= Moonbit_array_length(_M0L7entriesS2506)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS572
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2506[_M0L3idxS570];
    if (_M0L7_2abindS572 == 0) {
      _M0L5entryS571->$2 = _M0L3pslS569;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGssE(_M0L4selfS573, _M0L5entryS571, _M0L3idxS570);
      moonbit_decref(_M0L5entryS571);
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS575 = _M0L7_2abindS572;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS576 = _M0L7_2aSomeS575;
      int32_t _M0L3pslS2496 = _M0L14_2acurr__entryS576->$2;
      if (_M0L3pslS569 > _M0L3pslS2496) {
        int32_t _M0L3pslS2501;
        int32_t _M0L6_2atmpS2497;
        int32_t _M0L6_2atmpS2499;
        int32_t _M0L14capacity__maskS2500;
        int32_t _M0L6_2atmpS2498;
        _M0L5entryS571->$2 = _M0L3pslS569;
        moonbit_incref(_M0L14_2acurr__entryS576);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGssE(_M0L4selfS573, _M0L5entryS571, _M0L3idxS570);
        moonbit_decref(_M0L5entryS571);
        _M0L3pslS2501 = _M0L14_2acurr__entryS576->$2;
        _M0L6_2atmpS2497 = _M0L3pslS2501 + 1;
        _M0L6_2atmpS2499 = _M0L3idxS570 + 1;
        _M0L14capacity__maskS2500 = _M0L4selfS573->$3;
        _M0L6_2atmpS2498 = _M0L6_2atmpS2499 & _M0L14capacity__maskS2500;
        _M0L3pslS569 = _M0L6_2atmpS2497;
        _M0L3idxS570 = _M0L6_2atmpS2498;
        _M0L5entryS571 = _M0L14_2acurr__entryS576;
        continue;
      } else {
        int32_t _M0L6_2atmpS2502 = _M0L3pslS569 + 1;
        int32_t _M0L6_2atmpS2504 = _M0L3idxS570 + 1;
        int32_t _M0L14capacity__maskS2505 = _M0L4selfS573->$3;
        int32_t _M0L6_2atmpS2503 =
          _M0L6_2atmpS2504 & _M0L14capacity__maskS2505;
        struct _M0TPB5EntryGssE* _tmp_4313 = _M0L5entryS571;
        _M0L3pslS569 = _M0L6_2atmpS2502;
        _M0L3idxS570 = _M0L6_2atmpS2503;
        _M0L5entryS571 = _tmp_4313;
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
  int32_t _M0L3pslS2527;
  int32_t _M0L6_2atmpS2523;
  int32_t _M0L6_2atmpS2525;
  int32_t _M0L14capacity__maskS2526;
  int32_t _M0L6_2atmpS2524;
  int32_t _M0L3pslS579;
  int32_t _M0L3idxS580;
  struct _M0TPB5EntryGsbE* _M0L5entryS581;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2527 = _M0L5entryS587->$2;
  _M0L6_2atmpS2523 = _M0L3pslS2527 + 1;
  _M0L6_2atmpS2525 = _M0L3idxS588 + 1;
  _M0L14capacity__maskS2526 = _M0L4selfS583->$3;
  _M0L6_2atmpS2524 = _M0L6_2atmpS2525 & _M0L14capacity__maskS2526;
  moonbit_incref(_M0L5entryS587);
  _M0L3pslS579 = _M0L6_2atmpS2523;
  _M0L3idxS580 = _M0L6_2atmpS2524;
  _M0L5entryS581 = _M0L5entryS587;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2522 = _M0L4selfS583->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS582;
    if (
      _M0L3idxS580 < 0
      || _M0L3idxS580 >= Moonbit_array_length(_M0L7entriesS2522)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS582
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2522[_M0L3idxS580];
    if (_M0L7_2abindS582 == 0) {
      _M0L5entryS581->$2 = _M0L3pslS579;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsbE(_M0L4selfS583, _M0L5entryS581, _M0L3idxS580);
      moonbit_decref(_M0L5entryS581);
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS585 = _M0L7_2abindS582;
      struct _M0TPB5EntryGsbE* _M0L14_2acurr__entryS586 = _M0L7_2aSomeS585;
      int32_t _M0L3pslS2512 = _M0L14_2acurr__entryS586->$2;
      if (_M0L3pslS579 > _M0L3pslS2512) {
        int32_t _M0L3pslS2517;
        int32_t _M0L6_2atmpS2513;
        int32_t _M0L6_2atmpS2515;
        int32_t _M0L14capacity__maskS2516;
        int32_t _M0L6_2atmpS2514;
        _M0L5entryS581->$2 = _M0L3pslS579;
        moonbit_incref(_M0L14_2acurr__entryS586);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsbE(_M0L4selfS583, _M0L5entryS581, _M0L3idxS580);
        moonbit_decref(_M0L5entryS581);
        _M0L3pslS2517 = _M0L14_2acurr__entryS586->$2;
        _M0L6_2atmpS2513 = _M0L3pslS2517 + 1;
        _M0L6_2atmpS2515 = _M0L3idxS580 + 1;
        _M0L14capacity__maskS2516 = _M0L4selfS583->$3;
        _M0L6_2atmpS2514 = _M0L6_2atmpS2515 & _M0L14capacity__maskS2516;
        _M0L3pslS579 = _M0L6_2atmpS2513;
        _M0L3idxS580 = _M0L6_2atmpS2514;
        _M0L5entryS581 = _M0L14_2acurr__entryS586;
        continue;
      } else {
        int32_t _M0L6_2atmpS2518 = _M0L3pslS579 + 1;
        int32_t _M0L6_2atmpS2520 = _M0L3idxS580 + 1;
        int32_t _M0L14capacity__maskS2521 = _M0L4selfS583->$3;
        int32_t _M0L6_2atmpS2519 =
          _M0L6_2atmpS2520 & _M0L14capacity__maskS2521;
        struct _M0TPB5EntryGsbE* _tmp_4315 = _M0L5entryS581;
        _M0L3pslS579 = _M0L6_2atmpS2518;
        _M0L3idxS580 = _M0L6_2atmpS2519;
        _M0L5entryS581 = _tmp_4315;
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
  int32_t _M0L3pslS2543;
  int32_t _M0L6_2atmpS2539;
  int32_t _M0L6_2atmpS2541;
  int32_t _M0L14capacity__maskS2542;
  int32_t _M0L6_2atmpS2540;
  int32_t _M0L3pslS589;
  int32_t _M0L3idxS590;
  struct _M0TPB5EntryGsfE* _M0L5entryS591;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2543 = _M0L5entryS597->$2;
  _M0L6_2atmpS2539 = _M0L3pslS2543 + 1;
  _M0L6_2atmpS2541 = _M0L3idxS598 + 1;
  _M0L14capacity__maskS2542 = _M0L4selfS593->$3;
  _M0L6_2atmpS2540 = _M0L6_2atmpS2541 & _M0L14capacity__maskS2542;
  moonbit_incref(_M0L5entryS597);
  _M0L3pslS589 = _M0L6_2atmpS2539;
  _M0L3idxS590 = _M0L6_2atmpS2540;
  _M0L5entryS591 = _M0L5entryS597;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2538 = _M0L4selfS593->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS592;
    if (
      _M0L3idxS590 < 0
      || _M0L3idxS590 >= Moonbit_array_length(_M0L7entriesS2538)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS592
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2538[_M0L3idxS590];
    if (_M0L7_2abindS592 == 0) {
      _M0L5entryS591->$2 = _M0L3pslS589;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsfE(_M0L4selfS593, _M0L5entryS591, _M0L3idxS590);
      moonbit_decref(_M0L5entryS591);
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS595 = _M0L7_2abindS592;
      struct _M0TPB5EntryGsfE* _M0L14_2acurr__entryS596 = _M0L7_2aSomeS595;
      int32_t _M0L3pslS2528 = _M0L14_2acurr__entryS596->$2;
      if (_M0L3pslS589 > _M0L3pslS2528) {
        int32_t _M0L3pslS2533;
        int32_t _M0L6_2atmpS2529;
        int32_t _M0L6_2atmpS2531;
        int32_t _M0L14capacity__maskS2532;
        int32_t _M0L6_2atmpS2530;
        _M0L5entryS591->$2 = _M0L3pslS589;
        moonbit_incref(_M0L14_2acurr__entryS596);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsfE(_M0L4selfS593, _M0L5entryS591, _M0L3idxS590);
        moonbit_decref(_M0L5entryS591);
        _M0L3pslS2533 = _M0L14_2acurr__entryS596->$2;
        _M0L6_2atmpS2529 = _M0L3pslS2533 + 1;
        _M0L6_2atmpS2531 = _M0L3idxS590 + 1;
        _M0L14capacity__maskS2532 = _M0L4selfS593->$3;
        _M0L6_2atmpS2530 = _M0L6_2atmpS2531 & _M0L14capacity__maskS2532;
        _M0L3pslS589 = _M0L6_2atmpS2529;
        _M0L3idxS590 = _M0L6_2atmpS2530;
        _M0L5entryS591 = _M0L14_2acurr__entryS596;
        continue;
      } else {
        int32_t _M0L6_2atmpS2534 = _M0L3pslS589 + 1;
        int32_t _M0L6_2atmpS2536 = _M0L3idxS590 + 1;
        int32_t _M0L14capacity__maskS2537 = _M0L4selfS593->$3;
        int32_t _M0L6_2atmpS2535 =
          _M0L6_2atmpS2536 & _M0L14capacity__maskS2537;
        struct _M0TPB5EntryGsfE* _tmp_4317 = _M0L5entryS591;
        _M0L3pslS589 = _M0L6_2atmpS2534;
        _M0L3idxS590 = _M0L6_2atmpS2535;
        _M0L5entryS591 = _tmp_4317;
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
  int32_t _M0L3pslS2559;
  int32_t _M0L6_2atmpS2555;
  int32_t _M0L6_2atmpS2557;
  int32_t _M0L14capacity__maskS2558;
  int32_t _M0L6_2atmpS2556;
  int32_t _M0L3pslS599;
  int32_t _M0L3idxS600;
  struct _M0TPB5EntryGsiE* _M0L5entryS601;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2559 = _M0L5entryS607->$2;
  _M0L6_2atmpS2555 = _M0L3pslS2559 + 1;
  _M0L6_2atmpS2557 = _M0L3idxS608 + 1;
  _M0L14capacity__maskS2558 = _M0L4selfS603->$3;
  _M0L6_2atmpS2556 = _M0L6_2atmpS2557 & _M0L14capacity__maskS2558;
  moonbit_incref(_M0L5entryS607);
  _M0L3pslS599 = _M0L6_2atmpS2555;
  _M0L3idxS600 = _M0L6_2atmpS2556;
  _M0L5entryS601 = _M0L5entryS607;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2554 = _M0L4selfS603->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS602;
    if (
      _M0L3idxS600 < 0
      || _M0L3idxS600 >= Moonbit_array_length(_M0L7entriesS2554)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS602
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2554[_M0L3idxS600];
    if (_M0L7_2abindS602 == 0) {
      _M0L5entryS601->$2 = _M0L3pslS599;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsiE(_M0L4selfS603, _M0L5entryS601, _M0L3idxS600);
      moonbit_decref(_M0L5entryS601);
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS605 = _M0L7_2abindS602;
      struct _M0TPB5EntryGsiE* _M0L14_2acurr__entryS606 = _M0L7_2aSomeS605;
      int32_t _M0L3pslS2544 = _M0L14_2acurr__entryS606->$2;
      if (_M0L3pslS599 > _M0L3pslS2544) {
        int32_t _M0L3pslS2549;
        int32_t _M0L6_2atmpS2545;
        int32_t _M0L6_2atmpS2547;
        int32_t _M0L14capacity__maskS2548;
        int32_t _M0L6_2atmpS2546;
        _M0L5entryS601->$2 = _M0L3pslS599;
        moonbit_incref(_M0L14_2acurr__entryS606);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsiE(_M0L4selfS603, _M0L5entryS601, _M0L3idxS600);
        moonbit_decref(_M0L5entryS601);
        _M0L3pslS2549 = _M0L14_2acurr__entryS606->$2;
        _M0L6_2atmpS2545 = _M0L3pslS2549 + 1;
        _M0L6_2atmpS2547 = _M0L3idxS600 + 1;
        _M0L14capacity__maskS2548 = _M0L4selfS603->$3;
        _M0L6_2atmpS2546 = _M0L6_2atmpS2547 & _M0L14capacity__maskS2548;
        _M0L3pslS599 = _M0L6_2atmpS2545;
        _M0L3idxS600 = _M0L6_2atmpS2546;
        _M0L5entryS601 = _M0L14_2acurr__entryS606;
        continue;
      } else {
        int32_t _M0L6_2atmpS2550 = _M0L3pslS599 + 1;
        int32_t _M0L6_2atmpS2552 = _M0L3idxS600 + 1;
        int32_t _M0L14capacity__maskS2553 = _M0L4selfS603->$3;
        int32_t _M0L6_2atmpS2551 =
          _M0L6_2atmpS2552 & _M0L14capacity__maskS2553;
        struct _M0TPB5EntryGsiE* _tmp_4319 = _M0L5entryS601;
        _M0L3pslS599 = _M0L6_2atmpS2550;
        _M0L3idxS600 = _M0L6_2atmpS2551;
        _M0L5entryS601 = _tmp_4319;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS529,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS531,
  int32_t _M0L8new__idxS530
) {
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2470;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2471;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS3915;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS532;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2470 = _M0L4selfS529->$0;
  _M0L6_2atmpS2471 = _M0L5entryS531;
  if (
    _M0L8new__idxS530 < 0
    || _M0L8new__idxS530 >= Moonbit_array_length(_M0L7entriesS2470)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3915
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2470[
      _M0L8new__idxS530
    ];
  if (_M0L6_2atmpS2471) {
    moonbit_incref(_M0L6_2atmpS2471);
  }
  if (_M0L6_2aoldS3915) {
    moonbit_decref(_M0L6_2aoldS3915);
  }
  _M0L7entriesS2470[_M0L8new__idxS530] = _M0L6_2atmpS2471;
  _M0L7_2abindS532 = _M0L5entryS531->$1;
  if (_M0L7_2abindS532 == 0) {
    _M0L4selfS529->$6 = _M0L8new__idxS530;
  } else {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS533 =
      _M0L7_2abindS532;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2anextS534 =
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
  struct _M0TPB5EntryGsiE** _M0L7entriesS2472;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2473;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS3918;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS538;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2472 = _M0L4selfS535->$0;
  _M0L6_2atmpS2473 = _M0L5entryS537;
  if (
    _M0L8new__idxS536 < 0
    || _M0L8new__idxS536 >= Moonbit_array_length(_M0L7entriesS2472)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3918
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2472[_M0L8new__idxS536];
  if (_M0L6_2atmpS2473) {
    moonbit_incref(_M0L6_2atmpS2473);
  }
  if (_M0L6_2aoldS3918) {
    moonbit_decref(_M0L6_2aoldS3918);
  }
  _M0L7entriesS2472[_M0L8new__idxS536] = _M0L6_2atmpS2473;
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
  struct _M0TPB5EntryGssE** _M0L7entriesS2474;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2475;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3921;
  struct _M0TPB5EntryGssE* _M0L7_2abindS544;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2474 = _M0L4selfS541->$0;
  _M0L6_2atmpS2475 = _M0L5entryS543;
  if (
    _M0L8new__idxS542 < 0
    || _M0L8new__idxS542 >= Moonbit_array_length(_M0L7entriesS2474)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3921
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2474[_M0L8new__idxS542];
  if (_M0L6_2atmpS2475) {
    moonbit_incref(_M0L6_2atmpS2475);
  }
  if (_M0L6_2aoldS3921) {
    moonbit_decref(_M0L6_2aoldS3921);
  }
  _M0L7entriesS2474[_M0L8new__idxS542] = _M0L6_2atmpS2475;
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
  struct _M0TPB5EntryGsbE** _M0L7entriesS2476;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2477;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS3924;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS550;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2476 = _M0L4selfS547->$0;
  _M0L6_2atmpS2477 = _M0L5entryS549;
  if (
    _M0L8new__idxS548 < 0
    || _M0L8new__idxS548 >= Moonbit_array_length(_M0L7entriesS2476)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3924
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2476[_M0L8new__idxS548];
  if (_M0L6_2atmpS2477) {
    moonbit_incref(_M0L6_2atmpS2477);
  }
  if (_M0L6_2aoldS3924) {
    moonbit_decref(_M0L6_2aoldS3924);
  }
  _M0L7entriesS2476[_M0L8new__idxS548] = _M0L6_2atmpS2477;
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
  struct _M0TPB5EntryGsfE** _M0L7entriesS2478;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2479;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS3927;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS556;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2478 = _M0L4selfS553->$0;
  _M0L6_2atmpS2479 = _M0L5entryS555;
  if (
    _M0L8new__idxS554 < 0
    || _M0L8new__idxS554 >= Moonbit_array_length(_M0L7entriesS2478)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3927
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2478[_M0L8new__idxS554];
  if (_M0L6_2atmpS2479) {
    moonbit_incref(_M0L6_2atmpS2479);
  }
  if (_M0L6_2aoldS3927) {
    moonbit_decref(_M0L6_2aoldS3927);
  }
  _M0L7entriesS2478[_M0L8new__idxS554] = _M0L6_2atmpS2479;
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

int32_t _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS510,
  int32_t _M0L3idxS512,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS511
) {
  int32_t _M0L7_2abindS509;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2430;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2431;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS3929;
  int32_t _M0L4sizeS2433;
  int32_t _M0L6_2atmpS2432;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS509 = _M0L4selfS510->$6;
  switch (_M0L7_2abindS509) {
    case -1: {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2425 =
        _M0L5entryS511;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS3931 =
        _M0L4selfS510->$5;
      if (_M0L6_2atmpS2425) {
        moonbit_incref(_M0L6_2atmpS2425);
      }
      if (_M0L6_2aoldS3931) {
        moonbit_decref(_M0L6_2aoldS3931);
      }
      _M0L4selfS510->$5 = _M0L6_2atmpS2425;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2429 =
        _M0L4selfS510->$0;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2428;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2426;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2427;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS3932;
      if (
        _M0L7_2abindS509 < 0
        || _M0L7_2abindS509 >= Moonbit_array_length(_M0L7entriesS2429)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2428
      = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2429[
          _M0L7_2abindS509
        ];
      if (_M0L6_2atmpS2428) {
        moonbit_incref(_M0L6_2atmpS2428);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2426
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP19moonbitDB10RedisValueEE(_M0L6_2atmpS2428);
      if (_M0L6_2atmpS2428) {
        moonbit_decref(_M0L6_2atmpS2428);
      }
      _M0L6_2atmpS2427 = _M0L5entryS511;
      _M0L6_2aoldS3932 = _M0L6_2atmpS2426->$1;
      if (_M0L6_2atmpS2427) {
        moonbit_incref(_M0L6_2atmpS2427);
      }
      if (_M0L6_2aoldS3932) {
        moonbit_decref(_M0L6_2aoldS3932);
      }
      _M0L6_2atmpS2426->$1 = _M0L6_2atmpS2427;
      moonbit_decref(_M0L6_2atmpS2426);
      break;
    }
  }
  _M0L4selfS510->$6 = _M0L3idxS512;
  _M0L7entriesS2430 = _M0L4selfS510->$0;
  _M0L6_2atmpS2431 = _M0L5entryS511;
  if (
    _M0L3idxS512 < 0
    || _M0L3idxS512 >= Moonbit_array_length(_M0L7entriesS2430)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3929
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2430[
      _M0L3idxS512
    ];
  if (_M0L6_2atmpS2431) {
    moonbit_incref(_M0L6_2atmpS2431);
  }
  if (_M0L6_2aoldS3929) {
    moonbit_decref(_M0L6_2aoldS3929);
  }
  _M0L7entriesS2430[_M0L3idxS512] = _M0L6_2atmpS2431;
  _M0L4sizeS2433 = _M0L4selfS510->$1;
  _M0L6_2atmpS2432 = _M0L4sizeS2433 + 1;
  _M0L4selfS510->$1 = _M0L6_2atmpS2432;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGssE(
  struct _M0TPB3MapGssE* _M0L4selfS514,
  int32_t _M0L3idxS516,
  struct _M0TPB5EntryGssE* _M0L5entryS515
) {
  int32_t _M0L7_2abindS513;
  struct _M0TPB5EntryGssE** _M0L7entriesS2439;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2440;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3935;
  int32_t _M0L4sizeS2442;
  int32_t _M0L6_2atmpS2441;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS513 = _M0L4selfS514->$6;
  switch (_M0L7_2abindS513) {
    case -1: {
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2434 = _M0L5entryS515;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3937 = _M0L4selfS514->$5;
      if (_M0L6_2atmpS2434) {
        moonbit_incref(_M0L6_2atmpS2434);
      }
      if (_M0L6_2aoldS3937) {
        moonbit_decref(_M0L6_2aoldS3937);
      }
      _M0L4selfS514->$5 = _M0L6_2atmpS2434;
      break;
    }
    default: {
      struct _M0TPB5EntryGssE** _M0L7entriesS2438 = _M0L4selfS514->$0;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2437;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2435;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2436;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3938;
      if (
        _M0L7_2abindS513 < 0
        || _M0L7_2abindS513 >= Moonbit_array_length(_M0L7entriesS2438)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2437
      = (struct _M0TPB5EntryGssE*)_M0L7entriesS2438[_M0L7_2abindS513];
      if (_M0L6_2atmpS2437) {
        moonbit_incref(_M0L6_2atmpS2437);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2435
      = _M0MPC16option6Option6unwrapGRPB5EntryGssEE(_M0L6_2atmpS2437);
      if (_M0L6_2atmpS2437) {
        moonbit_decref(_M0L6_2atmpS2437);
      }
      _M0L6_2atmpS2436 = _M0L5entryS515;
      _M0L6_2aoldS3938 = _M0L6_2atmpS2435->$1;
      if (_M0L6_2atmpS2436) {
        moonbit_incref(_M0L6_2atmpS2436);
      }
      if (_M0L6_2aoldS3938) {
        moonbit_decref(_M0L6_2aoldS3938);
      }
      _M0L6_2atmpS2435->$1 = _M0L6_2atmpS2436;
      moonbit_decref(_M0L6_2atmpS2435);
      break;
    }
  }
  _M0L4selfS514->$6 = _M0L3idxS516;
  _M0L7entriesS2439 = _M0L4selfS514->$0;
  _M0L6_2atmpS2440 = _M0L5entryS515;
  if (
    _M0L3idxS516 < 0
    || _M0L3idxS516 >= Moonbit_array_length(_M0L7entriesS2439)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3935
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2439[_M0L3idxS516];
  if (_M0L6_2atmpS2440) {
    moonbit_incref(_M0L6_2atmpS2440);
  }
  if (_M0L6_2aoldS3935) {
    moonbit_decref(_M0L6_2aoldS3935);
  }
  _M0L7entriesS2439[_M0L3idxS516] = _M0L6_2atmpS2440;
  _M0L4sizeS2442 = _M0L4selfS514->$1;
  _M0L6_2atmpS2441 = _M0L4sizeS2442 + 1;
  _M0L4selfS514->$1 = _M0L6_2atmpS2441;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS518,
  int32_t _M0L3idxS520,
  struct _M0TPB5EntryGsbE* _M0L5entryS519
) {
  int32_t _M0L7_2abindS517;
  struct _M0TPB5EntryGsbE** _M0L7entriesS2448;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2449;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS3941;
  int32_t _M0L4sizeS2451;
  int32_t _M0L6_2atmpS2450;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS517 = _M0L4selfS518->$6;
  switch (_M0L7_2abindS517) {
    case -1: {
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2443 = _M0L5entryS519;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3943 = _M0L4selfS518->$5;
      if (_M0L6_2atmpS2443) {
        moonbit_incref(_M0L6_2atmpS2443);
      }
      if (_M0L6_2aoldS3943) {
        moonbit_decref(_M0L6_2aoldS3943);
      }
      _M0L4selfS518->$5 = _M0L6_2atmpS2443;
      break;
    }
    default: {
      struct _M0TPB5EntryGsbE** _M0L7entriesS2447 = _M0L4selfS518->$0;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2446;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2444;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2445;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS3944;
      if (
        _M0L7_2abindS517 < 0
        || _M0L7_2abindS517 >= Moonbit_array_length(_M0L7entriesS2447)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2446
      = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2447[_M0L7_2abindS517];
      if (_M0L6_2atmpS2446) {
        moonbit_incref(_M0L6_2atmpS2446);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2444
      = _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(_M0L6_2atmpS2446);
      if (_M0L6_2atmpS2446) {
        moonbit_decref(_M0L6_2atmpS2446);
      }
      _M0L6_2atmpS2445 = _M0L5entryS519;
      _M0L6_2aoldS3944 = _M0L6_2atmpS2444->$1;
      if (_M0L6_2atmpS2445) {
        moonbit_incref(_M0L6_2atmpS2445);
      }
      if (_M0L6_2aoldS3944) {
        moonbit_decref(_M0L6_2aoldS3944);
      }
      _M0L6_2atmpS2444->$1 = _M0L6_2atmpS2445;
      moonbit_decref(_M0L6_2atmpS2444);
      break;
    }
  }
  _M0L4selfS518->$6 = _M0L3idxS520;
  _M0L7entriesS2448 = _M0L4selfS518->$0;
  _M0L6_2atmpS2449 = _M0L5entryS519;
  if (
    _M0L3idxS520 < 0
    || _M0L3idxS520 >= Moonbit_array_length(_M0L7entriesS2448)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3941
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2448[_M0L3idxS520];
  if (_M0L6_2atmpS2449) {
    moonbit_incref(_M0L6_2atmpS2449);
  }
  if (_M0L6_2aoldS3941) {
    moonbit_decref(_M0L6_2aoldS3941);
  }
  _M0L7entriesS2448[_M0L3idxS520] = _M0L6_2atmpS2449;
  _M0L4sizeS2451 = _M0L4selfS518->$1;
  _M0L6_2atmpS2450 = _M0L4sizeS2451 + 1;
  _M0L4selfS518->$1 = _M0L6_2atmpS2450;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS522,
  int32_t _M0L3idxS524,
  struct _M0TPB5EntryGsfE* _M0L5entryS523
) {
  int32_t _M0L7_2abindS521;
  struct _M0TPB5EntryGsfE** _M0L7entriesS2457;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2458;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS3947;
  int32_t _M0L4sizeS2460;
  int32_t _M0L6_2atmpS2459;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS521 = _M0L4selfS522->$6;
  switch (_M0L7_2abindS521) {
    case -1: {
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2452 = _M0L5entryS523;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3949 = _M0L4selfS522->$5;
      if (_M0L6_2atmpS2452) {
        moonbit_incref(_M0L6_2atmpS2452);
      }
      if (_M0L6_2aoldS3949) {
        moonbit_decref(_M0L6_2aoldS3949);
      }
      _M0L4selfS522->$5 = _M0L6_2atmpS2452;
      break;
    }
    default: {
      struct _M0TPB5EntryGsfE** _M0L7entriesS2456 = _M0L4selfS522->$0;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2455;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2453;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2454;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS3950;
      if (
        _M0L7_2abindS521 < 0
        || _M0L7_2abindS521 >= Moonbit_array_length(_M0L7entriesS2456)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2455
      = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2456[_M0L7_2abindS521];
      if (_M0L6_2atmpS2455) {
        moonbit_incref(_M0L6_2atmpS2455);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2453
      = _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(_M0L6_2atmpS2455);
      if (_M0L6_2atmpS2455) {
        moonbit_decref(_M0L6_2atmpS2455);
      }
      _M0L6_2atmpS2454 = _M0L5entryS523;
      _M0L6_2aoldS3950 = _M0L6_2atmpS2453->$1;
      if (_M0L6_2atmpS2454) {
        moonbit_incref(_M0L6_2atmpS2454);
      }
      if (_M0L6_2aoldS3950) {
        moonbit_decref(_M0L6_2aoldS3950);
      }
      _M0L6_2atmpS2453->$1 = _M0L6_2atmpS2454;
      moonbit_decref(_M0L6_2atmpS2453);
      break;
    }
  }
  _M0L4selfS522->$6 = _M0L3idxS524;
  _M0L7entriesS2457 = _M0L4selfS522->$0;
  _M0L6_2atmpS2458 = _M0L5entryS523;
  if (
    _M0L3idxS524 < 0
    || _M0L3idxS524 >= Moonbit_array_length(_M0L7entriesS2457)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3947
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2457[_M0L3idxS524];
  if (_M0L6_2atmpS2458) {
    moonbit_incref(_M0L6_2atmpS2458);
  }
  if (_M0L6_2aoldS3947) {
    moonbit_decref(_M0L6_2aoldS3947);
  }
  _M0L7entriesS2457[_M0L3idxS524] = _M0L6_2atmpS2458;
  _M0L4sizeS2460 = _M0L4selfS522->$1;
  _M0L6_2atmpS2459 = _M0L4sizeS2460 + 1;
  _M0L4selfS522->$1 = _M0L6_2atmpS2459;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS526,
  int32_t _M0L3idxS528,
  struct _M0TPB5EntryGsiE* _M0L5entryS527
) {
  int32_t _M0L7_2abindS525;
  struct _M0TPB5EntryGsiE** _M0L7entriesS2466;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2467;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS3953;
  int32_t _M0L4sizeS2469;
  int32_t _M0L6_2atmpS2468;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS525 = _M0L4selfS526->$6;
  switch (_M0L7_2abindS525) {
    case -1: {
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2461 = _M0L5entryS527;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3955 = _M0L4selfS526->$5;
      if (_M0L6_2atmpS2461) {
        moonbit_incref(_M0L6_2atmpS2461);
      }
      if (_M0L6_2aoldS3955) {
        moonbit_decref(_M0L6_2aoldS3955);
      }
      _M0L4selfS526->$5 = _M0L6_2atmpS2461;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS2465 = _M0L4selfS526->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2464;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2462;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2463;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS3956;
      if (
        _M0L7_2abindS525 < 0
        || _M0L7_2abindS525 >= Moonbit_array_length(_M0L7entriesS2465)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2464
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2465[_M0L7_2abindS525];
      if (_M0L6_2atmpS2464) {
        moonbit_incref(_M0L6_2atmpS2464);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2462
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS2464);
      if (_M0L6_2atmpS2464) {
        moonbit_decref(_M0L6_2atmpS2464);
      }
      _M0L6_2atmpS2463 = _M0L5entryS527;
      _M0L6_2aoldS3956 = _M0L6_2atmpS2462->$1;
      if (_M0L6_2atmpS2463) {
        moonbit_incref(_M0L6_2atmpS2463);
      }
      if (_M0L6_2aoldS3956) {
        moonbit_decref(_M0L6_2aoldS3956);
      }
      _M0L6_2atmpS2462->$1 = _M0L6_2atmpS2463;
      moonbit_decref(_M0L6_2atmpS2462);
      break;
    }
  }
  _M0L4selfS526->$6 = _M0L3idxS528;
  _M0L7entriesS2466 = _M0L4selfS526->$0;
  _M0L6_2atmpS2467 = _M0L5entryS527;
  if (
    _M0L3idxS528 < 0
    || _M0L3idxS528 >= Moonbit_array_length(_M0L7entriesS2466)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3953
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2466[_M0L3idxS528];
  if (_M0L6_2atmpS2467) {
    moonbit_incref(_M0L6_2atmpS2467);
  }
  if (_M0L6_2aoldS3953) {
    moonbit_decref(_M0L6_2aoldS3953);
  }
  _M0L7entriesS2466[_M0L3idxS528] = _M0L6_2atmpS2467;
  _M0L4sizeS2469 = _M0L4selfS526->$1;
  _M0L6_2atmpS2468 = _M0L4sizeS2469 + 1;
  _M0L4selfS526->$1 = _M0L6_2atmpS2468;
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
  int32_t _M0L6_2atmpS2423;
  int32_t _M0L6_2atmpS2422;
  #line 71 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 72 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS505 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS506);
  _M0L6_2atmpS2423 = _M0Lm8capacityS505;
  #line 73 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2422 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2423);
  if (_M0L6lengthS506 > _M0L6_2atmpS2422) {
    int32_t _M0L6_2atmpS2424 = _M0Lm8capacityS505;
    _M0Lm8capacityS505 = _M0L6_2atmpS2424 * 2;
  }
  return _M0Lm8capacityS505;
}

struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0FPB8new__mapGsRP19moonbitDB10RedisValueE(
  int32_t _M0L8capacityS476
) {
  int32_t _M0L8capacityS475;
  int32_t _M0L7_2abindS477;
  int32_t _M0L7_2abindS478;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2417;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7_2abindS479;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS480;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _block_4320;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS475
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS476);
  _M0L7_2abindS477 = _M0L8capacityS475 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS478 = _M0FPB21calc__grow__threshold(_M0L8capacityS475);
  _M0L6_2atmpS2417 = 0;
  _M0L7_2abindS479
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE**)moonbit_make_ref_array(_M0L8capacityS475, _M0L6_2atmpS2417);
  _M0L7_2abindS480 = 0;
  _block_4320
  = (struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRP19moonbitDB10RedisValueE));
  Moonbit_object_header(_block_4320)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 96, 0);
  _block_4320->$0 = _M0L7_2abindS479;
  _block_4320->$1 = 0;
  _block_4320->$2 = _M0L8capacityS475;
  _block_4320->$3 = _M0L7_2abindS477;
  _block_4320->$4 = _M0L7_2abindS478;
  _block_4320->$5 = _M0L7_2abindS480;
  _block_4320->$6 = -1;
  return _block_4320;
}

struct _M0TPB3MapGsiE* _M0FPB8new__mapGsiE(int32_t _M0L8capacityS482) {
  int32_t _M0L8capacityS481;
  int32_t _M0L7_2abindS483;
  int32_t _M0L7_2abindS484;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2418;
  struct _M0TPB5EntryGsiE** _M0L7_2abindS485;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS486;
  struct _M0TPB3MapGsiE* _block_4321;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS481
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS482);
  _M0L7_2abindS483 = _M0L8capacityS481 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS484 = _M0FPB21calc__grow__threshold(_M0L8capacityS481);
  _M0L6_2atmpS2418 = 0;
  _M0L7_2abindS485
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L8capacityS481, _M0L6_2atmpS2418);
  _M0L7_2abindS486 = 0;
  _block_4321
  = (struct _M0TPB3MapGsiE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsiE));
  Moonbit_object_header(_block_4321)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 100, 0);
  _block_4321->$0 = _M0L7_2abindS485;
  _block_4321->$1 = 0;
  _block_4321->$2 = _M0L8capacityS481;
  _block_4321->$3 = _M0L7_2abindS483;
  _block_4321->$4 = _M0L7_2abindS484;
  _block_4321->$5 = _M0L7_2abindS486;
  _block_4321->$6 = -1;
  return _block_4321;
}

struct _M0TPB3MapGssE* _M0FPB8new__mapGssE(int32_t _M0L8capacityS488) {
  int32_t _M0L8capacityS487;
  int32_t _M0L7_2abindS489;
  int32_t _M0L7_2abindS490;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2419;
  struct _M0TPB5EntryGssE** _M0L7_2abindS491;
  struct _M0TPB5EntryGssE* _M0L7_2abindS492;
  struct _M0TPB3MapGssE* _block_4322;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS487
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS488);
  _M0L7_2abindS489 = _M0L8capacityS487 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS490 = _M0FPB21calc__grow__threshold(_M0L8capacityS487);
  _M0L6_2atmpS2419 = 0;
  _M0L7_2abindS491
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L8capacityS487, _M0L6_2atmpS2419);
  _M0L7_2abindS492 = 0;
  _block_4322
  = (struct _M0TPB3MapGssE*)moonbit_malloc(sizeof(struct _M0TPB3MapGssE));
  Moonbit_object_header(_block_4322)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 104, 0);
  _block_4322->$0 = _M0L7_2abindS491;
  _block_4322->$1 = 0;
  _block_4322->$2 = _M0L8capacityS487;
  _block_4322->$3 = _M0L7_2abindS489;
  _block_4322->$4 = _M0L7_2abindS490;
  _block_4322->$5 = _M0L7_2abindS492;
  _block_4322->$6 = -1;
  return _block_4322;
}

struct _M0TPB3MapGsbE* _M0FPB8new__mapGsbE(int32_t _M0L8capacityS494) {
  int32_t _M0L8capacityS493;
  int32_t _M0L7_2abindS495;
  int32_t _M0L7_2abindS496;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2420;
  struct _M0TPB5EntryGsbE** _M0L7_2abindS497;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS498;
  struct _M0TPB3MapGsbE* _block_4323;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS493
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS494);
  _M0L7_2abindS495 = _M0L8capacityS493 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS496 = _M0FPB21calc__grow__threshold(_M0L8capacityS493);
  _M0L6_2atmpS2420 = 0;
  _M0L7_2abindS497
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L8capacityS493, _M0L6_2atmpS2420);
  _M0L7_2abindS498 = 0;
  _block_4323
  = (struct _M0TPB3MapGsbE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsbE));
  Moonbit_object_header(_block_4323)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 108, 0);
  _block_4323->$0 = _M0L7_2abindS497;
  _block_4323->$1 = 0;
  _block_4323->$2 = _M0L8capacityS493;
  _block_4323->$3 = _M0L7_2abindS495;
  _block_4323->$4 = _M0L7_2abindS496;
  _block_4323->$5 = _M0L7_2abindS498;
  _block_4323->$6 = -1;
  return _block_4323;
}

struct _M0TPB3MapGsfE* _M0FPB8new__mapGsfE(int32_t _M0L8capacityS500) {
  int32_t _M0L8capacityS499;
  int32_t _M0L7_2abindS501;
  int32_t _M0L7_2abindS502;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2421;
  struct _M0TPB5EntryGsfE** _M0L7_2abindS503;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS504;
  struct _M0TPB3MapGsfE* _block_4324;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS499
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS500);
  _M0L7_2abindS501 = _M0L8capacityS499 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS502 = _M0FPB21calc__grow__threshold(_M0L8capacityS499);
  _M0L6_2atmpS2421 = 0;
  _M0L7_2abindS503
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L8capacityS499, _M0L6_2atmpS2421);
  _M0L7_2abindS504 = 0;
  _block_4324
  = (struct _M0TPB3MapGsfE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsfE));
  Moonbit_object_header(_block_4324)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 112, 0);
  _block_4324->$0 = _M0L7_2abindS503;
  _block_4324->$1 = 0;
  _block_4324->$2 = _M0L8capacityS499;
  _block_4324->$3 = _M0L7_2abindS501;
  _block_4324->$4 = _M0L7_2abindS502;
  _block_4324->$5 = _M0L7_2abindS504;
  _block_4324->$6 = -1;
  return _block_4324;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS474) {
  #line 33 "/home/developer/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS474 >= 0) {
    int32_t _M0L6_2atmpS2416;
    int32_t _M0L6_2atmpS2415;
    int32_t _M0L6_2atmpS2414;
    int32_t _M0L6_2atmpS2413;
    if (_M0L4selfS474 <= 1) {
      return 1;
    }
    if (_M0L4selfS474 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2416 = _M0L4selfS474 - 1;
    #line 44 "/home/developer/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS2415 = moonbit_clz32(_M0L6_2atmpS2416);
    _M0L6_2atmpS2414 = _M0L6_2atmpS2415 - 1;
    _M0L6_2atmpS2413 = 2147483647 >> (_M0L6_2atmpS2414 & 31);
    return _M0L6_2atmpS2413 + 1;
  } else {
    #line 34 "/home/developer/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS473) {
  int32_t _M0L6_2atmpS2412;
  #line 610 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2412 = _M0L8capacityS473 * 13;
  return _M0L6_2atmpS2412 / 16;
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

struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP19moonbitDB10RedisValueEE(
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4selfS463
) {
  #line 38 "/home/developer/.moon/lib/core/builtin/option.mbt"
  if (_M0L4selfS463 == 0) {
    #line 40 "/home/developer/.moon/lib/core/builtin/option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS464 =
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
  int32_t _M0L3endS2387;
  int32_t _M0L5startS2388;
  int32_t _M0L6_2atmpS2386;
  #line 1497 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS2387 = _M0L4selfS435.$2;
  _M0L5startS2388 = _M0L4selfS435.$1;
  _M0L6_2atmpS2386 = _M0L3endS2387 - _M0L5startS2388;
  if (_M0L6_2atmpS2386 == 0) {
    return (moonbit_string_t)moonbit_string_literal_95.data;
  } else {
    moonbit_string_t* _M0L3bufS2410 = _M0L4selfS435.$0;
    int32_t _M0L5startS2411 = _M0L4selfS435.$1;
    moonbit_string_t _M0L5_2ahdS436 =
      (moonbit_string_t)_M0L3bufS2410[_M0L5startS2411];
    moonbit_string_t* _M0L9_2ax__bufS437 = _M0L4selfS435.$0;
    int32_t _M0L5startS2409 = _M0L4selfS435.$1;
    int32_t _M0L11_2ax__startS438 = 1 + _M0L5startS2409;
    int32_t _M0L9_2ax__endS439 = _M0L4selfS435.$2;
    struct _M0TPC16string10StringView _M0L2hdS440;
    int32_t _M0L7_2abindS441;
    int32_t _M0L3endS2407;
    int32_t _M0L5startS2408;
    int32_t _M0L6_2atmpS2406;
    int32_t _M0L10size__hintS442;
    int32_t _M0L2__S443;
    int32_t _M0L10size__hintS444;
    int32_t _M0L10size__hintS449;
    struct _M0TPB13StringBuilder* _M0L3bufS450;
    int32_t _M0L3endS2390;
    int32_t _M0L5startS2391;
    int32_t _M0L6_2atmpS2389;
    moonbit_string_t _result_4328;
    moonbit_incref(_M0L9_2ax__bufS437);
    moonbit_incref(_M0L5_2ahdS436);
    #line 1504 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0L2hdS440
    = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L5_2ahdS436);
    moonbit_decref(_M0L5_2ahdS436);
    _M0L7_2abindS441 = _M0L9_2ax__endS439 - _M0L11_2ax__startS438;
    _M0L3endS2407 = _M0L2hdS440.$2;
    _M0L5startS2408 = _M0L2hdS440.$1;
    _M0L6_2atmpS2406 = _M0L3endS2407 - _M0L5startS2408;
    _M0L2__S443 = 0;
    _M0L10size__hintS444 = _M0L6_2atmpS2406;
    while (1) {
      if (_M0L2__S443 < _M0L7_2abindS441) {
        int32_t _M0L6_2atmpS2405 = _M0L11_2ax__startS438 + _M0L2__S443;
        moonbit_string_t _M0L1sS445 =
          (moonbit_string_t)_M0L9_2ax__bufS437[_M0L6_2atmpS2405];
        int32_t _M0L6_2atmpS2396 = _M0L2__S443 + 1;
        struct _M0TPC16string10StringView _M0L7_2abindS447;
        int32_t _M0L3endS2403;
        int32_t _M0L5startS2404;
        int32_t _M0L6_2atmpS2402;
        int32_t _M0L6_2atmpS2398;
        int32_t _M0L3endS2400;
        int32_t _M0L5startS2401;
        int32_t _M0L6_2atmpS2399;
        int32_t _M0L6_2atmpS2397;
        moonbit_incref(_M0L1sS445);
        #line 1506 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
        _M0L7_2abindS447
        = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS445);
        moonbit_decref(_M0L1sS445);
        _M0L3endS2403 = _M0L7_2abindS447.$2;
        _M0L5startS2404 = _M0L7_2abindS447.$1;
        moonbit_decref(_M0L7_2abindS447.$0);
        _M0L6_2atmpS2402 = _M0L3endS2403 - _M0L5startS2404;
        _M0L6_2atmpS2398 = _M0L10size__hintS444 + _M0L6_2atmpS2402;
        _M0L3endS2400 = _M0L9separatorS448.$2;
        _M0L5startS2401 = _M0L9separatorS448.$1;
        _M0L6_2atmpS2399 = _M0L3endS2400 - _M0L5startS2401;
        _M0L6_2atmpS2397 = _M0L6_2atmpS2398 + _M0L6_2atmpS2399;
        _M0L2__S443 = _M0L6_2atmpS2396;
        _M0L10size__hintS444 = _M0L6_2atmpS2397;
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
    _M0L3endS2390 = _M0L9separatorS448.$2;
    _M0L5startS2391 = _M0L9separatorS448.$1;
    _M0L6_2atmpS2389 = _M0L3endS2390 - _M0L5startS2391;
    if (_M0L6_2atmpS2389 == 0) {
      int32_t _M0L7_2abindS451 = _M0L9_2ax__endS439 - _M0L11_2ax__startS438;
      int32_t _M0L2__S452 = 0;
      while (1) {
        if (_M0L2__S452 < _M0L7_2abindS451) {
          int32_t _M0L6_2atmpS2393 = _M0L11_2ax__startS438 + _M0L2__S452;
          moonbit_string_t _M0L1sS453 =
            (moonbit_string_t)_M0L9_2ax__bufS437[_M0L6_2atmpS2393];
          struct _M0TPC16string10StringView _M0L1sS454;
          int32_t _M0L6_2atmpS2392;
          moonbit_incref(_M0L1sS453);
          #line 1517 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS454
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS453);
          moonbit_decref(_M0L1sS453);
          #line 1518 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS450, _M0L1sS454);
          moonbit_decref(_M0L1sS454.$0);
          _M0L6_2atmpS2392 = _M0L2__S452 + 1;
          _M0L2__S452 = _M0L6_2atmpS2392;
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
          int32_t _M0L6_2atmpS2395 = _M0L11_2ax__startS438 + _M0L2__S457;
          moonbit_string_t _M0L1sS458 =
            (moonbit_string_t)_M0L9_2ax__bufS437[_M0L6_2atmpS2395];
          struct _M0TPC16string10StringView _M0L1sS459;
          int32_t _M0L6_2atmpS2394;
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
          _M0L6_2atmpS2394 = _M0L2__S457 + 1;
          _M0L2__S457 = _M0L6_2atmpS2394;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS437);
        }
        break;
      }
    }
    #line 1528 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _result_4328 = _M0MPB13StringBuilder10to__string(_M0L3bufS450);
    moonbit_decref(_M0L3bufS450);
    return _result_4328;
  }
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS431,
  int32_t _M0L5indexS432
) {
  uint64_t* _M0L6_2atmpS2384;
  uint64_t _result_4329;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS431);
  _M0L6_2atmpS2384 = _M0L4selfS431;
  if (
    _M0L5indexS432 < 0
    || _M0L5indexS432 >= Moonbit_array_length(_M0L6_2atmpS2384)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_4329 = (uint64_t)_M0L6_2atmpS2384[_M0L5indexS432];
  moonbit_decref(_M0L6_2atmpS2384);
  return _result_4329;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS433,
  int32_t _M0L5indexS434
) {
  uint32_t* _M0L6_2atmpS2385;
  uint32_t _result_4330;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS433);
  _M0L6_2atmpS2385 = _M0L4selfS433;
  if (
    _M0L5indexS434 < 0
    || _M0L5indexS434 >= Moonbit_array_length(_M0L6_2atmpS2385)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_4330 = (uint32_t)_M0L6_2atmpS2385[_M0L5indexS434];
  moonbit_decref(_M0L6_2atmpS2385);
  return _result_4330;
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
    return (moonbit_string_t)moonbit_string_literal_106.data;
  } else {
    return (moonbit_string_t)moonbit_string_literal_107.data;
  }
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS427) {
  #line 2494 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  return (uint64_t)_M0L4selfS427;
}

struct _M0TPC16string10StringView _M0IPC16string6StringPB12ToStringView16to__string__view(
  moonbit_string_t _M0L4selfS426
) {
  int32_t _M0L6_2atmpS2383;
  #line 24 "/home/developer/.moon/lib/core/builtin/string_like.mbt"
  _M0L6_2atmpS2383 = Moonbit_array_length(_M0L4selfS426);
  moonbit_incref(_M0L4selfS426);
  return (struct _M0TPC16string10StringView){.$0 = _M0L4selfS426,
                                               .$1 = 0,
                                               .$2 = _M0L6_2atmpS2383};
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS417,
  moonbit_string_t _M0L5valueS419
) {
  int32_t _M0L3lenS2368;
  moonbit_string_t* _M0L6_2atmpS2370;
  int32_t _M0L6_2atmpS2369;
  int32_t _M0L6lengthS418;
  moonbit_string_t* _M0L3bufS2371;
  moonbit_string_t _M0L6_2aoldS3965;
  int32_t _M0L6_2atmpS2372;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2368 = _M0L4selfS417->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2370 = _M0MPC15array5Array6bufferGsE(_M0L4selfS417);
  _M0L6_2atmpS2369 = Moonbit_array_length(_M0L6_2atmpS2370);
  moonbit_decref(_M0L6_2atmpS2370);
  if (_M0L3lenS2368 == _M0L6_2atmpS2369) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS417);
  }
  _M0L6lengthS418 = _M0L4selfS417->$1;
  _M0L3bufS2371 = _M0L4selfS417->$0;
  _M0L6_2aoldS3965 = (moonbit_string_t)_M0L3bufS2371[_M0L6lengthS418];
  moonbit_incref(_M0L5valueS419);
  moonbit_decref(_M0L6_2aoldS3965);
  _M0L3bufS2371[_M0L6lengthS418] = _M0L5valueS419;
  _M0L6_2atmpS2372 = _M0L6lengthS418 + 1;
  _M0L4selfS417->$1 = _M0L6_2atmpS2372;
  return 0;
}

int32_t _M0MPC15array5Array4pushGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS420,
  moonbit_string_t _M0L5valueS422
) {
  int32_t _M0L3lenS2373;
  moonbit_string_t* _M0L6_2atmpS2375;
  int32_t _M0L6_2atmpS2374;
  int32_t _M0L6lengthS421;
  moonbit_string_t* _M0L3bufS2376;
  moonbit_string_t _M0L6_2aoldS3967;
  int32_t _M0L6_2atmpS2377;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2373 = _M0L4selfS420->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2375 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS420);
  _M0L6_2atmpS2374 = Moonbit_array_length(_M0L6_2atmpS2375);
  moonbit_decref(_M0L6_2atmpS2375);
  if (_M0L3lenS2373 == _M0L6_2atmpS2374) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGOsE(_M0L4selfS420);
  }
  _M0L6lengthS421 = _M0L4selfS420->$1;
  _M0L3bufS2376 = _M0L4selfS420->$0;
  _M0L6_2aoldS3967 = (moonbit_string_t)_M0L3bufS2376[_M0L6lengthS421];
  if (_M0L5valueS422) {
    moonbit_incref(_M0L5valueS422);
  }
  if (_M0L6_2aoldS3967) {
    moonbit_decref(_M0L6_2aoldS3967);
  }
  _M0L3bufS2376[_M0L6lengthS421] = _M0L5valueS422;
  _M0L6_2atmpS2377 = _M0L6lengthS421 + 1;
  _M0L4selfS420->$1 = _M0L6_2atmpS2377;
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS423,
  struct _M0TUsfE* _M0L5valueS425
) {
  int32_t _M0L3lenS2378;
  struct _M0TUsfE** _M0L6_2atmpS2380;
  int32_t _M0L6_2atmpS2379;
  int32_t _M0L6lengthS424;
  struct _M0TUsfE** _M0L3bufS2381;
  struct _M0TUsfE* _M0L6_2aoldS3969;
  int32_t _M0L6_2atmpS2382;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2378 = _M0L4selfS423->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2380 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS423);
  _M0L6_2atmpS2379 = Moonbit_array_length(_M0L6_2atmpS2380);
  moonbit_decref(_M0L6_2atmpS2380);
  if (_M0L3lenS2378 == _M0L6_2atmpS2379) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsfEE(_M0L4selfS423);
  }
  _M0L6lengthS424 = _M0L4selfS423->$1;
  _M0L3bufS2381 = _M0L4selfS423->$0;
  _M0L6_2aoldS3969 = (struct _M0TUsfE*)_M0L3bufS2381[_M0L6lengthS424];
  moonbit_incref(_M0L5valueS425);
  if (_M0L6_2aoldS3969) {
    moonbit_decref(_M0L6_2aoldS3969);
  }
  _M0L3bufS2381[_M0L6lengthS424] = _M0L5valueS425;
  _M0L6_2atmpS2382 = _M0L6lengthS424 + 1;
  _M0L4selfS423->$1 = _M0L6_2atmpS2382;
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
  moonbit_string_t* _M0L6_2aoldS3971;
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
  _M0L6_2aoldS3971 = _M0L4selfS391->$0;
  moonbit_decref(_M0L6_2aoldS3971);
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
  moonbit_string_t* _M0L6_2aoldS3973;
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
  _M0L6_2aoldS3973 = _M0L4selfS397->$0;
  moonbit_decref(_M0L6_2aoldS3973);
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
  struct _M0TUsfE** _M0L6_2aoldS3975;
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
  _M0L6_2aoldS3975 = _M0L4selfS403->$0;
  moonbit_decref(_M0L6_2aoldS3975);
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
  int32_t _M0L3endS2366;
  int32_t _M0L5startS2367;
  int32_t _M0L8str__lenS383;
  int32_t _M0L3lenS2359;
  int32_t _M0L6_2atmpS2358;
  uint16_t* _M0L4dataS2360;
  int32_t _M0L3lenS2361;
  moonbit_string_t _M0L6_2atmpS2362;
  int32_t _M0L6_2atmpS2363;
  int32_t _M0L3lenS2365;
  int32_t _M0L6_2atmpS2364;
  #line 131 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3endS2366 = _M0L3strS384.$2;
  _M0L5startS2367 = _M0L3strS384.$1;
  _M0L8str__lenS383 = _M0L3endS2366 - _M0L5startS2367;
  _M0L3lenS2359 = _M0L4selfS385->$1;
  _M0L6_2atmpS2358 = _M0L3lenS2359 + _M0L8str__lenS383;
  #line 136 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS385, _M0L6_2atmpS2358);
  _M0L4dataS2360 = _M0L4selfS385->$0;
  _M0L3lenS2361 = _M0L4selfS385->$1;
  moonbit_incref(_M0L4dataS2360);
  #line 139 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2362 = _M0MPC16string10StringView4data(_M0L3strS384);
  #line 140 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2363 = _M0MPC16string10StringView13start__offset(_M0L3strS384);
  #line 137 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2360, _M0L3lenS2361, _M0L6_2atmpS2362, _M0L6_2atmpS2363, _M0L8str__lenS383);
  moonbit_decref(_M0L4dataS2360);
  moonbit_decref(_M0L6_2atmpS2362);
  _M0L3lenS2365 = _M0L4selfS385->$1;
  _M0L6_2atmpS2364 = _M0L3lenS2365 + _M0L8str__lenS383;
  _M0L4selfS385->$1 = _M0L6_2atmpS2364;
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/home/developer/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS380
) {
  moonbit_string_t* _M0L8_2afieldS3978;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3978 = _M0L4selfS380->$0;
  moonbit_incref(_M0L8_2afieldS3978);
  return _M0L8_2afieldS3978;
}

moonbit_string_t* _M0MPC15array5Array6bufferGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS381
) {
  moonbit_string_t* _M0L8_2afieldS3979;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3979 = _M0L4selfS381->$0;
  moonbit_incref(_M0L8_2afieldS3979);
  return _M0L8_2afieldS3979;
}

struct _M0TUsfE** _M0MPC15array5Array6bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS382
) {
  struct _M0TUsfE** _M0L8_2afieldS3980;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS3980 = _M0L4selfS382->$0;
  moonbit_incref(_M0L8_2afieldS3980);
  return _M0L8_2afieldS3980;
}

struct _M0TPB4IterGUssEE* _M0MPB4Iter3newGUssEE(
  struct _M0TWEOUssE* _M0L1fS364,
  int64_t _M0L10size__hintS361
) {
  int64_t _M0L10size__hintS360;
  struct _M0TPB4IterGUssEE* _block_4331;
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
  _block_4331
  = (struct _M0TPB4IterGUssEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUssEE));
  Moonbit_object_header(_block_4331)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 116, 0);
  _block_4331->$0 = _M0L1fS364;
  _block_4331->$1 = _M0L10size__hintS360;
  return _block_4331;
}

struct _M0TPB4IterGUsbEE* _M0MPB4Iter3newGUsbEE(
  struct _M0TWEOUsbE* _M0L1fS369,
  int64_t _M0L10size__hintS366
) {
  int64_t _M0L10size__hintS365;
  struct _M0TPB4IterGUsbEE* _block_4332;
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
  _block_4332
  = (struct _M0TPB4IterGUsbEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsbEE));
  Moonbit_object_header(_block_4332)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 119, 0);
  _block_4332->$0 = _M0L1fS369;
  _block_4332->$1 = _M0L10size__hintS365;
  return _block_4332;
}

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB4Iter3newGUsRP19moonbitDB10RedisValueEE(
  struct _M0TWEOUsRP19moonbitDB10RedisValueE* _M0L1fS374,
  int64_t _M0L10size__hintS371
) {
  int64_t _M0L10size__hintS370;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _block_4333;
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
  _block_4333
  = (struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE));
  Moonbit_object_header(_block_4333)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 122, 0);
  _block_4333->$0 = _M0L1fS374;
  _block_4333->$1 = _M0L10size__hintS370;
  return _block_4333;
}

struct _M0TPB4IterGUsfEE* _M0MPB4Iter3newGUsfEE(
  struct _M0TWEOUsfE* _M0L1fS379,
  int64_t _M0L10size__hintS376
) {
  int64_t _M0L10size__hintS375;
  struct _M0TPB4IterGUsfEE* _block_4334;
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
  _block_4334
  = (struct _M0TPB4IterGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsfEE));
  Moonbit_object_header(_block_4334)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 125, 0);
  _block_4334->$0 = _M0L1fS379;
  _block_4334->$1 = _M0L10size__hintS375;
  return _block_4334;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS352,
  int32_t _M0L5radixS351
) {
  int32_t _if__result_4335;
  uint16_t* _M0L6bufferS353;
  #line 607 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS351 < 2) {
    _if__result_4335 = 1;
  } else {
    _if__result_4335 = _M0L5radixS351 > 36;
  }
  if (_if__result_4335) {
    #line 611 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_108.data);
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
  int32_t _M0L6_2atmpS2357;
  uint64_t _M0L3numS327;
  int32_t _M0L6offsetS328;
  #line 493 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2357 = _M0L10total__lenS350 - _M0L12digit__startS338;
  _M0L3numS327 = _M0L3numS349;
  _M0L6offsetS328 = _M0L6_2atmpS2357;
  while (1) {
    if (_M0L3numS327 >= 10000ull) {
      uint64_t _M0L1tS329 = _M0L3numS327 / 10000ull;
      uint64_t _M0L6_2atmpS2334 = _M0L3numS327 % 10000ull;
      int32_t _M0L1rS330 = (int32_t)_M0L6_2atmpS2334;
      int32_t _M0L2d1S331 = _M0L1rS330 / 100;
      int32_t _M0L2d2S332 = _M0L1rS330 % 100;
      int32_t _M0L6_2atmpS2333 = _M0L2d1S331 / 10;
      int32_t _M0L6_2atmpS2332 = 48 + _M0L6_2atmpS2333;
      int32_t _M0L6d1__hiS333 = (uint16_t)_M0L6_2atmpS2332;
      int32_t _M0L6_2atmpS2331 = _M0L2d1S331 % 10;
      int32_t _M0L6_2atmpS2330 = 48 + _M0L6_2atmpS2331;
      int32_t _M0L6d1__loS334 = (uint16_t)_M0L6_2atmpS2330;
      int32_t _M0L6_2atmpS2329 = _M0L2d2S332 / 10;
      int32_t _M0L6_2atmpS2328 = 48 + _M0L6_2atmpS2329;
      int32_t _M0L6d2__hiS335 = (uint16_t)_M0L6_2atmpS2328;
      int32_t _M0L6_2atmpS2327 = _M0L2d2S332 % 10;
      int32_t _M0L6_2atmpS2326 = 48 + _M0L6_2atmpS2327;
      int32_t _M0L6d2__loS336 = (uint16_t)_M0L6_2atmpS2326;
      int32_t _M0L6_2atmpS2318 = _M0L12digit__startS338 + _M0L6offsetS328;
      int32_t _M0L6_2atmpS2317 = _M0L6_2atmpS2318 - 4;
      int32_t _M0L6_2atmpS2320;
      int32_t _M0L6_2atmpS2319;
      int32_t _M0L6_2atmpS2322;
      int32_t _M0L6_2atmpS2321;
      int32_t _M0L6_2atmpS2324;
      int32_t _M0L6_2atmpS2323;
      int32_t _M0L6_2atmpS2325;
      _M0L6bufferS337[_M0L6_2atmpS2317] = _M0L6d1__hiS333;
      _M0L6_2atmpS2320 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2319 = _M0L6_2atmpS2320 - 3;
      _M0L6bufferS337[_M0L6_2atmpS2319] = _M0L6d1__loS334;
      _M0L6_2atmpS2322 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2321 = _M0L6_2atmpS2322 - 2;
      _M0L6bufferS337[_M0L6_2atmpS2321] = _M0L6d2__hiS335;
      _M0L6_2atmpS2324 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2323 = _M0L6_2atmpS2324 - 1;
      _M0L6bufferS337[_M0L6_2atmpS2323] = _M0L6d2__loS336;
      _M0L6_2atmpS2325 = _M0L6offsetS328 - 4;
      _M0L3numS327 = _M0L1tS329;
      _M0L6offsetS328 = _M0L6_2atmpS2325;
      continue;
    } else {
      int32_t _M0L6_2atmpS2356 = (int32_t)_M0L3numS327;
      int32_t _M0L9remainingS340 = _M0L6_2atmpS2356;
      int32_t _M0L6offsetS341 = _M0L6offsetS328;
      while (1) {
        if (_M0L9remainingS340 >= 100) {
          int32_t _M0L1tS342 = _M0L9remainingS340 / 100;
          int32_t _M0L1dS343 = _M0L9remainingS340 % 100;
          int32_t _M0L6_2atmpS2343 = _M0L1dS343 / 10;
          int32_t _M0L6_2atmpS2342 = 48 + _M0L6_2atmpS2343;
          int32_t _M0L5d__hiS344 = (uint16_t)_M0L6_2atmpS2342;
          int32_t _M0L6_2atmpS2341 = _M0L1dS343 % 10;
          int32_t _M0L6_2atmpS2340 = 48 + _M0L6_2atmpS2341;
          int32_t _M0L5d__loS345 = (uint16_t)_M0L6_2atmpS2340;
          int32_t _M0L6_2atmpS2336 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2335 = _M0L6_2atmpS2336 - 2;
          int32_t _M0L6_2atmpS2338;
          int32_t _M0L6_2atmpS2337;
          int32_t _M0L6_2atmpS2339;
          _M0L6bufferS337[_M0L6_2atmpS2335] = _M0L5d__hiS344;
          _M0L6_2atmpS2338 = _M0L12digit__startS338 + _M0L6offsetS341;
          _M0L6_2atmpS2337 = _M0L6_2atmpS2338 - 1;
          _M0L6bufferS337[_M0L6_2atmpS2337] = _M0L5d__loS345;
          _M0L6_2atmpS2339 = _M0L6offsetS341 - 2;
          _M0L9remainingS340 = _M0L1tS342;
          _M0L6offsetS341 = _M0L6_2atmpS2339;
          continue;
        } else if (_M0L9remainingS340 >= 10) {
          int32_t _M0L6_2atmpS2351 = _M0L9remainingS340 / 10;
          int32_t _M0L6_2atmpS2350 = 48 + _M0L6_2atmpS2351;
          int32_t _M0L5d__hiS347 = (uint16_t)_M0L6_2atmpS2350;
          int32_t _M0L6_2atmpS2349 = _M0L9remainingS340 % 10;
          int32_t _M0L6_2atmpS2348 = 48 + _M0L6_2atmpS2349;
          int32_t _M0L5d__loS348 = (uint16_t)_M0L6_2atmpS2348;
          int32_t _M0L6_2atmpS2345 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2344 = _M0L6_2atmpS2345 - 2;
          int32_t _M0L6_2atmpS2347;
          int32_t _M0L6_2atmpS2346;
          _M0L6bufferS337[_M0L6_2atmpS2344] = _M0L5d__hiS347;
          _M0L6_2atmpS2347 = _M0L12digit__startS338 + _M0L6offsetS341;
          _M0L6_2atmpS2346 = _M0L6_2atmpS2347 - 1;
          _M0L6bufferS337[_M0L6_2atmpS2346] = _M0L5d__loS348;
        } else {
          int32_t _M0L6_2atmpS2355 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2352 = _M0L6_2atmpS2355 - 1;
          int32_t _M0L6_2atmpS2354 = 48 + _M0L9remainingS340;
          int32_t _M0L6_2atmpS2353 = (uint16_t)_M0L6_2atmpS2354;
          _M0L6bufferS337[_M0L6_2atmpS2352] = _M0L6_2atmpS2353;
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
  int32_t _M0L6_2atmpS2302;
  int32_t _M0L6_2atmpS2301;
  #line 462 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  #line 470 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS310 = _M0MPC13int3Int10to__uint64(_M0L5radixS311);
  _M0L6_2atmpS2302 = _M0L5radixS311 - 1;
  _M0L6_2atmpS2301 = _M0L5radixS311 & _M0L6_2atmpS2302;
  if (_M0L6_2atmpS2301 == 0) {
    int32_t _M0L5shiftS312;
    uint64_t _M0L4maskS313;
    int32_t _M0L6_2atmpS2309;
    int32_t _M0L6offsetS314;
    uint64_t _M0L1nS315;
    #line 473 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS312 = moonbit_ctz32(_M0L5radixS311);
    _M0L4maskS313 = _M0L4baseS310 - 1ull;
    _M0L6_2atmpS2309 = _M0L10total__lenS320 - _M0L12digit__startS318;
    _M0L6offsetS314 = _M0L6_2atmpS2309;
    _M0L1nS315 = _M0L3numS321;
    while (1) {
      if (_M0L1nS315 > 0ull) {
        uint64_t _M0L6_2atmpS2308 = _M0L1nS315 & _M0L4maskS313;
        int32_t _M0L5digitS316 = (int32_t)_M0L6_2atmpS2308;
        int32_t _M0L6_2atmpS2305 = _M0L12digit__startS318 + _M0L6offsetS314;
        int32_t _M0L6_2atmpS2303 = _M0L6_2atmpS2305 - 1;
        int32_t _M0L6_2atmpS2304 =
          ((moonbit_string_t)moonbit_string_literal_109.data)[_M0L5digitS316];
        int32_t _M0L6_2atmpS2306;
        uint64_t _M0L6_2atmpS2307;
        _M0L6bufferS317[_M0L6_2atmpS2303] = _M0L6_2atmpS2304;
        _M0L6_2atmpS2306 = _M0L6offsetS314 - 1;
        _M0L6_2atmpS2307 = _M0L1nS315 >> (_M0L5shiftS312 & 63);
        _M0L6offsetS314 = _M0L6_2atmpS2306;
        _M0L1nS315 = _M0L6_2atmpS2307;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2316 = _M0L10total__lenS320 - _M0L12digit__startS318;
    int32_t _M0L6offsetS322 = _M0L6_2atmpS2316;
    uint64_t _M0L1nS323 = _M0L3numS321;
    while (1) {
      if (_M0L1nS323 > 0ull) {
        uint64_t _M0L1qS324 = _M0L1nS323 / _M0L4baseS310;
        uint64_t _M0L6_2atmpS2315 = _M0L1qS324 * _M0L4baseS310;
        uint64_t _M0L6_2atmpS2314 = _M0L1nS323 - _M0L6_2atmpS2315;
        int32_t _M0L5digitS325 = (int32_t)_M0L6_2atmpS2314;
        int32_t _M0L6_2atmpS2312 = _M0L12digit__startS318 + _M0L6offsetS322;
        int32_t _M0L6_2atmpS2310 = _M0L6_2atmpS2312 - 1;
        int32_t _M0L6_2atmpS2311 =
          ((moonbit_string_t)moonbit_string_literal_109.data)[_M0L5digitS325];
        int32_t _M0L6_2atmpS2313;
        _M0L6bufferS317[_M0L6_2atmpS2310] = _M0L6_2atmpS2311;
        _M0L6_2atmpS2313 = _M0L6offsetS322 - 1;
        _M0L6offsetS322 = _M0L6_2atmpS2313;
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
  int32_t _M0L6_2atmpS2300;
  int32_t _M0L6offsetS299;
  uint64_t _M0L1nS300;
  #line 434 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2300 = _M0L10total__lenS308 - _M0L12digit__startS305;
  _M0L6offsetS299 = _M0L6_2atmpS2300;
  _M0L1nS300 = _M0L3numS309;
  while (1) {
    if (_M0L6offsetS299 >= 2) {
      uint64_t _M0L6_2atmpS2297 = _M0L1nS300 & 255ull;
      int32_t _M0L9byte__valS301 = (int32_t)_M0L6_2atmpS2297;
      int32_t _M0L2hiS302 = _M0L9byte__valS301 / 16;
      int32_t _M0L2loS303 = _M0L9byte__valS301 % 16;
      int32_t _M0L6_2atmpS2291 = _M0L12digit__startS305 + _M0L6offsetS299;
      int32_t _M0L6_2atmpS2289 = _M0L6_2atmpS2291 - 2;
      int32_t _M0L6_2atmpS2290 =
        ((moonbit_string_t)moonbit_string_literal_109.data)[_M0L2hiS302];
      int32_t _M0L6_2atmpS2294;
      int32_t _M0L6_2atmpS2292;
      int32_t _M0L6_2atmpS2293;
      int32_t _M0L6_2atmpS2295;
      uint64_t _M0L6_2atmpS2296;
      _M0L6bufferS304[_M0L6_2atmpS2289] = _M0L6_2atmpS2290;
      _M0L6_2atmpS2294 = _M0L12digit__startS305 + _M0L6offsetS299;
      _M0L6_2atmpS2292 = _M0L6_2atmpS2294 - 1;
      _M0L6_2atmpS2293
      = ((moonbit_string_t)moonbit_string_literal_109.data)[
        _M0L2loS303
      ];
      _M0L6bufferS304[_M0L6_2atmpS2292] = _M0L6_2atmpS2293;
      _M0L6_2atmpS2295 = _M0L6offsetS299 - 2;
      _M0L6_2atmpS2296 = _M0L1nS300 >> 8;
      _M0L6offsetS299 = _M0L6_2atmpS2295;
      _M0L1nS300 = _M0L6_2atmpS2296;
      continue;
    } else if (_M0L6offsetS299 == 1) {
      uint64_t _M0L6_2atmpS2299 = _M0L1nS300 & 15ull;
      int32_t _M0L6nibbleS307 = (int32_t)_M0L6_2atmpS2299;
      int32_t _M0L6_2atmpS2298 =
        ((moonbit_string_t)moonbit_string_literal_109.data)[_M0L6nibbleS307];
      _M0L6bufferS304[_M0L12digit__startS305] = _M0L6_2atmpS2298;
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
      uint64_t _M0L6_2atmpS2287 = _M0L3numS296 / _M0L4baseS294;
      int32_t _M0L6_2atmpS2288 = _M0L5countS297 + 1;
      _M0L3numS296 = _M0L6_2atmpS2287;
      _M0L5countS297 = _M0L6_2atmpS2288;
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
    int32_t _M0L6_2atmpS2286;
    int32_t _M0L6_2atmpS2285;
    #line 412 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS292 = moonbit_clz64(_M0L5valueS291);
    _M0L6_2atmpS2286 = 63 - _M0L14leading__zerosS292;
    _M0L6_2atmpS2285 = _M0L6_2atmpS2286 / 4;
    return _M0L6_2atmpS2285 + 1;
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
  int32_t _if__result_4342;
  int32_t _M0L12is__negativeS275;
  uint32_t _M0L3numS276;
  uint16_t* _M0L6bufferS277;
  #line 209 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS273 < 2) {
    _if__result_4342 = 1;
  } else {
    _if__result_4342 = _M0L5radixS273 > 36;
  }
  if (_if__result_4342) {
    #line 213 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_108.data);
  }
  if (_M0L4selfS274 == 0) {
    return (moonbit_string_t)moonbit_string_literal_84.data;
  }
  _M0L12is__negativeS275 = _M0L4selfS274 < 0;
  if (_M0L12is__negativeS275) {
    int32_t _M0L6_2atmpS2284 = -_M0L4selfS274;
    _M0L3numS276 = *(uint32_t*)&_M0L6_2atmpS2284;
  } else {
    _M0L3numS276 = *(uint32_t*)&_M0L4selfS274;
  }
  switch (_M0L5radixS273) {
    case 10: {
      int32_t _M0L10digit__lenS278;
      int32_t _M0L6_2atmpS2281;
      int32_t _M0L10total__lenS279;
      uint16_t* _M0L6bufferS280;
      int32_t _M0L12digit__startS281;
      #line 235 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS278 = _M0FPB12dec__count32(_M0L3numS276);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2281 = 1;
      } else {
        _M0L6_2atmpS2281 = 0;
      }
      _M0L10total__lenS279 = _M0L10digit__lenS278 + _M0L6_2atmpS2281;
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
      int32_t _M0L6_2atmpS2282;
      int32_t _M0L10total__lenS283;
      uint16_t* _M0L6bufferS284;
      int32_t _M0L12digit__startS285;
      #line 243 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS282 = _M0FPB12hex__count32(_M0L3numS276);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2282 = 1;
      } else {
        _M0L6_2atmpS2282 = 0;
      }
      _M0L10total__lenS283 = _M0L10digit__lenS282 + _M0L6_2atmpS2282;
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
      int32_t _M0L6_2atmpS2283;
      int32_t _M0L10total__lenS287;
      uint16_t* _M0L6bufferS288;
      int32_t _M0L12digit__startS289;
      #line 251 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS286
      = _M0FPB14radix__count32(_M0L3numS276, _M0L5radixS273);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2283 = 1;
      } else {
        _M0L6_2atmpS2283 = 0;
      }
      _M0L10total__lenS287 = _M0L10digit__lenS286 + _M0L6_2atmpS2283;
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
      uint32_t _M0L6_2atmpS2279 = _M0L3numS270 / _M0L4baseS268;
      int32_t _M0L6_2atmpS2280 = _M0L5countS271 + 1;
      _M0L3numS270 = _M0L6_2atmpS2279;
      _M0L5countS271 = _M0L6_2atmpS2280;
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
    int32_t _M0L6_2atmpS2278;
    int32_t _M0L6_2atmpS2277;
    #line 182 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS266 = moonbit_clz32(_M0L5valueS265);
    _M0L6_2atmpS2278 = 31 - _M0L14leading__zerosS266;
    _M0L6_2atmpS2277 = _M0L6_2atmpS2278 / 4;
    return _M0L6_2atmpS2277 + 1;
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
  int32_t _M0L6_2atmpS2276;
  uint32_t _M0L3numS240;
  int32_t _M0L6offsetS241;
  #line 88 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2276 = _M0L10total__lenS263 - _M0L12digit__startS251;
  _M0L3numS240 = _M0L3numS262;
  _M0L6offsetS241 = _M0L6_2atmpS2276;
  while (1) {
    if (_M0L3numS240 >= 10000u) {
      uint32_t _M0L1tS242 = _M0L3numS240 / 10000u;
      uint32_t _M0L6_2atmpS2253 = _M0L3numS240 % 10000u;
      int32_t _M0L1rS243 = *(int32_t*)&_M0L6_2atmpS2253;
      int32_t _M0L2d1S244 = _M0L1rS243 / 100;
      int32_t _M0L2d2S245 = _M0L1rS243 % 100;
      int32_t _M0L6_2atmpS2252 = _M0L2d1S244 / 10;
      int32_t _M0L6_2atmpS2251 = 48 + _M0L6_2atmpS2252;
      int32_t _M0L6d1__hiS246 = (uint16_t)_M0L6_2atmpS2251;
      int32_t _M0L6_2atmpS2250 = _M0L2d1S244 % 10;
      int32_t _M0L6_2atmpS2249 = 48 + _M0L6_2atmpS2250;
      int32_t _M0L6d1__loS247 = (uint16_t)_M0L6_2atmpS2249;
      int32_t _M0L6_2atmpS2248 = _M0L2d2S245 / 10;
      int32_t _M0L6_2atmpS2247 = 48 + _M0L6_2atmpS2248;
      int32_t _M0L6d2__hiS248 = (uint16_t)_M0L6_2atmpS2247;
      int32_t _M0L6_2atmpS2246 = _M0L2d2S245 % 10;
      int32_t _M0L6_2atmpS2245 = 48 + _M0L6_2atmpS2246;
      int32_t _M0L6d2__loS249 = (uint16_t)_M0L6_2atmpS2245;
      int32_t _M0L6_2atmpS2237 = _M0L12digit__startS251 + _M0L6offsetS241;
      int32_t _M0L6_2atmpS2236 = _M0L6_2atmpS2237 - 4;
      int32_t _M0L6_2atmpS2239;
      int32_t _M0L6_2atmpS2238;
      int32_t _M0L6_2atmpS2241;
      int32_t _M0L6_2atmpS2240;
      int32_t _M0L6_2atmpS2243;
      int32_t _M0L6_2atmpS2242;
      int32_t _M0L6_2atmpS2244;
      _M0L6bufferS250[_M0L6_2atmpS2236] = _M0L6d1__hiS246;
      _M0L6_2atmpS2239 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2238 = _M0L6_2atmpS2239 - 3;
      _M0L6bufferS250[_M0L6_2atmpS2238] = _M0L6d1__loS247;
      _M0L6_2atmpS2241 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2240 = _M0L6_2atmpS2241 - 2;
      _M0L6bufferS250[_M0L6_2atmpS2240] = _M0L6d2__hiS248;
      _M0L6_2atmpS2243 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2242 = _M0L6_2atmpS2243 - 1;
      _M0L6bufferS250[_M0L6_2atmpS2242] = _M0L6d2__loS249;
      _M0L6_2atmpS2244 = _M0L6offsetS241 - 4;
      _M0L3numS240 = _M0L1tS242;
      _M0L6offsetS241 = _M0L6_2atmpS2244;
      continue;
    } else {
      int32_t _M0L6_2atmpS2275 = *(int32_t*)&_M0L3numS240;
      int32_t _M0L9remainingS253 = _M0L6_2atmpS2275;
      int32_t _M0L6offsetS254 = _M0L6offsetS241;
      while (1) {
        if (_M0L9remainingS253 >= 100) {
          int32_t _M0L1tS255 = _M0L9remainingS253 / 100;
          int32_t _M0L1dS256 = _M0L9remainingS253 % 100;
          int32_t _M0L6_2atmpS2262 = _M0L1dS256 / 10;
          int32_t _M0L6_2atmpS2261 = 48 + _M0L6_2atmpS2262;
          int32_t _M0L5d__hiS257 = (uint16_t)_M0L6_2atmpS2261;
          int32_t _M0L6_2atmpS2260 = _M0L1dS256 % 10;
          int32_t _M0L6_2atmpS2259 = 48 + _M0L6_2atmpS2260;
          int32_t _M0L5d__loS258 = (uint16_t)_M0L6_2atmpS2259;
          int32_t _M0L6_2atmpS2255 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2254 = _M0L6_2atmpS2255 - 2;
          int32_t _M0L6_2atmpS2257;
          int32_t _M0L6_2atmpS2256;
          int32_t _M0L6_2atmpS2258;
          _M0L6bufferS250[_M0L6_2atmpS2254] = _M0L5d__hiS257;
          _M0L6_2atmpS2257 = _M0L12digit__startS251 + _M0L6offsetS254;
          _M0L6_2atmpS2256 = _M0L6_2atmpS2257 - 1;
          _M0L6bufferS250[_M0L6_2atmpS2256] = _M0L5d__loS258;
          _M0L6_2atmpS2258 = _M0L6offsetS254 - 2;
          _M0L9remainingS253 = _M0L1tS255;
          _M0L6offsetS254 = _M0L6_2atmpS2258;
          continue;
        } else if (_M0L9remainingS253 >= 10) {
          int32_t _M0L6_2atmpS2270 = _M0L9remainingS253 / 10;
          int32_t _M0L6_2atmpS2269 = 48 + _M0L6_2atmpS2270;
          int32_t _M0L5d__hiS260 = (uint16_t)_M0L6_2atmpS2269;
          int32_t _M0L6_2atmpS2268 = _M0L9remainingS253 % 10;
          int32_t _M0L6_2atmpS2267 = 48 + _M0L6_2atmpS2268;
          int32_t _M0L5d__loS261 = (uint16_t)_M0L6_2atmpS2267;
          int32_t _M0L6_2atmpS2264 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2263 = _M0L6_2atmpS2264 - 2;
          int32_t _M0L6_2atmpS2266;
          int32_t _M0L6_2atmpS2265;
          _M0L6bufferS250[_M0L6_2atmpS2263] = _M0L5d__hiS260;
          _M0L6_2atmpS2266 = _M0L12digit__startS251 + _M0L6offsetS254;
          _M0L6_2atmpS2265 = _M0L6_2atmpS2266 - 1;
          _M0L6bufferS250[_M0L6_2atmpS2265] = _M0L5d__loS261;
        } else {
          int32_t _M0L6_2atmpS2274 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2271 = _M0L6_2atmpS2274 - 1;
          int32_t _M0L6_2atmpS2273 = 48 + _M0L9remainingS253;
          int32_t _M0L6_2atmpS2272 = (uint16_t)_M0L6_2atmpS2273;
          _M0L6bufferS250[_M0L6_2atmpS2271] = _M0L6_2atmpS2272;
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
  int32_t _M0L6_2atmpS2221;
  int32_t _M0L6_2atmpS2220;
  #line 57 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS223 = *(uint32_t*)&_M0L5radixS224;
  _M0L6_2atmpS2221 = _M0L5radixS224 - 1;
  _M0L6_2atmpS2220 = _M0L5radixS224 & _M0L6_2atmpS2221;
  if (_M0L6_2atmpS2220 == 0) {
    int32_t _M0L5shiftS225;
    uint32_t _M0L4maskS226;
    int32_t _M0L6_2atmpS2228;
    int32_t _M0L6offsetS227;
    uint32_t _M0L1nS228;
    #line 68 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS225 = moonbit_ctz32(_M0L5radixS224);
    _M0L4maskS226 = _M0L4baseS223 - 1u;
    _M0L6_2atmpS2228 = _M0L10total__lenS233 - _M0L12digit__startS231;
    _M0L6offsetS227 = _M0L6_2atmpS2228;
    _M0L1nS228 = _M0L3numS234;
    while (1) {
      if (_M0L1nS228 > 0u) {
        uint32_t _M0L6_2atmpS2227 = _M0L1nS228 & _M0L4maskS226;
        int32_t _M0L5digitS229 = *(int32_t*)&_M0L6_2atmpS2227;
        int32_t _M0L6_2atmpS2224 = _M0L12digit__startS231 + _M0L6offsetS227;
        int32_t _M0L6_2atmpS2222 = _M0L6_2atmpS2224 - 1;
        int32_t _M0L6_2atmpS2223 =
          ((moonbit_string_t)moonbit_string_literal_109.data)[_M0L5digitS229];
        int32_t _M0L6_2atmpS2225;
        uint32_t _M0L6_2atmpS2226;
        _M0L6bufferS230[_M0L6_2atmpS2222] = _M0L6_2atmpS2223;
        _M0L6_2atmpS2225 = _M0L6offsetS227 - 1;
        _M0L6_2atmpS2226 = _M0L1nS228 >> (_M0L5shiftS225 & 31);
        _M0L6offsetS227 = _M0L6_2atmpS2225;
        _M0L1nS228 = _M0L6_2atmpS2226;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2235 = _M0L10total__lenS233 - _M0L12digit__startS231;
    int32_t _M0L6offsetS235 = _M0L6_2atmpS2235;
    uint32_t _M0L1nS236 = _M0L3numS234;
    while (1) {
      if (_M0L1nS236 > 0u) {
        uint32_t _M0L1qS237 = _M0L1nS236 / _M0L4baseS223;
        uint32_t _M0L6_2atmpS2234 = _M0L1qS237 * _M0L4baseS223;
        uint32_t _M0L6_2atmpS2233 = _M0L1nS236 - _M0L6_2atmpS2234;
        int32_t _M0L5digitS238 = *(int32_t*)&_M0L6_2atmpS2233;
        int32_t _M0L6_2atmpS2231 = _M0L12digit__startS231 + _M0L6offsetS235;
        int32_t _M0L6_2atmpS2229 = _M0L6_2atmpS2231 - 1;
        int32_t _M0L6_2atmpS2230 =
          ((moonbit_string_t)moonbit_string_literal_109.data)[_M0L5digitS238];
        int32_t _M0L6_2atmpS2232;
        _M0L6bufferS230[_M0L6_2atmpS2229] = _M0L6_2atmpS2230;
        _M0L6_2atmpS2232 = _M0L6offsetS235 - 1;
        _M0L6offsetS235 = _M0L6_2atmpS2232;
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
  int32_t _M0L6_2atmpS2219;
  int32_t _M0L6offsetS212;
  uint32_t _M0L1nS213;
  #line 29 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2219 = _M0L10total__lenS221 - _M0L12digit__startS218;
  _M0L6offsetS212 = _M0L6_2atmpS2219;
  _M0L1nS213 = _M0L3numS222;
  while (1) {
    if (_M0L6offsetS212 >= 2) {
      uint32_t _M0L6_2atmpS2216 = _M0L1nS213 & 255u;
      int32_t _M0L9byte__valS214 = *(int32_t*)&_M0L6_2atmpS2216;
      int32_t _M0L2hiS215 = _M0L9byte__valS214 / 16;
      int32_t _M0L2loS216 = _M0L9byte__valS214 % 16;
      int32_t _M0L6_2atmpS2210 = _M0L12digit__startS218 + _M0L6offsetS212;
      int32_t _M0L6_2atmpS2208 = _M0L6_2atmpS2210 - 2;
      int32_t _M0L6_2atmpS2209 =
        ((moonbit_string_t)moonbit_string_literal_109.data)[_M0L2hiS215];
      int32_t _M0L6_2atmpS2213;
      int32_t _M0L6_2atmpS2211;
      int32_t _M0L6_2atmpS2212;
      int32_t _M0L6_2atmpS2214;
      uint32_t _M0L6_2atmpS2215;
      _M0L6bufferS217[_M0L6_2atmpS2208] = _M0L6_2atmpS2209;
      _M0L6_2atmpS2213 = _M0L12digit__startS218 + _M0L6offsetS212;
      _M0L6_2atmpS2211 = _M0L6_2atmpS2213 - 1;
      _M0L6_2atmpS2212
      = ((moonbit_string_t)moonbit_string_literal_109.data)[
        _M0L2loS216
      ];
      _M0L6bufferS217[_M0L6_2atmpS2211] = _M0L6_2atmpS2212;
      _M0L6_2atmpS2214 = _M0L6offsetS212 - 2;
      _M0L6_2atmpS2215 = _M0L1nS213 >> 8;
      _M0L6offsetS212 = _M0L6_2atmpS2214;
      _M0L1nS213 = _M0L6_2atmpS2215;
      continue;
    } else if (_M0L6offsetS212 == 1) {
      uint32_t _M0L6_2atmpS2218 = _M0L1nS213 & 15u;
      int32_t _M0L6nibbleS220 = *(int32_t*)&_M0L6_2atmpS2218;
      int32_t _M0L6_2atmpS2217 =
        ((moonbit_string_t)moonbit_string_literal_109.data)[_M0L6nibbleS220];
      _M0L6bufferS217[_M0L12digit__startS218] = _M0L6_2atmpS2217;
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
    int64_t _M0L6_2atmpS2200;
    if (_M0L4_2anS193 > 0) {
      int32_t _M0L6_2atmpS2201 = _M0L4_2anS193 - 1;
      _M0L6_2atmpS2200 = (int64_t)_M0L6_2atmpS2201;
    } else {
      _M0L6_2atmpS2200 = _M0MPB4Iter4nextN6constrS9980GUssEE;
    }
    _M0L4selfS189->$1 = _M0L6_2atmpS2200;
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
    int64_t _M0L6_2atmpS2202;
    if (_M0L4_2anS199 > 0) {
      int32_t _M0L6_2atmpS2203 = _M0L4_2anS199 - 1;
      _M0L6_2atmpS2202 = (int64_t)_M0L6_2atmpS2203;
    } else {
      _M0L6_2atmpS2202 = _M0MPB4Iter4nextN6constrS9980GUsbEE;
    }
    _M0L4selfS195->$1 = _M0L6_2atmpS2202;
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
    int64_t _M0L6_2atmpS2204;
    if (_M0L4_2anS205 > 0) {
      int32_t _M0L6_2atmpS2205 = _M0L4_2anS205 - 1;
      _M0L6_2atmpS2204 = (int64_t)_M0L6_2atmpS2205;
    } else {
      _M0L6_2atmpS2204
      = _M0MPB4Iter4nextN6constrS9980GUsRP19moonbitDB10RedisValueEE;
    }
    _M0L4selfS201->$1 = _M0L6_2atmpS2204;
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
    int64_t _M0L6_2atmpS2206;
    if (_M0L4_2anS211 > 0) {
      int32_t _M0L6_2atmpS2207 = _M0L4_2anS211 - 1;
      _M0L6_2atmpS2206 = (int64_t)_M0L6_2atmpS2207;
    } else {
      _M0L6_2atmpS2206 = _M0MPB4Iter4nextN6constrS9980GUsfEE;
    }
    _M0L4selfS207->$1 = _M0L6_2atmpS2206;
  }
  return _M0L6resultS208;
}

int32_t _M0IP016_24default__implPB4Show6outputGsE(
  moonbit_string_t _M0L4selfS179,
  struct _M0TPB6Logger _M0L6loggerS178
) {
  moonbit_string_t _M0L6_2atmpS2195;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2195 = _M0IPC16string6StringPB4Show10to__string(_M0L4selfS179);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS178.$0->$method_0(_M0L6loggerS178.$1, _M0L6_2atmpS2195);
  moonbit_decref(_M0L6_2atmpS2195);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGiE(
  int32_t _M0L4selfS181,
  struct _M0TPB6Logger _M0L6loggerS180
) {
  moonbit_string_t _M0L6_2atmpS2196;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2196 = _M0IPC13int3IntPB4Show10to__string(_M0L4selfS181);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS180.$0->$method_0(_M0L6loggerS180.$1, _M0L6_2atmpS2196);
  moonbit_decref(_M0L6_2atmpS2196);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGbE(
  int32_t _M0L4selfS183,
  struct _M0TPB6Logger _M0L6loggerS182
) {
  moonbit_string_t _M0L6_2atmpS2197;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2197 = _M0IPC14bool4BoolPB4Show10to__string(_M0L4selfS183);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS182.$0->$method_0(_M0L6loggerS182.$1, _M0L6_2atmpS2197);
  moonbit_decref(_M0L6_2atmpS2197);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGfE(
  float _M0L4selfS185,
  struct _M0TPB6Logger _M0L6loggerS184
) {
  moonbit_string_t _M0L6_2atmpS2198;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2198 = _M0IPC15float5FloatPB4Show10to__string(_M0L4selfS185);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS184.$0->$method_0(_M0L6loggerS184.$1, _M0L6_2atmpS2198);
  moonbit_decref(_M0L6_2atmpS2198);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGmE(
  uint64_t _M0L4selfS187,
  struct _M0TPB6Logger _M0L6loggerS186
) {
  moonbit_string_t _M0L6_2atmpS2199;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2199 = _M0IPC16uint646UInt64PB4Show10to__string(_M0L4selfS187);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS186.$0->$method_0(_M0L6loggerS186.$1, _M0L6_2atmpS2199);
  moonbit_decref(_M0L6_2atmpS2199);
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
  moonbit_string_t _M0L8_2afieldS3985;
  #line 92 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L8_2afieldS3985 = _M0L4selfS176.$0;
  moonbit_incref(_M0L8_2afieldS3985);
  return _M0L8_2afieldS3985;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS172,
  moonbit_string_t _M0L5valueS173,
  int32_t _M0L5startS174,
  int32_t _M0L3lenS175
) {
  int32_t _M0L6_2atmpS2194;
  int64_t _M0L6_2atmpS2193;
  struct _M0TPC16string10StringView _M0L6_2atmpS2192;
  #line 122 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2194 = _M0L5startS174 + _M0L3lenS175;
  _M0L6_2atmpS2193 = (int64_t)_M0L6_2atmpS2194;
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2192
  = _M0MPC16string6String11sub_2einner(_M0L5valueS173, _M0L5startS174, _M0L6_2atmpS2193);
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS172, _M0L6_2atmpS2192);
  moonbit_decref(_M0L6_2atmpS2192.$0);
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
  int32_t _if__result_4349;
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
      _if__result_4349 = _M0L3endS166 <= _M0L3lenS164;
    } else {
      _if__result_4349 = 0;
    }
  } else {
    _if__result_4349 = 0;
  }
  if (_if__result_4349) {
    if (_M0L5startS170 < _M0L3lenS164) {
      int32_t _M0L6_2atmpS2189 = _M0L4selfS165[_M0L5startS170];
      int32_t _M0L6_2atmpS2188;
      #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2188
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2189);
      if (!_M0L6_2atmpS2188) {
        
      } else {
        #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS166 < _M0L3lenS164) {
      int32_t _M0L6_2atmpS2191 = _M0L4selfS165[_M0L3endS166];
      int32_t _M0L6_2atmpS2190;
      #line 768 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2190
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2191);
      if (!_M0L6_2atmpS2190) {
        
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
  struct _M0TPB6Logger _M0L6_2atmpS2187;
  #line 116 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS163);
  _M0L6_2atmpS2187
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS163
  };
  #line 117 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS162.$0->$method_0(_M0L4showS162.$1, _M0L6_2atmpS2187);
  if (_M0L6_2atmpS2187.$1) {
    moonbit_decref(_M0L6_2atmpS2187.$1);
  }
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS161,
  struct _M0TPB4Show _M0L4showS160
) {
  struct _M0TPB6Logger _M0L6_2atmpS2186;
  #line 111 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS161);
  _M0L6_2atmpS2186
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS161
  };
  #line 112 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS160.$0->$method_0(_M0L4showS160.$1, _M0L6_2atmpS2186);
  if (_M0L6_2atmpS2186.$1) {
    moonbit_decref(_M0L6_2atmpS2186.$1);
  }
  return 0;
}

int32_t _M0FPB13finalize__acc(uint32_t _M0L3accS159) {
  uint32_t _M0L6_2atmpS2185;
  #line 444 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  #line 445 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2185 = _M0FPB14avalanche__acc(_M0L3accS159);
  return *(int32_t*)&_M0L6_2atmpS2185;
}

uint32_t _M0FPB14avalanche__acc(uint32_t _M0L3accS158) {
  uint32_t _M0Lm3accS157;
  uint32_t _M0L6_2atmpS2174;
  uint32_t _M0L6_2atmpS2176;
  uint32_t _M0L6_2atmpS2175;
  uint32_t _M0L6_2atmpS2177;
  uint32_t _M0L6_2atmpS2178;
  uint32_t _M0L6_2atmpS2180;
  uint32_t _M0L6_2atmpS2179;
  uint32_t _M0L6_2atmpS2181;
  uint32_t _M0L6_2atmpS2182;
  uint32_t _M0L6_2atmpS2184;
  uint32_t _M0L6_2atmpS2183;
  #line 449 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0Lm3accS157 = _M0L3accS158;
  _M0L6_2atmpS2174 = _M0Lm3accS157;
  _M0L6_2atmpS2176 = _M0Lm3accS157;
  _M0L6_2atmpS2175 = _M0L6_2atmpS2176 >> 15;
  _M0Lm3accS157 = _M0L6_2atmpS2174 ^ _M0L6_2atmpS2175;
  _M0L6_2atmpS2177 = _M0Lm3accS157;
  _M0Lm3accS157 = _M0L6_2atmpS2177 * 2246822519u;
  _M0L6_2atmpS2178 = _M0Lm3accS157;
  _M0L6_2atmpS2180 = _M0Lm3accS157;
  _M0L6_2atmpS2179 = _M0L6_2atmpS2180 >> 13;
  _M0Lm3accS157 = _M0L6_2atmpS2178 ^ _M0L6_2atmpS2179;
  _M0L6_2atmpS2181 = _M0Lm3accS157;
  _M0Lm3accS157 = _M0L6_2atmpS2181 * 3266489917u;
  _M0L6_2atmpS2182 = _M0Lm3accS157;
  _M0L6_2atmpS2184 = _M0Lm3accS157;
  _M0L6_2atmpS2183 = _M0L6_2atmpS2184 >> 16;
  _M0Lm3accS157 = _M0L6_2atmpS2182 ^ _M0L6_2atmpS2183;
  return _M0Lm3accS157;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS156) {
  int64_t _M0L6_2atmpS2173;
  #line 907 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2173 = (int64_t)_M0L4selfS156;
  return *(uint64_t*)&_M0L6_2atmpS2173;
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS155,
  moonbit_string_t _M0L3strS154
) {
  int32_t _M0L8str__lenS153;
  int32_t _M0L3lenS2168;
  int32_t _M0L6_2atmpS2167;
  uint16_t* _M0L4dataS2169;
  int32_t _M0L3lenS2170;
  int32_t _M0L3lenS2172;
  int32_t _M0L6_2atmpS2171;
  #line 86 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS153 = Moonbit_array_length(_M0L3strS154);
  _M0L3lenS2168 = _M0L4selfS155->$1;
  _M0L6_2atmpS2167 = _M0L3lenS2168 + _M0L8str__lenS153;
  #line 88 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS155, _M0L6_2atmpS2167);
  _M0L4dataS2169 = _M0L4selfS155->$0;
  _M0L3lenS2170 = _M0L4selfS155->$1;
  moonbit_incref(_M0L4dataS2169);
  #line 89 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2169, _M0L3lenS2170, _M0L3strS154, 0, _M0L8str__lenS153);
  moonbit_decref(_M0L4dataS2169);
  _M0L3lenS2172 = _M0L4selfS155->$1;
  _M0L6_2atmpS2171 = _M0L3lenS2172 + _M0L8str__lenS153;
  _M0L4selfS155->$1 = _M0L6_2atmpS2171;
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
      int32_t _M0L6_2atmpS2164 = _M0L3strS150[_M0L1iS147];
      int32_t _M0L6_2atmpS2165;
      int32_t _M0L6_2atmpS2166;
      if (
        _M0L1jS148 < 0 || _M0L1jS148 >= Moonbit_array_length(_M0L4selfS149)
      ) {
        #line 80 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
        moonbit_panic();
      }
      _M0L4selfS149[_M0L1jS148] = _M0L6_2atmpS2164;
      _M0L6_2atmpS2165 = _M0L1iS147 + 1;
      _M0L6_2atmpS2166 = _M0L1jS148 + 1;
      _M0L1iS147 = _M0L6_2atmpS2165;
      _M0L1jS148 = _M0L6_2atmpS2166;
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
    int32_t _M0L3lenS2143 = _M0L4selfS141->$1;
    int32_t _M0L6_2atmpS2142 = _M0L3lenS2143 + 1;
    uint16_t* _M0L4dataS2144;
    int32_t _M0L3lenS2145;
    int32_t _M0L6_2atmpS2146;
    int32_t _M0L3lenS2148;
    int32_t _M0L6_2atmpS2147;
    #line 98 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS141, _M0L6_2atmpS2142);
    _M0L4dataS2144 = _M0L4selfS141->$0;
    _M0L3lenS2145 = _M0L4selfS141->$1;
    moonbit_incref(_M0L4dataS2144);
    #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2146 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS139);
    if (
      _M0L3lenS2145 < 0
      || _M0L3lenS2145 >= Moonbit_array_length(_M0L4dataS2144)
    ) {
      #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2144[_M0L3lenS2145] = _M0L6_2atmpS2146;
    moonbit_decref(_M0L4dataS2144);
    _M0L3lenS2148 = _M0L4selfS141->$1;
    _M0L6_2atmpS2147 = _M0L3lenS2148 + 1;
    _M0L4selfS141->$1 = _M0L6_2atmpS2147;
  } else if (_M0L4codeS139 <= 1114111u) {
    int32_t _M0L3lenS2150 = _M0L4selfS141->$1;
    int32_t _M0L6_2atmpS2149 = _M0L3lenS2150 + 2;
    uint32_t _M0L4codeS142;
    uint16_t* _M0L4dataS2151;
    int32_t _M0L3lenS2152;
    uint32_t _M0L6_2atmpS2155;
    uint32_t _M0L6_2atmpS2154;
    int32_t _M0L6_2atmpS2153;
    uint16_t* _M0L4dataS2156;
    int32_t _M0L3lenS2161;
    int32_t _M0L6_2atmpS2157;
    uint32_t _M0L6_2atmpS2160;
    uint32_t _M0L6_2atmpS2159;
    int32_t _M0L6_2atmpS2158;
    int32_t _M0L3lenS2163;
    int32_t _M0L6_2atmpS2162;
    #line 102 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS141, _M0L6_2atmpS2149);
    _M0L4codeS142 = _M0L4codeS139 - 65536u;
    _M0L4dataS2151 = _M0L4selfS141->$0;
    _M0L3lenS2152 = _M0L4selfS141->$1;
    _M0L6_2atmpS2155 = _M0L4codeS142 >> 10;
    _M0L6_2atmpS2154 = 55296u + _M0L6_2atmpS2155;
    moonbit_incref(_M0L4dataS2151);
    #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2153 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2154);
    if (
      _M0L3lenS2152 < 0
      || _M0L3lenS2152 >= Moonbit_array_length(_M0L4dataS2151)
    ) {
      #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2151[_M0L3lenS2152] = _M0L6_2atmpS2153;
    moonbit_decref(_M0L4dataS2151);
    _M0L4dataS2156 = _M0L4selfS141->$0;
    _M0L3lenS2161 = _M0L4selfS141->$1;
    _M0L6_2atmpS2157 = _M0L3lenS2161 + 1;
    _M0L6_2atmpS2160 = _M0L4codeS142 & 1023u;
    _M0L6_2atmpS2159 = 56320u + _M0L6_2atmpS2160;
    moonbit_incref(_M0L4dataS2156);
    #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2158 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2159);
    if (
      _M0L6_2atmpS2157 < 0
      || _M0L6_2atmpS2157 >= Moonbit_array_length(_M0L4dataS2156)
    ) {
      #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2156[_M0L6_2atmpS2157] = _M0L6_2atmpS2158;
    moonbit_decref(_M0L4dataS2156);
    _M0L3lenS2163 = _M0L4selfS141->$1;
    _M0L6_2atmpS2162 = _M0L3lenS2163 + 2;
    _M0L4selfS141->$1 = _M0L6_2atmpS2162;
  } else {
    #line 108 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_110.data);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS133,
  int32_t _M0L8requiredS134
) {
  uint16_t* _M0L4dataS2141;
  int32_t _M0L12current__lenS132;
  int32_t _M0L13enough__spaceS135;
  int32_t _M0L13enough__spaceS136;
  uint16_t* _M0L4dataS2137;
  int32_t _M0L6_2atmpS2138;
  int32_t _M0L3lenS2139;
  uint16_t* _M0L9new__dataS138;
  uint16_t* _M0L6_2aoldS3990;
  #line 46 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS2141 = _M0L4selfS133->$0;
  _M0L12current__lenS132 = Moonbit_array_length(_M0L4dataS2141);
  if (_M0L8requiredS134 <= _M0L12current__lenS132) {
    return 0;
  }
  _M0L13enough__spaceS136 = _M0L12current__lenS132;
  while (1) {
    if (_M0L13enough__spaceS136 < _M0L8requiredS134) {
      int32_t _M0L6_2atmpS2140 = _M0L13enough__spaceS136 * 2;
      _M0L13enough__spaceS136 = _M0L6_2atmpS2140;
      continue;
    } else {
      _M0L13enough__spaceS135 = _M0L13enough__spaceS136;
    }
    break;
  }
  _M0L4dataS2137 = _M0L4selfS133->$0;
  moonbit_incref(_M0L4dataS2137);
  #line 64 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2138 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L3lenS2139 = _M0L4selfS133->$1;
  #line 61 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L9new__dataS138
  = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS2137, _M0L13enough__spaceS135, _M0L6_2atmpS2138, _M0L3lenS2139, 0, 0);
  moonbit_decref(_M0L4dataS2137);
  _M0L6_2aoldS3990 = _M0L4selfS133->$0;
  moonbit_decref(_M0L6_2aoldS3990);
  _M0L4selfS133->$0 = _M0L9new__dataS138;
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS131) {
  int32_t _M0L6_2atmpS2136;
  #line 2676 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2136 = *(int32_t*)&_M0L4selfS131;
  return (uint16_t)_M0L6_2atmpS2136;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS130) {
  int32_t _M0L6_2atmpS2135;
  #line 1254 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2135 = _M0L4selfS130;
  return *(uint32_t*)&_M0L6_2atmpS2135;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS128
) {
  int32_t _M0L3lenS2127;
  uint16_t* _M0L4dataS2129;
  int32_t _M0L6_2atmpS2128;
  #line 148 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS2127 = _M0L4selfS128->$1;
  _M0L4dataS2129 = _M0L4selfS128->$0;
  _M0L6_2atmpS2128 = Moonbit_array_length(_M0L4dataS2129);
  if (_M0L3lenS2127 == _M0L6_2atmpS2128) {
    uint16_t* _M0L4dataS2130 = _M0L4selfS128->$0;
    moonbit_incref(_M0L4dataS2130);
    return _M0L4dataS2130;
  } else {
    uint16_t* _M0L4dataS2131 = _M0L4selfS128->$0;
    int32_t _M0L3lenS2132 = _M0L4selfS128->$1;
    int32_t _M0L6_2atmpS2133;
    int32_t _M0L3lenS2134;
    uint16_t* _M0L4dataS129;
    moonbit_incref(_M0L4dataS2131);
    #line 155 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2133 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L3lenS2134 = _M0L4selfS128->$1;
    #line 152 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L4dataS129
    = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS2131, _M0L3lenS2132, _M0L6_2atmpS2133, _M0L3lenS2134, 0, 0);
    moonbit_decref(_M0L4dataS2131);
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
  int32_t _if__result_4352;
  #line 97 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L13allocate__lenS121 >= 0) {
    if (_M0L3lenS122 >= 0) {
      if (_M0L11src__offsetS123 >= 0) {
        if (_M0L11dst__offsetS124 >= 0) {
          int32_t _M0L6_2atmpS2123 = _M0L11src__offsetS123 + _M0L3lenS122;
          int32_t _M0L6_2atmpS2124 = Moonbit_array_length(_M0L3srcS125);
          if (_M0L6_2atmpS2123 <= _M0L6_2atmpS2124) {
            int32_t _M0L6_2atmpS2122 = _M0L11dst__offsetS124 + _M0L3lenS122;
            _if__result_4352 = _M0L6_2atmpS2122 <= _M0L13allocate__lenS121;
          } else {
            _if__result_4352 = 0;
          }
        } else {
          _if__result_4352 = 0;
        }
      } else {
        _if__result_4352 = 0;
      }
    } else {
      _if__result_4352 = 0;
    }
  } else {
    _if__result_4352 = 0;
  }
  if (_if__result_4352) {
    moonbit_incref(_M0L3srcS125);
    #line 115 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    return _M0MPC15array10FixedArray23unsafe__make__and__blitGkE(_M0L3srcS125, _M0L13allocate__lenS121, _M0L4initS126, _M0L11src__offsetS123, _M0L11dst__offsetS124, _M0L3lenS122);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS127;
    int32_t _M0L6_2atmpS2126;
    moonbit_string_t _M0L6_2atmpS2125;
    uint16_t* _result_4353;
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L18_2astring__builderS127
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_111.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L13allocate__lenS121);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_112.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L11src__offsetS123);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_113.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L11dst__offsetS124);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_114.data);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L3lenS122);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS127, (moonbit_string_t)moonbit_string_literal_115.data);
    _M0L6_2atmpS2126 = Moonbit_array_length(_M0L3srcS125);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L6_2atmpS2126);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L6_2atmpS2125
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS127);
    moonbit_decref(_M0L18_2astring__builderS127);
    #line 111 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _result_4353 = _M0FPC15abort5abortGAkE(_M0L6_2atmpS2125);
    moonbit_decref(_M0L6_2atmpS2125);
    return _result_4353;
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
  struct _M0TPB13StringBuilder* _block_4354;
  #line 32 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS112 < 1) {
    _M0L7initialS111 = 1;
  } else {
    int32_t _M0L6_2atmpS2121 = _M0L10size__hintS112 + 1;
    _M0L7initialS111 = _M0L6_2atmpS2121 / 2;
  }
  _M0L4dataS113 = (uint16_t*)moonbit_make_string(_M0L7initialS111, 0);
  _block_4354
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4354)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 128, 0);
  _block_4354->$0 = _M0L4dataS113;
  _block_4354->$1 = 0;
  return _block_4354;
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(
  moonbit_string_t* _M0L3srcS97,
  int32_t _M0L13allocate__lenS93,
  int32_t _M0L3lenS94,
  int32_t _M0L11src__offsetS95,
  int32_t _M0L11dst__offsetS96
) {
  int32_t _if__result_4355;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS93 >= 0) {
    if (_M0L3lenS94 >= 0) {
      if (_M0L11src__offsetS95 >= 0) {
        if (_M0L11dst__offsetS96 >= 0) {
          int32_t _M0L6_2atmpS2107 = _M0L11src__offsetS95 + _M0L3lenS94;
          int32_t _M0L6_2atmpS2108;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2108
          = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS97);
          if (_M0L6_2atmpS2107 <= _M0L6_2atmpS2108) {
            int32_t _M0L6_2atmpS2106 = _M0L11dst__offsetS96 + _M0L3lenS94;
            _if__result_4355 = _M0L6_2atmpS2106 <= _M0L13allocate__lenS93;
          } else {
            _if__result_4355 = 0;
          }
        } else {
          _if__result_4355 = 0;
        }
      } else {
        _if__result_4355 = 0;
      }
    } else {
      _if__result_4355 = 0;
    }
  } else {
    _if__result_4355 = 0;
  }
  if (_if__result_4355) {
    moonbit_incref(_M0L3srcS97);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS93, (moonbit_string_t)moonbit_string_literal_95.data, _M0L3srcS97, _M0L11src__offsetS95, _M0L11dst__offsetS96, _M0L3lenS94);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS98;
    int32_t _M0L6_2atmpS2110;
    moonbit_string_t _M0L6_2atmpS2109;
    moonbit_string_t* _result_4356;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS98
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_111.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L13allocate__lenS93);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_112.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L11src__offsetS95);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_113.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L11dst__offsetS96);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_114.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L3lenS94);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS98, (moonbit_string_t)moonbit_string_literal_115.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2110 = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS97);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L6_2atmpS2110);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2109
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS98);
    moonbit_decref(_M0L18_2astring__builderS98);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4356
    = _M0FPC15abort5abortGRPB18UninitializedArrayGsEE(_M0L6_2atmpS2109);
    moonbit_decref(_M0L6_2atmpS2109);
    return _result_4356;
  }
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGOsE(
  moonbit_string_t* _M0L3srcS103,
  int32_t _M0L13allocate__lenS99,
  int32_t _M0L3lenS100,
  int32_t _M0L11src__offsetS101,
  int32_t _M0L11dst__offsetS102
) {
  int32_t _if__result_4357;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS99 >= 0) {
    if (_M0L3lenS100 >= 0) {
      if (_M0L11src__offsetS101 >= 0) {
        if (_M0L11dst__offsetS102 >= 0) {
          int32_t _M0L6_2atmpS2112 = _M0L11src__offsetS101 + _M0L3lenS100;
          int32_t _M0L6_2atmpS2113;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2113
          = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS103);
          if (_M0L6_2atmpS2112 <= _M0L6_2atmpS2113) {
            int32_t _M0L6_2atmpS2111 = _M0L11dst__offsetS102 + _M0L3lenS100;
            _if__result_4357 = _M0L6_2atmpS2111 <= _M0L13allocate__lenS99;
          } else {
            _if__result_4357 = 0;
          }
        } else {
          _if__result_4357 = 0;
        }
      } else {
        _if__result_4357 = 0;
      }
    } else {
      _if__result_4357 = 0;
    }
  } else {
    _if__result_4357 = 0;
  }
  if (_if__result_4357) {
    moonbit_incref(_M0L3srcS103);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS99, 0, _M0L3srcS103, _M0L11src__offsetS101, _M0L11dst__offsetS102, _M0L3lenS100);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS104;
    int32_t _M0L6_2atmpS2115;
    moonbit_string_t _M0L6_2atmpS2114;
    moonbit_string_t* _result_4358;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS104
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_111.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L13allocate__lenS99);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_112.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L11src__offsetS101);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_113.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L11dst__offsetS102);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_114.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L3lenS100);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS104, (moonbit_string_t)moonbit_string_literal_115.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2115 = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS103);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L6_2atmpS2115);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2114
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS104);
    moonbit_decref(_M0L18_2astring__builderS104);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4358
    = _M0FPC15abort5abortGRPB18UninitializedArrayGOsEE(_M0L6_2atmpS2114);
    moonbit_decref(_M0L6_2atmpS2114);
    return _result_4358;
  }
}

struct _M0TUsfE** _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(
  struct _M0TUsfE** _M0L3srcS109,
  int32_t _M0L13allocate__lenS105,
  int32_t _M0L3lenS106,
  int32_t _M0L11src__offsetS107,
  int32_t _M0L11dst__offsetS108
) {
  int32_t _if__result_4359;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS105 >= 0) {
    if (_M0L3lenS106 >= 0) {
      if (_M0L11src__offsetS107 >= 0) {
        if (_M0L11dst__offsetS108 >= 0) {
          int32_t _M0L6_2atmpS2117 = _M0L11src__offsetS107 + _M0L3lenS106;
          int32_t _M0L6_2atmpS2118;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2118
          = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS109);
          if (_M0L6_2atmpS2117 <= _M0L6_2atmpS2118) {
            int32_t _M0L6_2atmpS2116 = _M0L11dst__offsetS108 + _M0L3lenS106;
            _if__result_4359 = _M0L6_2atmpS2116 <= _M0L13allocate__lenS105;
          } else {
            _if__result_4359 = 0;
          }
        } else {
          _if__result_4359 = 0;
        }
      } else {
        _if__result_4359 = 0;
      }
    } else {
      _if__result_4359 = 0;
    }
  } else {
    _if__result_4359 = 0;
  }
  if (_if__result_4359) {
    moonbit_incref(_M0L3srcS109);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (struct _M0TUsfE**)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS105, 0, _M0L3srcS109, _M0L11src__offsetS107, _M0L11dst__offsetS108, _M0L3lenS106);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS110;
    int32_t _M0L6_2atmpS2120;
    moonbit_string_t _M0L6_2atmpS2119;
    struct _M0TUsfE** _result_4360;
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L18_2astring__builderS110
    = _M0MPB13StringBuilder21StringBuilder_2einner(89);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_111.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L13allocate__lenS105);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_112.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L11src__offsetS107);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_113.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L11dst__offsetS108);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_114.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L3lenS106);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS110, (moonbit_string_t)moonbit_string_literal_115.data);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2120 = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS109);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L6_2atmpS2120);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2119
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS110);
    moonbit_decref(_M0L18_2astring__builderS110);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4360
    = _M0FPC15abort5abortGRPB18UninitializedArrayGUsfEEE(_M0L6_2atmpS2119);
    moonbit_decref(_M0L6_2atmpS2119);
    return _result_4360;
  }
}

int32_t _M0MPB13StringBuilder13write__objectGsE(
  struct _M0TPB13StringBuilder* _M0L4selfS84,
  moonbit_string_t _M0L3objS83
) {
  struct _M0TPB6Logger _M0L6_2atmpS2101;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS84);
  _M0L6_2atmpS2101
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS84
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGsE(_M0L3objS83, _M0L6_2atmpS2101);
  if (_M0L6_2atmpS2101.$1) {
    moonbit_decref(_M0L6_2atmpS2101.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGiE(
  struct _M0TPB13StringBuilder* _M0L4selfS86,
  int32_t _M0L3objS85
) {
  struct _M0TPB6Logger _M0L6_2atmpS2102;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS86);
  _M0L6_2atmpS2102
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS86
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGiE(_M0L3objS85, _M0L6_2atmpS2102);
  if (_M0L6_2atmpS2102.$1) {
    moonbit_decref(_M0L6_2atmpS2102.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGbE(
  struct _M0TPB13StringBuilder* _M0L4selfS88,
  int32_t _M0L3objS87
) {
  struct _M0TPB6Logger _M0L6_2atmpS2103;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS88);
  _M0L6_2atmpS2103
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS88
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGbE(_M0L3objS87, _M0L6_2atmpS2103);
  if (_M0L6_2atmpS2103.$1) {
    moonbit_decref(_M0L6_2atmpS2103.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGfE(
  struct _M0TPB13StringBuilder* _M0L4selfS90,
  float _M0L3objS89
) {
  struct _M0TPB6Logger _M0L6_2atmpS2104;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS90);
  _M0L6_2atmpS2104
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS90
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGfE(_M0L3objS89, _M0L6_2atmpS2104);
  if (_M0L6_2atmpS2104.$1) {
    moonbit_decref(_M0L6_2atmpS2104.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGmE(
  struct _M0TPB13StringBuilder* _M0L4selfS92,
  uint64_t _M0L3objS91
) {
  struct _M0TPB6Logger _M0L6_2atmpS2105;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS92);
  _M0L6_2atmpS2105
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS92
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGmE(_M0L3objS91, _M0L6_2atmpS2105);
  if (_M0L6_2atmpS2105.$1) {
    moonbit_decref(_M0L6_2atmpS2105.$1);
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
  int32_t _if__result_4361;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS14 == _M0L3srcS15) {
    _if__result_4361 = _M0L11dst__offsetS16 < _M0L11src__offsetS17;
  } else {
    _if__result_4361 = 0;
  }
  if (_if__result_4361) {
    int32_t _M0L1iS18 = 0;
    while (1) {
      if (_M0L1iS18 < _M0L3lenS19) {
        int32_t _M0L6_2atmpS2065 = _M0L11dst__offsetS16 + _M0L1iS18;
        int32_t _M0L6_2atmpS2067 = _M0L11src__offsetS17 + _M0L1iS18;
        int32_t _M0L6_2atmpS2066;
        int32_t _M0L6_2atmpS2068;
        if (
          _M0L6_2atmpS2067 < 0
          || _M0L6_2atmpS2067 >= Moonbit_array_length(_M0L3srcS15)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2066 = (int32_t)_M0L3srcS15[_M0L6_2atmpS2067];
        if (
          _M0L6_2atmpS2065 < 0
          || _M0L6_2atmpS2065 >= Moonbit_array_length(_M0L3dstS14)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS14[_M0L6_2atmpS2065] = _M0L6_2atmpS2066;
        _M0L6_2atmpS2068 = _M0L1iS18 + 1;
        _M0L1iS18 = _M0L6_2atmpS2068;
        continue;
      } else {
        moonbit_decref(_M0L3srcS15);
        moonbit_decref(_M0L3dstS14);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2073 = _M0L3lenS19 - 1;
    int32_t _M0L1iS21 = _M0L6_2atmpS2073;
    while (1) {
      if (_M0L1iS21 >= 0) {
        int32_t _M0L6_2atmpS2069 = _M0L11dst__offsetS16 + _M0L1iS21;
        int32_t _M0L6_2atmpS2071 = _M0L11src__offsetS17 + _M0L1iS21;
        int32_t _M0L6_2atmpS2070;
        int32_t _M0L6_2atmpS2072;
        if (
          _M0L6_2atmpS2071 < 0
          || _M0L6_2atmpS2071 >= Moonbit_array_length(_M0L3srcS15)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2070 = (int32_t)_M0L3srcS15[_M0L6_2atmpS2071];
        if (
          _M0L6_2atmpS2069 < 0
          || _M0L6_2atmpS2069 >= Moonbit_array_length(_M0L3dstS14)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS14[_M0L6_2atmpS2069] = _M0L6_2atmpS2070;
        _M0L6_2atmpS2072 = _M0L1iS21 - 1;
        _M0L1iS21 = _M0L6_2atmpS2072;
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
  int32_t _if__result_4364;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS23 == _M0L3srcS24) {
    _if__result_4364 = _M0L11dst__offsetS25 < _M0L11src__offsetS26;
  } else {
    _if__result_4364 = 0;
  }
  if (_if__result_4364) {
    int32_t _M0L1iS27 = 0;
    while (1) {
      if (_M0L1iS27 < _M0L3lenS28) {
        int32_t _M0L6_2atmpS2074 = _M0L11dst__offsetS25 + _M0L1iS27;
        int32_t _M0L6_2atmpS2076 = _M0L11src__offsetS26 + _M0L1iS27;
        moonbit_string_t _M0L6_2atmpS2075;
        moonbit_string_t _M0L6_2aoldS3996;
        int32_t _M0L6_2atmpS2077;
        if (
          _M0L6_2atmpS2076 < 0
          || _M0L6_2atmpS2076 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2075 = (moonbit_string_t)_M0L3srcS24[_M0L6_2atmpS2076];
        if (
          _M0L6_2atmpS2074 < 0
          || _M0L6_2atmpS2074 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3996 = (moonbit_string_t)_M0L3dstS23[_M0L6_2atmpS2074];
        moonbit_incref(_M0L6_2atmpS2075);
        moonbit_decref(_M0L6_2aoldS3996);
        _M0L3dstS23[_M0L6_2atmpS2074] = _M0L6_2atmpS2075;
        _M0L6_2atmpS2077 = _M0L1iS27 + 1;
        _M0L1iS27 = _M0L6_2atmpS2077;
        continue;
      } else {
        moonbit_decref(_M0L3srcS24);
        moonbit_decref(_M0L3dstS23);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2082 = _M0L3lenS28 - 1;
    int32_t _M0L1iS30 = _M0L6_2atmpS2082;
    while (1) {
      if (_M0L1iS30 >= 0) {
        int32_t _M0L6_2atmpS2078 = _M0L11dst__offsetS25 + _M0L1iS30;
        int32_t _M0L6_2atmpS2080 = _M0L11src__offsetS26 + _M0L1iS30;
        moonbit_string_t _M0L6_2atmpS2079;
        moonbit_string_t _M0L6_2aoldS3998;
        int32_t _M0L6_2atmpS2081;
        if (
          _M0L6_2atmpS2080 < 0
          || _M0L6_2atmpS2080 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2079 = (moonbit_string_t)_M0L3srcS24[_M0L6_2atmpS2080];
        if (
          _M0L6_2atmpS2078 < 0
          || _M0L6_2atmpS2078 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3998 = (moonbit_string_t)_M0L3dstS23[_M0L6_2atmpS2078];
        moonbit_incref(_M0L6_2atmpS2079);
        moonbit_decref(_M0L6_2aoldS3998);
        _M0L3dstS23[_M0L6_2atmpS2078] = _M0L6_2atmpS2079;
        _M0L6_2atmpS2081 = _M0L1iS30 - 1;
        _M0L1iS30 = _M0L6_2atmpS2081;
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
  int32_t _if__result_4367;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS32 == _M0L3srcS33) {
    _if__result_4367 = _M0L11dst__offsetS34 < _M0L11src__offsetS35;
  } else {
    _if__result_4367 = 0;
  }
  if (_if__result_4367) {
    int32_t _M0L1iS36 = 0;
    while (1) {
      if (_M0L1iS36 < _M0L3lenS37) {
        int32_t _M0L6_2atmpS2083 = _M0L11dst__offsetS34 + _M0L1iS36;
        int32_t _M0L6_2atmpS2085 = _M0L11src__offsetS35 + _M0L1iS36;
        moonbit_string_t _M0L6_2atmpS2084;
        moonbit_string_t _M0L6_2aoldS4000;
        int32_t _M0L6_2atmpS2086;
        if (
          _M0L6_2atmpS2085 < 0
          || _M0L6_2atmpS2085 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2084 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS2085];
        if (
          _M0L6_2atmpS2083 < 0
          || _M0L6_2atmpS2083 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4000 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS2083];
        if (_M0L6_2atmpS2084) {
          moonbit_incref(_M0L6_2atmpS2084);
        }
        if (_M0L6_2aoldS4000) {
          moonbit_decref(_M0L6_2aoldS4000);
        }
        _M0L3dstS32[_M0L6_2atmpS2083] = _M0L6_2atmpS2084;
        _M0L6_2atmpS2086 = _M0L1iS36 + 1;
        _M0L1iS36 = _M0L6_2atmpS2086;
        continue;
      } else {
        moonbit_decref(_M0L3srcS33);
        moonbit_decref(_M0L3dstS32);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2091 = _M0L3lenS37 - 1;
    int32_t _M0L1iS39 = _M0L6_2atmpS2091;
    while (1) {
      if (_M0L1iS39 >= 0) {
        int32_t _M0L6_2atmpS2087 = _M0L11dst__offsetS34 + _M0L1iS39;
        int32_t _M0L6_2atmpS2089 = _M0L11src__offsetS35 + _M0L1iS39;
        moonbit_string_t _M0L6_2atmpS2088;
        moonbit_string_t _M0L6_2aoldS4002;
        int32_t _M0L6_2atmpS2090;
        if (
          _M0L6_2atmpS2089 < 0
          || _M0L6_2atmpS2089 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2088 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS2089];
        if (
          _M0L6_2atmpS2087 < 0
          || _M0L6_2atmpS2087 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4002 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS2087];
        if (_M0L6_2atmpS2088) {
          moonbit_incref(_M0L6_2atmpS2088);
        }
        if (_M0L6_2aoldS4002) {
          moonbit_decref(_M0L6_2aoldS4002);
        }
        _M0L3dstS32[_M0L6_2atmpS2087] = _M0L6_2atmpS2088;
        _M0L6_2atmpS2090 = _M0L1iS39 - 1;
        _M0L1iS39 = _M0L6_2atmpS2090;
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
  int32_t _if__result_4370;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS41 == _M0L3srcS42) {
    _if__result_4370 = _M0L11dst__offsetS43 < _M0L11src__offsetS44;
  } else {
    _if__result_4370 = 0;
  }
  if (_if__result_4370) {
    int32_t _M0L1iS45 = 0;
    while (1) {
      if (_M0L1iS45 < _M0L3lenS46) {
        int32_t _M0L6_2atmpS2092 = _M0L11dst__offsetS43 + _M0L1iS45;
        int32_t _M0L6_2atmpS2094 = _M0L11src__offsetS44 + _M0L1iS45;
        struct _M0TUsfE* _M0L6_2atmpS2093;
        struct _M0TUsfE* _M0L6_2aoldS4004;
        int32_t _M0L6_2atmpS2095;
        if (
          _M0L6_2atmpS2094 < 0
          || _M0L6_2atmpS2094 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2093 = (struct _M0TUsfE*)_M0L3srcS42[_M0L6_2atmpS2094];
        if (
          _M0L6_2atmpS2092 < 0
          || _M0L6_2atmpS2092 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4004 = (struct _M0TUsfE*)_M0L3dstS41[_M0L6_2atmpS2092];
        if (_M0L6_2atmpS2093) {
          moonbit_incref(_M0L6_2atmpS2093);
        }
        if (_M0L6_2aoldS4004) {
          moonbit_decref(_M0L6_2aoldS4004);
        }
        _M0L3dstS41[_M0L6_2atmpS2092] = _M0L6_2atmpS2093;
        _M0L6_2atmpS2095 = _M0L1iS45 + 1;
        _M0L1iS45 = _M0L6_2atmpS2095;
        continue;
      } else {
        moonbit_decref(_M0L3srcS42);
        moonbit_decref(_M0L3dstS41);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2100 = _M0L3lenS46 - 1;
    int32_t _M0L1iS48 = _M0L6_2atmpS2100;
    while (1) {
      if (_M0L1iS48 >= 0) {
        int32_t _M0L6_2atmpS2096 = _M0L11dst__offsetS43 + _M0L1iS48;
        int32_t _M0L6_2atmpS2098 = _M0L11src__offsetS44 + _M0L1iS48;
        struct _M0TUsfE* _M0L6_2atmpS2097;
        struct _M0TUsfE* _M0L6_2aoldS4006;
        int32_t _M0L6_2atmpS2099;
        if (
          _M0L6_2atmpS2098 < 0
          || _M0L6_2atmpS2098 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2097 = (struct _M0TUsfE*)_M0L3srcS42[_M0L6_2atmpS2098];
        if (
          _M0L6_2atmpS2096 < 0
          || _M0L6_2atmpS2096 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4006 = (struct _M0TUsfE*)_M0L3dstS41[_M0L6_2atmpS2096];
        if (_M0L6_2atmpS2097) {
          moonbit_incref(_M0L6_2atmpS2097);
        }
        if (_M0L6_2aoldS4006) {
          moonbit_decref(_M0L6_2aoldS4006);
        }
        _M0L3dstS41[_M0L6_2atmpS2096] = _M0L6_2atmpS2097;
        _M0L6_2atmpS2099 = _M0L1iS48 - 1;
        _M0L1iS48 = _M0L6_2atmpS2099;
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
  uint32_t _M0L6_2atmpS2064;
  uint32_t _M0L6_2atmpS2063;
  uint32_t _M0L6_2atmpS2062;
  #line 465 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2064 = _M0L5inputS10 * 3266489917u;
  _M0L6_2atmpS2063 = _M0L3accS9 + _M0L6_2atmpS2064;
  #line 466 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2062 = _M0FPB4rotl(_M0L6_2atmpS2063, 17);
  return _M0L6_2atmpS2062 * 668265263u;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS7, int32_t _M0L1rS8) {
  uint32_t _M0L6_2atmpS2059;
  int32_t _M0L6_2atmpS2061;
  uint32_t _M0L6_2atmpS2060;
  #line 475 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2059 = _M0L1xS7 << (_M0L1rS8 & 31);
  _M0L6_2atmpS2061 = 32 - _M0L1rS8;
  _M0L6_2atmpS2060 = _M0L1xS7 >> (_M0L6_2atmpS2061 & 31);
  return _M0L6_2atmpS2059 | _M0L6_2atmpS2060;
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
  void* _M0L11_2aobj__ptrS1945,
  struct _M0TPB4Show _M0L8_2aparamS1944
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1943 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1945;
  _M0IP016_24default__implPB6Logger5writeGRPB13StringBuilderE(_M0L7_2aselfS1943, _M0L8_2aparamS1944);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger84write__string__interpolation_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1942,
  struct _M0TPB4Show _M0L8_2aparamS1941
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1940 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1942;
  _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(_M0L7_2aselfS1940, _M0L8_2aparamS1941);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1939,
  int32_t _M0L8_2aparamS1938
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1937 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1939;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1937, _M0L8_2aparamS1938);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1936,
  struct _M0TPC16string10StringView _M0L8_2aparamS1935
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1934 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1936;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1934, _M0L8_2aparamS1935);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1933,
  moonbit_string_t _M0L8_2aparamS1930,
  int32_t _M0L8_2aparamS1931,
  int32_t _M0L8_2aparamS1932
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1929 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1933;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1929, _M0L8_2aparamS1930, _M0L8_2aparamS1931, _M0L8_2aparamS1932);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1928,
  moonbit_string_t _M0L8_2aparamS1927
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1926 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1928;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1926, _M0L8_2aparamS1927);
  return 0;
}

void moonbit_init() {
  moonbit_layout_table = moonbit_layout_table_data;
}

int main(int argc, char** argv) {
  struct _M0TP19moonbitDB8Database* _M0L2dbS1870;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1871;
  moonbit_string_t _M0L6_2atmpS1948;
  moonbit_string_t _M0L6_2atmpS1947;
  moonbit_string_t _M0L6_2atmpS1946;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1872;
  moonbit_string_t _M0L6_2atmpS1951;
  moonbit_string_t _M0L6_2atmpS1950;
  moonbit_string_t _M0L6_2atmpS1949;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1873;
  int32_t _M0L6_2atmpS1953;
  moonbit_string_t _M0L6_2atmpS1952;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1874;
  int32_t _M0L6_2atmpS1955;
  moonbit_string_t _M0L6_2atmpS1954;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1875;
  moonbit_string_t _M0L6_2atmpS1958;
  moonbit_string_t _M0L6_2atmpS1957;
  moonbit_string_t _M0L6_2atmpS1956;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1876;
  int64_t _M0L6_2atmpS1961;
  moonbit_string_t _M0L6_2atmpS1960;
  moonbit_string_t _M0L6_2atmpS1959;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1877;
  int64_t _M0L6_2atmpS1964;
  moonbit_string_t _M0L6_2atmpS1963;
  moonbit_string_t _M0L6_2atmpS1962;
  int32_t _M0L6_2atmpS1965;
  int32_t _M0L6_2atmpS1966;
  int32_t _M0L6_2atmpS1967;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1878;
  moonbit_string_t _M0L6_2atmpS1970;
  moonbit_string_t _M0L6_2atmpS1969;
  moonbit_string_t _M0L6_2atmpS1968;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1879;
  int32_t _M0L6_2atmpS1972;
  moonbit_string_t _M0L6_2atmpS1971;
  struct _M0TPB3MapGssE* _M0L3allS1880;
  moonbit_string_t* _M0L6_2atmpS2058;
  struct _M0TPB5ArrayGsE* _M0L6fieldsS1881;
  struct _M0TPB4IterGUssEE* _M0L5_2aitS1882;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1890;
  moonbit_string_t _M0L7_2abindS1891;
  int32_t _M0L6_2atmpS1976;
  struct _M0TPC16string10StringView _M0L6_2atmpS1975;
  moonbit_string_t _M0L6_2atmpS1974;
  moonbit_string_t _M0L6_2atmpS1973;
  int32_t _M0L6_2atmpS1977;
  int32_t _M0L6_2atmpS1978;
  int32_t _M0L6_2atmpS1979;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1892;
  int32_t _M0L6_2atmpS1981;
  moonbit_string_t _M0L6_2atmpS1980;
  struct _M0TPB5ArrayGsE* _M0L5tasksS1893;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1894;
  moonbit_string_t _M0L7_2abindS1895;
  int32_t _M0L6_2atmpS1985;
  struct _M0TPC16string10StringView _M0L6_2atmpS1984;
  moonbit_string_t _M0L6_2atmpS1983;
  moonbit_string_t _M0L6_2atmpS1982;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1896;
  moonbit_string_t _M0L6_2atmpS1988;
  moonbit_string_t _M0L6_2atmpS1987;
  moonbit_string_t _M0L6_2atmpS1986;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1897;
  moonbit_string_t _M0L6_2atmpS1991;
  moonbit_string_t _M0L6_2atmpS1990;
  moonbit_string_t _M0L6_2atmpS1989;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1898;
  moonbit_string_t _M0L6_2atmpS1994;
  moonbit_string_t _M0L6_2atmpS1993;
  moonbit_string_t _M0L6_2atmpS1992;
  int32_t _M0L6_2atmpS1995;
  int32_t _M0L6_2atmpS1996;
  int32_t _M0L6_2atmpS1997;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1899;
  int32_t _M0L6_2atmpS1999;
  moonbit_string_t _M0L6_2atmpS1998;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1900;
  int32_t _M0L6_2atmpS2001;
  moonbit_string_t _M0L6_2atmpS2000;
  struct _M0TPB5ArrayGsE* _M0L4tagsS1901;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1902;
  moonbit_string_t _M0L7_2abindS1903;
  int32_t _M0L6_2atmpS2005;
  struct _M0TPC16string10StringView _M0L6_2atmpS2004;
  moonbit_string_t _M0L6_2atmpS2003;
  moonbit_string_t _M0L6_2atmpS2002;
  int32_t _M0L6_2atmpS2006;
  int32_t _M0L6_2atmpS2007;
  int32_t _M0L6_2atmpS2008;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1904;
  int32_t _M0L6_2atmpS2010;
  moonbit_string_t _M0L6_2atmpS2009;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1905;
  void* _M0L6_2atmpS2013;
  moonbit_string_t _M0L6_2atmpS2012;
  moonbit_string_t _M0L6_2atmpS2011;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1906;
  int64_t _M0L6_2atmpS2016;
  moonbit_string_t _M0L6_2atmpS2015;
  moonbit_string_t _M0L6_2atmpS2014;
  struct _M0TPB5ArrayGsE* _M0L3topS1907;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1908;
  moonbit_string_t _M0L7_2abindS1909;
  int32_t _M0L6_2atmpS2020;
  struct _M0TPC16string10StringView _M0L6_2atmpS2019;
  moonbit_string_t _M0L6_2atmpS2018;
  moonbit_string_t _M0L6_2atmpS2017;
  int32_t _M0L6_2atmpS2021;
  int32_t _M0L6_2atmpS2022;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1910;
  int32_t _M0L6_2atmpS2024;
  moonbit_string_t _M0L6_2atmpS2023;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1911;
  int32_t _M0L6_2atmpS2026;
  moonbit_string_t _M0L6_2atmpS2025;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1912;
  int32_t _M0L6_2atmpS2028;
  moonbit_string_t _M0L6_2atmpS2027;
  moonbit_string_t* _M0L6_2atmpS2033;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2030;
  moonbit_string_t* _M0L6_2atmpS2032;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2031;
  int32_t _M0L6_2atmpS2029;
  moonbit_string_t* _M0L6_2atmpS2057;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2056;
  struct _M0TPB5ArrayGOsE* _M0L4valsS1913;
  int32_t _M0L1iS1914;
  moonbit_string_t* _M0L6_2atmpS2055;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2054;
  int32_t _M0L7deletedS1917;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1918;
  moonbit_string_t _M0L6_2atmpS2039;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1919;
  int32_t _M0L6_2atmpS2041;
  moonbit_string_t _M0L6_2atmpS2040;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1920;
  moonbit_string_t _M0L6_2atmpS2043;
  moonbit_string_t _M0L6_2atmpS2042;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1921;
  moonbit_string_t _M0L6_2atmpS2045;
  moonbit_string_t _M0L6_2atmpS2044;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1922;
  moonbit_string_t _M0L6_2atmpS2047;
  moonbit_string_t _M0L6_2atmpS2046;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1923;
  moonbit_string_t _M0L6_2atmpS2049;
  moonbit_string_t _M0L6_2atmpS2048;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1924;
  moonbit_string_t _M0L6_2atmpS2051;
  moonbit_string_t _M0L6_2atmpS2050;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1925;
  moonbit_string_t _M0L6_2atmpS2053;
  moonbit_string_t _M0L6_2atmpS2052;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  #line 23 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L2dbS1870 = _M0MP19moonbitDB8Database3new();
  #line 25 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_116.data);
  #line 26 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MP19moonbitDB8Database3set(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_117.data, (moonbit_string_t)moonbit_string_literal_118.data);
  #line 27 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MP19moonbitDB8Database3set(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_119.data, (moonbit_string_t)moonbit_string_literal_120.data);
  #line 28 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1871
  = _M0MPB13StringBuilder21StringBuilder_2einner(10);
  #line 28 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1871, (moonbit_string_t)moonbit_string_literal_121.data);
  #line 28 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1948
  = _M0MP19moonbitDB8Database3get(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_117.data);
  #line 28 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1947
  = _M0FP39moonbitDB8examples12basic__usage9show__opt(_M0L6_2atmpS1948);
  if (_M0L6_2atmpS1948) {
    moonbit_decref(_M0L6_2atmpS1948);
  }
  #line 28 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1871, _M0L6_2atmpS1947);
  moonbit_decref(_M0L6_2atmpS1947);
  #line 28 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1946
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1871);
  moonbit_decref(_M0L18_2astring__builderS1871);
  #line 28 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1946);
  moonbit_decref(_M0L6_2atmpS1946);
  #line 29 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1872
  = _M0MPB13StringBuilder21StringBuilder_2einner(9);
  #line 29 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1872, (moonbit_string_t)moonbit_string_literal_122.data);
  #line 29 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1951
  = _M0MP19moonbitDB8Database3get(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_119.data);
  #line 29 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1950
  = _M0FP39moonbitDB8examples12basic__usage9show__opt(_M0L6_2atmpS1951);
  if (_M0L6_2atmpS1951) {
    moonbit_decref(_M0L6_2atmpS1951);
  }
  #line 29 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1872, _M0L6_2atmpS1950);
  moonbit_decref(_M0L6_2atmpS1950);
  #line 29 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1949
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1872);
  moonbit_decref(_M0L18_2astring__builderS1872);
  #line 29 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1949);
  moonbit_decref(_M0L6_2atmpS1949);
  #line 30 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1873
  = _M0MPB13StringBuilder21StringBuilder_2einner(13);
  #line 30 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1873, (moonbit_string_t)moonbit_string_literal_123.data);
  #line 30 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1953
  = _M0MP19moonbitDB8Database6strlen(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_117.data);
  #line 30 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1873, _M0L6_2atmpS1953);
  #line 30 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1952
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1873);
  moonbit_decref(_M0L18_2astring__builderS1873);
  #line 30 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1952);
  moonbit_decref(_M0L6_2atmpS1952);
  #line 31 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1874
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 31 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1874, (moonbit_string_t)moonbit_string_literal_124.data);
  #line 31 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1955
  = _M0MP19moonbitDB8Database6append(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_117.data, (moonbit_string_t)moonbit_string_literal_125.data);
  #line 31 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1874, _M0L6_2atmpS1955);
  #line 31 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1954
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1874);
  moonbit_decref(_M0L18_2astring__builderS1874);
  #line 31 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1954);
  moonbit_decref(_M0L6_2atmpS1954);
  #line 32 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1875
  = _M0MPB13StringBuilder21StringBuilder_2einner(23);
  #line 32 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1875, (moonbit_string_t)moonbit_string_literal_126.data);
  #line 32 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1958
  = _M0MP19moonbitDB8Database3get(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_117.data);
  #line 32 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1957
  = _M0FP39moonbitDB8examples12basic__usage9show__opt(_M0L6_2atmpS1958);
  if (_M0L6_2atmpS1958) {
    moonbit_decref(_M0L6_2atmpS1958);
  }
  #line 32 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1875, _M0L6_2atmpS1957);
  moonbit_decref(_M0L6_2atmpS1957);
  #line 32 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1956
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1875);
  moonbit_decref(_M0L18_2astring__builderS1875);
  #line 32 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1956);
  moonbit_decref(_M0L6_2atmpS1956);
  #line 33 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1876
  = _M0MPB13StringBuilder21StringBuilder_2einner(10);
  #line 33 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1876, (moonbit_string_t)moonbit_string_literal_127.data);
  #line 33 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1961
  = _M0MP19moonbitDB8Database4incr(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_119.data);
  #line 33 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1960
  = _M0FP39moonbitDB8examples12basic__usage14show__opt__int(_M0L6_2atmpS1961);
  #line 33 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1876, _M0L6_2atmpS1960);
  moonbit_decref(_M0L6_2atmpS1960);
  #line 33 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1959
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1876);
  moonbit_decref(_M0L18_2astring__builderS1876);
  #line 33 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1959);
  moonbit_decref(_M0L6_2atmpS1959);
  #line 34 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1877
  = _M0MPB13StringBuilder21StringBuilder_2einner(10);
  #line 34 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1877, (moonbit_string_t)moonbit_string_literal_128.data);
  #line 34 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1964
  = _M0MP19moonbitDB8Database4decr(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_119.data);
  #line 34 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1963
  = _M0FP39moonbitDB8examples12basic__usage14show__opt__int(_M0L6_2atmpS1964);
  #line 34 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1877, _M0L6_2atmpS1963);
  moonbit_decref(_M0L6_2atmpS1963);
  #line 34 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1962
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1877);
  moonbit_decref(_M0L18_2astring__builderS1877);
  #line 34 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1962);
  moonbit_decref(_M0L6_2atmpS1962);
  #line 36 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_129.data);
  #line 37 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1965
  = _M0MP19moonbitDB8Database4hset(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_130.data, (moonbit_string_t)moonbit_string_literal_131.data, (moonbit_string_t)moonbit_string_literal_132.data);
  #line 38 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1966
  = _M0MP19moonbitDB8Database4hset(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_130.data, (moonbit_string_t)moonbit_string_literal_133.data, (moonbit_string_t)moonbit_string_literal_134.data);
  #line 39 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1967
  = _M0MP19moonbitDB8Database4hset(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_130.data, (moonbit_string_t)moonbit_string_literal_119.data, (moonbit_string_t)moonbit_string_literal_120.data);
  #line 40 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1878
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 40 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1878, (moonbit_string_t)moonbit_string_literal_135.data);
  #line 40 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1970
  = _M0MP19moonbitDB8Database4hget(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_130.data, (moonbit_string_t)moonbit_string_literal_131.data);
  #line 40 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1969
  = _M0FP39moonbitDB8examples12basic__usage9show__opt(_M0L6_2atmpS1970);
  if (_M0L6_2atmpS1970) {
    moonbit_decref(_M0L6_2atmpS1970);
  }
  #line 40 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1878, _M0L6_2atmpS1969);
  moonbit_decref(_M0L6_2atmpS1969);
  #line 40 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1968
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1878);
  moonbit_decref(_M0L18_2astring__builderS1878);
  #line 40 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1968);
  moonbit_decref(_M0L6_2atmpS1968);
  #line 41 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1879
  = _M0MPB13StringBuilder21StringBuilder_2einner(13);
  #line 41 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1879, (moonbit_string_t)moonbit_string_literal_136.data);
  #line 41 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1972
  = _M0MP19moonbitDB8Database4hlen(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_130.data);
  #line 41 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1879, _M0L6_2atmpS1972);
  #line 41 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1971
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1879);
  moonbit_decref(_M0L18_2astring__builderS1879);
  #line 41 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1971);
  moonbit_decref(_M0L6_2atmpS1971);
  #line 42 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L3allS1880
  = _M0MP19moonbitDB8Database7hgetall(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_130.data);
  _M0L6_2atmpS2058 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6fieldsS1881
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6fieldsS1881)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6fieldsS1881->$0 = _M0L6_2atmpS2058;
  _M0L6fieldsS1881->$1 = 0;
  #line 43 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L5_2aitS1882 = _M0MPB3Map5iter2GssE(_M0L3allS1880);
  moonbit_decref(_M0L3allS1880);
  while (1) {
    moonbit_string_t _M0L1fS1884;
    struct _M0TUssE* _M0L7_2abindS1886;
    #line 44 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
    _M0L7_2abindS1886 = _M0MPB5Iter24nextGssE(_M0L5_2aitS1882);
    if (_M0L7_2abindS1886 == 0) {
      if (_M0L7_2abindS1886) {
        moonbit_decref(_M0L7_2abindS1886);
      }
      moonbit_decref(_M0L5_2aitS1882);
    } else {
      struct _M0TUssE* _M0L7_2aSomeS1887 = _M0L7_2abindS1886;
      struct _M0TUssE* _M0L4_2axS1888 = _M0L7_2aSomeS1887;
      moonbit_string_t _M0L8_2afieldS4008 = _M0L4_2axS1888->$0;
      int32_t _M0L6_2acntS4082 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1888));
      moonbit_string_t _M0L4_2afS1889;
      if (_M0L6_2acntS4082 > 1) {
        int32_t _M0L11_2anew__cntS4084 = _M0L6_2acntS4082 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1888), _M0L11_2anew__cntS4084);
        moonbit_incref(_M0L8_2afieldS4008);
      } else if (_M0L6_2acntS4082 == 1) {
        moonbit_string_t _M0L8_2afieldS4083 = _M0L4_2axS1888->$1;
        moonbit_decref(_M0L8_2afieldS4083);
        #line 44 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
        moonbit_free(_M0L4_2axS1888);
      }
      _M0L4_2afS1889 = _M0L8_2afieldS4008;
      _M0L1fS1884 = _M0L4_2afS1889;
      goto join_1883;
    }
    goto joinlet_4374;
    join_1883:;
    #line 45 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
    _M0MPC15array5Array4pushGsE(_M0L6fieldsS1881, _M0L1fS1884);
    moonbit_decref(_M0L1fS1884);
    continue;
    joinlet_4374:;
    break;
  }
  #line 47 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1890
  = _M0MPB13StringBuilder21StringBuilder_2einner(18);
  #line 47 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1890, (moonbit_string_t)moonbit_string_literal_137.data);
  _M0L7_2abindS1891 = (moonbit_string_t)moonbit_string_literal_138.data;
  _M0L6_2atmpS1976 = Moonbit_array_length(_M0L7_2abindS1891);
  _M0L6_2atmpS1975
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS1891, .$1 = 0, .$2 = _M0L6_2atmpS1976
  };
  #line 47 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1974
  = _M0MPC15array5Array4joinGsE(_M0L6fieldsS1881, _M0L6_2atmpS1975);
  moonbit_decref(_M0L6fieldsS1881);
  moonbit_decref(_M0L6_2atmpS1975.$0);
  #line 47 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1890, _M0L6_2atmpS1974);
  moonbit_decref(_M0L6_2atmpS1974);
  #line 47 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1890, (moonbit_string_t)moonbit_string_literal_139.data);
  #line 47 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1973
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1890);
  moonbit_decref(_M0L18_2astring__builderS1890);
  #line 47 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1973);
  moonbit_decref(_M0L6_2atmpS1973);
  #line 49 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_140.data);
  #line 50 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1977
  = _M0MP19moonbitDB8Database5rpush(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_141.data, (moonbit_string_t)moonbit_string_literal_142.data);
  #line 51 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1978
  = _M0MP19moonbitDB8Database5rpush(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_141.data, (moonbit_string_t)moonbit_string_literal_143.data);
  #line 52 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1979
  = _M0MP19moonbitDB8Database5rpush(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_141.data, (moonbit_string_t)moonbit_string_literal_144.data);
  #line 53 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1892
  = _M0MPB13StringBuilder21StringBuilder_2einner(12);
  #line 53 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1892, (moonbit_string_t)moonbit_string_literal_145.data);
  #line 53 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1981
  = _M0MP19moonbitDB8Database4llen(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_141.data);
  #line 53 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1892, _M0L6_2atmpS1981);
  #line 53 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1980
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1892);
  moonbit_decref(_M0L18_2astring__builderS1892);
  #line 53 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1980);
  moonbit_decref(_M0L6_2atmpS1980);
  #line 54 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L5tasksS1893
  = _M0MP19moonbitDB8Database6lrange(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_141.data, 0, -1);
  #line 55 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1894
  = _M0MPB13StringBuilder21StringBuilder_2einner(21);
  #line 55 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1894, (moonbit_string_t)moonbit_string_literal_146.data);
  _M0L7_2abindS1895 = (moonbit_string_t)moonbit_string_literal_138.data;
  _M0L6_2atmpS1985 = Moonbit_array_length(_M0L7_2abindS1895);
  _M0L6_2atmpS1984
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS1895, .$1 = 0, .$2 = _M0L6_2atmpS1985
  };
  #line 55 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1983
  = _M0MPC15array5Array4joinGsE(_M0L5tasksS1893, _M0L6_2atmpS1984);
  moonbit_decref(_M0L5tasksS1893);
  moonbit_decref(_M0L6_2atmpS1984.$0);
  #line 55 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1894, _M0L6_2atmpS1983);
  moonbit_decref(_M0L6_2atmpS1983);
  #line 55 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1894, (moonbit_string_t)moonbit_string_literal_139.data);
  #line 55 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1982
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1894);
  moonbit_decref(_M0L18_2astring__builderS1894);
  #line 55 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1982);
  moonbit_decref(_M0L6_2atmpS1982);
  #line 56 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1896
  = _M0MPB13StringBuilder21StringBuilder_2einner(12);
  #line 56 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1896, (moonbit_string_t)moonbit_string_literal_147.data);
  #line 56 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1988
  = _M0MP19moonbitDB8Database4lpop(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_141.data);
  #line 56 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1987
  = _M0FP39moonbitDB8examples12basic__usage9show__opt(_M0L6_2atmpS1988);
  if (_M0L6_2atmpS1988) {
    moonbit_decref(_M0L6_2atmpS1988);
  }
  #line 56 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1896, _M0L6_2atmpS1987);
  moonbit_decref(_M0L6_2atmpS1987);
  #line 56 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1986
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1896);
  moonbit_decref(_M0L18_2astring__builderS1896);
  #line 56 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1986);
  moonbit_decref(_M0L6_2atmpS1986);
  #line 57 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1897
  = _M0MPB13StringBuilder21StringBuilder_2einner(12);
  #line 57 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1897, (moonbit_string_t)moonbit_string_literal_148.data);
  #line 57 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1991
  = _M0MP19moonbitDB8Database4rpop(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_141.data);
  #line 57 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1990
  = _M0FP39moonbitDB8examples12basic__usage9show__opt(_M0L6_2atmpS1991);
  if (_M0L6_2atmpS1991) {
    moonbit_decref(_M0L6_2atmpS1991);
  }
  #line 57 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1897, _M0L6_2atmpS1990);
  moonbit_decref(_M0L6_2atmpS1990);
  #line 57 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1989
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1897);
  moonbit_decref(_M0L18_2astring__builderS1897);
  #line 57 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1989);
  moonbit_decref(_M0L6_2atmpS1989);
  #line 58 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1898
  = _M0MPB13StringBuilder21StringBuilder_2einner(16);
  #line 58 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1898, (moonbit_string_t)moonbit_string_literal_149.data);
  #line 58 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1994
  = _M0MP19moonbitDB8Database6lindex(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_141.data, 0);
  #line 58 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1993
  = _M0FP39moonbitDB8examples12basic__usage9show__opt(_M0L6_2atmpS1994);
  if (_M0L6_2atmpS1994) {
    moonbit_decref(_M0L6_2atmpS1994);
  }
  #line 58 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1898, _M0L6_2atmpS1993);
  moonbit_decref(_M0L6_2atmpS1993);
  #line 58 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1992
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1898);
  moonbit_decref(_M0L18_2astring__builderS1898);
  #line 58 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1992);
  moonbit_decref(_M0L6_2atmpS1992);
  #line 60 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_150.data);
  #line 61 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1995
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_151.data, (moonbit_string_t)moonbit_string_literal_152.data);
  #line 62 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1996
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_151.data, (moonbit_string_t)moonbit_string_literal_153.data);
  #line 63 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1997
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_151.data, (moonbit_string_t)moonbit_string_literal_154.data);
  #line 64 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1899
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 64 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1899, (moonbit_string_t)moonbit_string_literal_155.data);
  #line 64 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1999
  = _M0MP19moonbitDB8Database5scard(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_151.data);
  #line 64 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1899, _M0L6_2atmpS1999);
  #line 64 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS1998
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1899);
  moonbit_decref(_M0L18_2astring__builderS1899);
  #line 64 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1998);
  moonbit_decref(_M0L6_2atmpS1998);
  #line 65 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1900
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 65 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1900, (moonbit_string_t)moonbit_string_literal_156.data);
  #line 65 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2001
  = _M0MP19moonbitDB8Database9sismember(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_151.data, (moonbit_string_t)moonbit_string_literal_154.data);
  #line 65 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGbE(_M0L18_2astring__builderS1900, _M0L6_2atmpS2001);
  #line 65 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2000
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1900);
  moonbit_decref(_M0L18_2astring__builderS1900);
  #line 65 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2000);
  moonbit_decref(_M0L6_2atmpS2000);
  #line 66 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L4tagsS1901
  = _M0MP19moonbitDB8Database8smembers(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_151.data);
  #line 67 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1902
  = _M0MPB13StringBuilder21StringBuilder_2einner(12);
  #line 67 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1902, (moonbit_string_t)moonbit_string_literal_157.data);
  _M0L7_2abindS1903 = (moonbit_string_t)moonbit_string_literal_138.data;
  _M0L6_2atmpS2005 = Moonbit_array_length(_M0L7_2abindS1903);
  _M0L6_2atmpS2004
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS1903, .$1 = 0, .$2 = _M0L6_2atmpS2005
  };
  #line 67 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2003
  = _M0MPC15array5Array4joinGsE(_M0L4tagsS1901, _M0L6_2atmpS2004);
  moonbit_decref(_M0L4tagsS1901);
  moonbit_decref(_M0L6_2atmpS2004.$0);
  #line 67 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1902, _M0L6_2atmpS2003);
  moonbit_decref(_M0L6_2atmpS2003);
  #line 67 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1902, (moonbit_string_t)moonbit_string_literal_139.data);
  #line 67 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2002
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1902);
  moonbit_decref(_M0L18_2astring__builderS1902);
  #line 67 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2002);
  moonbit_decref(_M0L6_2atmpS2002);
  #line 69 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_158.data);
  #line 70 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2006
  = _M0MP19moonbitDB8Database4zadd(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_159.data, 0x1.9p+6f, (moonbit_string_t)moonbit_string_literal_160.data);
  #line 71 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2007
  = _M0MP19moonbitDB8Database4zadd(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_159.data, 0x1.9p+7f, (moonbit_string_t)moonbit_string_literal_161.data);
  #line 72 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2008
  = _M0MP19moonbitDB8Database4zadd(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_159.data, 0x1.2cp+7f, (moonbit_string_t)moonbit_string_literal_162.data);
  #line 73 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1904
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 73 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1904, (moonbit_string_t)moonbit_string_literal_163.data);
  #line 73 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2010
  = _M0MP19moonbitDB8Database5zcard(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_159.data);
  #line 73 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1904, _M0L6_2atmpS2010);
  #line 73 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2009
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1904);
  moonbit_decref(_M0L18_2astring__builderS1904);
  #line 73 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2009);
  moonbit_decref(_M0L6_2atmpS2009);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1905
  = _M0MPB13StringBuilder21StringBuilder_2einner(16);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1905, (moonbit_string_t)moonbit_string_literal_164.data);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2013
  = _M0MP19moonbitDB8Database6zscore(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_159.data, (moonbit_string_t)moonbit_string_literal_161.data);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2012
  = _M0FP39moonbitDB8examples12basic__usage16show__opt__float(_M0L6_2atmpS2013);
  moonbit_decref(_M0L6_2atmpS2013);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1905, _M0L6_2atmpS2012);
  moonbit_decref(_M0L6_2atmpS2012);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2011
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1905);
  moonbit_decref(_M0L18_2astring__builderS1905);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2011);
  moonbit_decref(_M0L6_2atmpS2011);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1906
  = _M0MPB13StringBuilder21StringBuilder_2einner(15);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1906, (moonbit_string_t)moonbit_string_literal_165.data);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2016
  = _M0MP19moonbitDB8Database5zrank(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_159.data, (moonbit_string_t)moonbit_string_literal_162.data);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2015
  = _M0FP39moonbitDB8examples12basic__usage14show__opt__int(_M0L6_2atmpS2016);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1906, _M0L6_2atmpS2015);
  moonbit_decref(_M0L6_2atmpS2015);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2014
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1906);
  moonbit_decref(_M0L18_2astring__builderS1906);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2014);
  moonbit_decref(_M0L6_2atmpS2014);
  #line 76 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L3topS1907
  = _M0MP19moonbitDB8Database6zrange(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_159.data, 0, 2);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1908
  = _M0MPB13StringBuilder21StringBuilder_2einner(14);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1908, (moonbit_string_t)moonbit_string_literal_166.data);
  _M0L7_2abindS1909 = (moonbit_string_t)moonbit_string_literal_138.data;
  _M0L6_2atmpS2020 = Moonbit_array_length(_M0L7_2abindS1909);
  _M0L6_2atmpS2019
  = (struct _M0TPC16string10StringView){
    .$0 = _M0L7_2abindS1909, .$1 = 0, .$2 = _M0L6_2atmpS2020
  };
  #line 77 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2018
  = _M0MPC15array5Array4joinGsE(_M0L3topS1907, _M0L6_2atmpS2019);
  moonbit_decref(_M0L3topS1907);
  moonbit_decref(_M0L6_2atmpS2019.$0);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1908, _M0L6_2atmpS2018);
  moonbit_decref(_M0L6_2atmpS2018);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1908, (moonbit_string_t)moonbit_string_literal_139.data);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2017
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1908);
  moonbit_decref(_M0L18_2astring__builderS1908);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2017);
  moonbit_decref(_M0L6_2atmpS2017);
  #line 79 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_167.data);
  #line 80 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2021
  = _M0MP19moonbitDB8Database3set(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_168.data, (moonbit_string_t)moonbit_string_literal_169.data);
  #line 81 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2022
  = _M0MP19moonbitDB8Database6expire(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_168.data, 30);
  #line 82 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1910
  = _M0MPB13StringBuilder21StringBuilder_2einner(20);
  #line 82 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1910, (moonbit_string_t)moonbit_string_literal_170.data);
  #line 82 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2024
  = _M0MP19moonbitDB8Database3ttl(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_168.data);
  #line 82 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1910, _M0L6_2atmpS2024);
  #line 82 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1910, (moonbit_string_t)moonbit_string_literal_171.data);
  #line 82 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2023
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1910);
  moonbit_decref(_M0L18_2astring__builderS1910);
  #line 82 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2023);
  moonbit_decref(_M0L6_2atmpS2023);
  #line 83 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MP19moonbitDB8Database13advance__time(_M0L2dbS1870, 20000);
  #line 84 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1911
  = _M0MPB13StringBuilder21StringBuilder_2einner(15);
  #line 84 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1911, (moonbit_string_t)moonbit_string_literal_172.data);
  #line 84 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2026
  = _M0MP19moonbitDB8Database3ttl(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_168.data);
  #line 84 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1911, _M0L6_2atmpS2026);
  #line 84 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1911, (moonbit_string_t)moonbit_string_literal_171.data);
  #line 84 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2025
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1911);
  moonbit_decref(_M0L18_2astring__builderS1911);
  #line 84 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2025);
  moonbit_decref(_M0L6_2atmpS2025);
  #line 85 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MP19moonbitDB8Database13advance__time(_M0L2dbS1870, 15000);
  #line 86 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1912
  = _M0MPB13StringBuilder21StringBuilder_2einner(17);
  #line 86 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1912, (moonbit_string_t)moonbit_string_literal_173.data);
  #line 86 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2028
  = _M0MP19moonbitDB8Database6exists(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_168.data);
  #line 86 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGbE(_M0L18_2astring__builderS1912, _M0L6_2atmpS2028);
  #line 86 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2027
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1912);
  moonbit_decref(_M0L18_2astring__builderS1912);
  #line 86 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2027);
  moonbit_decref(_M0L6_2atmpS2027);
  #line 88 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_174.data);
  _M0L6_2atmpS2033 = (moonbit_string_t*)moonbit_make_ref_array_raw(3);
  _M0L6_2atmpS2033[0] = (moonbit_string_t)moonbit_string_literal_175.data;
  _M0L6_2atmpS2033[1] = (moonbit_string_t)moonbit_string_literal_176.data;
  _M0L6_2atmpS2033[2] = (moonbit_string_t)moonbit_string_literal_177.data;
  _M0L6_2atmpS2030
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2030)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6_2atmpS2030->$0 = _M0L6_2atmpS2033;
  _M0L6_2atmpS2030->$1 = 3;
  _M0L6_2atmpS2032 = (moonbit_string_t*)moonbit_make_ref_array_raw(3);
  _M0L6_2atmpS2032[0] = (moonbit_string_t)moonbit_string_literal_178.data;
  _M0L6_2atmpS2032[1] = (moonbit_string_t)moonbit_string_literal_179.data;
  _M0L6_2atmpS2032[2] = (moonbit_string_t)moonbit_string_literal_180.data;
  _M0L6_2atmpS2031
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2031)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6_2atmpS2031->$0 = _M0L6_2atmpS2032;
  _M0L6_2atmpS2031->$1 = 3;
  #line 89 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2029
  = _M0MP19moonbitDB8Database4mset(_M0L2dbS1870, _M0L6_2atmpS2030, _M0L6_2atmpS2031);
  moonbit_decref(_M0L6_2atmpS2030);
  moonbit_decref(_M0L6_2atmpS2031);
  _M0L6_2atmpS2057 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2057[0] = (moonbit_string_t)moonbit_string_literal_175.data;
  _M0L6_2atmpS2057[1] = (moonbit_string_t)moonbit_string_literal_176.data;
  _M0L6_2atmpS2057[2] = (moonbit_string_t)moonbit_string_literal_177.data;
  _M0L6_2atmpS2057[3] = (moonbit_string_t)moonbit_string_literal_181.data;
  _M0L6_2atmpS2056
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2056)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6_2atmpS2056->$0 = _M0L6_2atmpS2057;
  _M0L6_2atmpS2056->$1 = 4;
  #line 90 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L4valsS1913
  = _M0MP19moonbitDB8Database4mget(_M0L2dbS1870, _M0L6_2atmpS2056);
  moonbit_decref(_M0L6_2atmpS2056);
  #line 91 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_182.data);
  _M0L1iS1914 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2034;
    #line 92 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
    _M0L6_2atmpS2034 = _M0MPC15array5Array6lengthGOsE(_M0L4valsS1913);
    if (_M0L1iS1914 < _M0L6_2atmpS2034) {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1915;
      moonbit_string_t _M0L6_2atmpS2037;
      moonbit_string_t _M0L6_2atmpS2036;
      moonbit_string_t _M0L6_2atmpS2035;
      int32_t _M0L6_2atmpS2038;
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0L18_2astring__builderS1915
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1915, (moonbit_string_t)moonbit_string_literal_183.data);
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1915, _M0L1iS1914);
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1915, (moonbit_string_t)moonbit_string_literal_184.data);
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0L6_2atmpS2037
      = _M0MPC15array5Array2atGOsE(_M0L4valsS1913, _M0L1iS1914);
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0L6_2atmpS2036
      = _M0FP39moonbitDB8examples12basic__usage9show__opt(_M0L6_2atmpS2037);
      if (_M0L6_2atmpS2037) {
        moonbit_decref(_M0L6_2atmpS2037);
      }
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1915, _M0L6_2atmpS2036);
      moonbit_decref(_M0L6_2atmpS2036);
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0L6_2atmpS2035
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1915);
      moonbit_decref(_M0L18_2astring__builderS1915);
      #line 93 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2035);
      moonbit_decref(_M0L6_2atmpS2035);
      _M0L6_2atmpS2038 = _M0L1iS1914 + 1;
      _M0L1iS1914 = _M0L6_2atmpS2038;
      continue;
    } else {
      moonbit_decref(_M0L4valsS1913);
    }
    break;
  }
  _M0L6_2atmpS2055 = (moonbit_string_t*)moonbit_make_ref_array_raw(3);
  _M0L6_2atmpS2055[0] = (moonbit_string_t)moonbit_string_literal_175.data;
  _M0L6_2atmpS2055[1] = (moonbit_string_t)moonbit_string_literal_176.data;
  _M0L6_2atmpS2055[2] = (moonbit_string_t)moonbit_string_literal_177.data;
  _M0L6_2atmpS2054
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2054)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _M0L6_2atmpS2054->$0 = _M0L6_2atmpS2055;
  _M0L6_2atmpS2054->$1 = 3;
  #line 95 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L7deletedS1917
  = _M0MP19moonbitDB8Database4mdel(_M0L2dbS1870, _M0L6_2atmpS2054);
  moonbit_decref(_M0L6_2atmpS2054);
  #line 96 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1918
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 96 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1918, (moonbit_string_t)moonbit_string_literal_185.data);
  #line 96 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1918, _M0L7deletedS1917);
  #line 96 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2039
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1918);
  moonbit_decref(_M0L18_2astring__builderS1918);
  #line 96 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2039);
  moonbit_decref(_M0L6_2atmpS2039);
  #line 98 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_186.data);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1919
  = _M0MPB13StringBuilder21StringBuilder_2einner(8);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1919, (moonbit_string_t)moonbit_string_literal_187.data);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2041 = _M0MP19moonbitDB8Database6dbsize(_M0L2dbS1870);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1919, _M0L6_2atmpS2041);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2040
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1919);
  moonbit_decref(_M0L18_2astring__builderS1919);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2040);
  moonbit_decref(_M0L6_2atmpS2040);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1920
  = _M0MPB13StringBuilder21StringBuilder_2einner(6);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1920, (moonbit_string_t)moonbit_string_literal_188.data);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2043 = _M0MP19moonbitDB8Database4ping();
  #line 100 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1920, _M0L6_2atmpS2043);
  moonbit_decref(_M0L6_2atmpS2043);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2042
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1920);
  moonbit_decref(_M0L18_2astring__builderS1920);
  #line 100 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2042);
  moonbit_decref(_M0L6_2atmpS2042);
  #line 101 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1921
  = _M0MPB13StringBuilder21StringBuilder_2einner(6);
  #line 101 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1921, (moonbit_string_t)moonbit_string_literal_189.data);
  #line 101 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2045
  = _M0MP19moonbitDB8Database4echo(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_190.data);
  #line 101 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1921, _M0L6_2atmpS2045);
  moonbit_decref(_M0L6_2atmpS2045);
  #line 101 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2044
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1921);
  moonbit_decref(_M0L18_2astring__builderS1921);
  #line 101 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2044);
  moonbit_decref(_M0L6_2atmpS2044);
  #line 103 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_191.data);
  #line 104 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1922
  = _M0MPB13StringBuilder21StringBuilder_2einner(13);
  #line 104 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1922, (moonbit_string_t)moonbit_string_literal_192.data);
  #line 104 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2047
  = _M0MP19moonbitDB8Database8type__of(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_130.data);
  #line 104 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1922, _M0L6_2atmpS2047);
  moonbit_decref(_M0L6_2atmpS2047);
  #line 104 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2046
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1922);
  moonbit_decref(_M0L18_2astring__builderS1922);
  #line 104 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2046);
  moonbit_decref(_M0L6_2atmpS2046);
  #line 105 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1923
  = _M0MPB13StringBuilder21StringBuilder_2einner(12);
  #line 105 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1923, (moonbit_string_t)moonbit_string_literal_193.data);
  #line 105 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2049
  = _M0MP19moonbitDB8Database8type__of(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_141.data);
  #line 105 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1923, _M0L6_2atmpS2049);
  moonbit_decref(_M0L6_2atmpS2049);
  #line 105 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2048
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1923);
  moonbit_decref(_M0L18_2astring__builderS1923);
  #line 105 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2048);
  moonbit_decref(_M0L6_2atmpS2048);
  #line 106 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1924
  = _M0MPB13StringBuilder21StringBuilder_2einner(18);
  #line 106 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1924, (moonbit_string_t)moonbit_string_literal_194.data);
  #line 106 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2051
  = _M0MP19moonbitDB8Database8type__of(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_159.data);
  #line 106 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1924, _M0L6_2atmpS2051);
  moonbit_decref(_M0L6_2atmpS2051);
  #line 106 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2050
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1924);
  moonbit_decref(_M0L18_2astring__builderS1924);
  #line 106 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2050);
  moonbit_decref(_M0L6_2atmpS2050);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L18_2astring__builderS1925
  = _M0MPB13StringBuilder21StringBuilder_2einner(18);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1925, (moonbit_string_t)moonbit_string_literal_195.data);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2053
  = _M0MP19moonbitDB8Database8type__of(_M0L2dbS1870, (moonbit_string_t)moonbit_string_literal_196.data);
  moonbit_decref(_M0L2dbS1870);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1925, _M0L6_2atmpS2053);
  moonbit_decref(_M0L6_2atmpS2053);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0L6_2atmpS2052
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1925);
  moonbit_decref(_M0L18_2astring__builderS1925);
  #line 107 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2052);
  moonbit_decref(_M0L6_2atmpS2052);
  #line 109 "/home/developer/Documents2/moonbitDB/examples/basic_usage/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_197.data);
  return 0;
}