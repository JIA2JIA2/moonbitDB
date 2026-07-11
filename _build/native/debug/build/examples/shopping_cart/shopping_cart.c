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
struct _M0TPB9ArrayViewGUsfEE;

struct _M0TP19moonbitDB8Database;

struct _M0TWEOUssE;

struct _M0TWEOUsRP19moonbitDB10RedisValueE;

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3151__l711__;

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

struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3161__l711__;

struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE;

struct _M0DTP19moonbitDB10RedisValue4Hash;

struct _M0TPB6Logger;

struct _M0TP19moonbitDB5Deque;

struct _M0TUsfE;

struct _M0TPB8MutLocalGORPB5EntryGsfEE;

struct _M0TPB5EntryGsbE;

struct _M0TPB8MutLocalGORPB5EntryGssEE;

struct _M0DTPC16option6OptionGfE4Some;

struct _M0TPB19MulShiftAll64Result;

struct _M0TPB5ArrayGRP39moonbitDB8examples14shopping__cart7ProductE;

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

struct _M0TPB8MutLocalGfE;

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3141__l711__;

struct _M0TP39moonbitDB8examples14shopping__cart7Product;

struct _M0TPB4IterGUsfEE;

struct _M0TWEOUsfE;

struct _M0BTPB4Show;

struct _M0TPC16string10StringView;

struct _M0TPB8MutLocalGbE;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB8MutLocalGORPB5EntryGsbEE;

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3171__l711__;

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

struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3151__l711__ {
  struct _M0TUsbE*(* code)(struct _M0TWEOUsbE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* $1;
  
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

struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3161__l711__ {
  struct _M0TUsRP19moonbitDB10RedisValueE*(* code)(
    struct _M0TWEOUsRP19moonbitDB10RedisValueE*
  );
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE* $1;
  
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

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0TPB5ArrayGRP39moonbitDB8examples14shopping__cart7ProductE {
  struct _M0TP39moonbitDB8examples14shopping__cart7Product** $0;
  int32_t $1;
  
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

struct _M0TPB8MutLocalGfE {
  float $0;
  
};

struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3141__l711__ {
  struct _M0TUssE*(* code)(struct _M0TWEOUssE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* $1;
  
};

struct _M0TP39moonbitDB8examples14shopping__cart7Product {
  moonbit_string_t $0;
  moonbit_string_t $1;
  float $2;
  int32_t $3;
  
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

struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3171__l711__ {
  struct _M0TUsfE*(* code)(struct _M0TWEOUsfE*);
  struct _M0TPB8MutLocalGiE* $0;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* $1;
  
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

moonbit_string_t _M0FP39moonbitDB8examples14shopping__cart16show__opt__float(
  void*
);

struct _M0TP39moonbitDB8examples14shopping__cart7Product* _M0MP39moonbitDB8examples14shopping__cart7Product3new(
  moonbit_string_t,
  moonbit_string_t,
  float,
  int32_t
);

float _M0FP39moonbitDB8examples14shopping__cart12parse__float(
  moonbit_string_t
);

int32_t _M0FP39moonbitDB8examples14shopping__cart10parse__int(
  moonbit_string_t
);

moonbit_string_t _M0FP39moonbitDB8examples14shopping__cart9show__opt(
  moonbit_string_t
);

moonbit_string_t _M0FP19moonbitDB16show__opt__float(void*);

moonbit_string_t _M0FP19moonbitDB14show__opt__int(int64_t);

moonbit_string_t _M0FP19moonbitDB9show__opt(moonbit_string_t);

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database7command();

moonbit_string_t _M0MP19moonbitDB8Database4ping();

moonbit_string_t _M0MP19moonbitDB8Database9randomkey(
  struct _M0TP19moonbitDB8Database*
);

int32_t _M0MP19moonbitDB8Database7flushdb(struct _M0TP19moonbitDB8Database*);

int32_t _M0MP19moonbitDB8Database6dbsize(struct _M0TP19moonbitDB8Database*);

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

struct _M0TUsfE* _M0MPB3Map4iterGsfEC3171l711(struct _M0TWEOUsfE*);

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB3Map4iterGsRP19moonbitDB10RedisValueEC3161l711(
  struct _M0TWEOUsRP19moonbitDB10RedisValueE*
);

struct _M0TUsbE* _M0MPB3Map4iterGsbEC3151l711(struct _M0TWEOUsbE*);

struct _M0TUssE* _M0MPB3Map4iterGssEC3141l711(struct _M0TWEOUssE*);

int32_t _M0MPB3Map6lengthGssE(struct _M0TPB3MapGssE*);

int32_t _M0MPB3Map6lengthGsbE(struct _M0TPB3MapGsbE*);

int32_t _M0MPB3Map6lengthGsfE(struct _M0TPB3MapGsfE*);

int32_t _M0MPB3Map6removeGsiE(struct _M0TPB3MapGsiE*, moonbit_string_t);

int32_t _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  moonbit_string_t
);

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

int32_t _M0MPB3Map11shift__backGsbE(struct _M0TPB3MapGsbE*, int32_t);

int32_t _M0MPB3Map13remove__entryGsiE(
  struct _M0TPB3MapGsiE*,
  struct _M0TPB5EntryGsiE*
);

int32_t _M0MPB3Map13remove__entryGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*,
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*
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

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_96 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 0, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_92 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 55, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_175 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 32, 32, 
    20851, 32852, 29992, 25143, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_142 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 17, 104, 105, 
    115, 116, 111, 114, 121, 58, 117, 115, 101, 114, 58, 49, 48, 48, 
    49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_66 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 90, 82, 
    69, 86, 82, 65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_12 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 73, 78, 
    67, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_13 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 68, 69, 
    67, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_63 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 90, 82, 
    65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_74 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 68, 66, 
    83, 73, 90, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_176 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 32, 32, 
    21097, 20313, 26102, 38388, 58, 32, 0
  };

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
} const moonbit_string_literal_46 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 83, 77, 
    69, 77, 66, 69, 82, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_24 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 68, 
    69, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_178 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 32, 
    49, 48, 48, 48, 31186, 21518, 46, 46, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_172 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 115, 101, 
    115, 115, 58, 97, 98, 99, 49, 50, 51, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_85 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 79, 
    78, 71, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_171 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 25, 10, 9201,
    65039, 32, 38454, 27573, 56, 65306, 20250, 35805, 31649, 29702, 65288,
    83, 116, 114, 105, 110, 103, 32, 43, 32, 36807, 26399, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_86 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 49, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_0 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_152 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 10, 55357,
    56599, 32, 38454, 27573, 53, 65306, 20849, 21516, 25910, 34255, 65288,
    83, 101, 116, 20132, 38598, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_55 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 83, 68, 
    73, 70, 70, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_20 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 71, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_42 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 76, 80, 
    85, 83, 72, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_38 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 76, 73, 
    78, 68, 69, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_58 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 90, 65, 
    68, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_147 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    21382, 21490, 35760, 24405, 24635, 25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_65 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 90, 82, 
    69, 86, 82, 65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_146 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_89 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 52, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_161 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 32, 32, 
    39, 30828, 20214, 39, 26631, 31614, 21830, 21697, 25968, 58, 32, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_116 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 39640, 24615,
    33021, 26381, 21153, 22120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_110 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 46, 108, 101, 110, 103, 116, 104, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_122 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 26080, 32447,
    40736, 26631, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_17 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 84, 
    84, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_195 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 32, 61, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_184 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 104, 111, 
    116, 58, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_112 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 21, 32, 32, 
    30005, 21830, 36141, 29289, 36710, 32, 38, 32, 32531, 23384, 31995,
    32479, 32, 45, 32, 37096, 32626, 31034, 20363, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_107 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 44, 32, 
    115, 114, 99, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_5 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 71, 69, 84, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_164 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 25, 10, 55357,
    56522, 32, 38454, 27573, 55, 65306, 38144, 37327, 25490, 34892, 27036,
    65288, 83, 111, 114, 116, 101, 100, 32, 83, 101, 116, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_72 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_169 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 32, 45, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_56 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 77, 
    79, 86, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_192 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 32, 32, 
    25209, 37327, 39044, 28909, 32, 53, 32, 20010, 28909, 28857, 107, 
    101, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_177 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 31186, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_165 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 115, 97, 
    108, 101, 115, 58, 114, 97, 110, 107, 105, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_49 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 83, 73, 
    83, 77, 69, 77, 66, 69, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_194 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 32, 61, 
    32, 40, 110, 105, 108, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_197 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 32, 
    24635, 32, 75, 101, 121, 32, 25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_188 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 118, 97, 
    108, 117, 101, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_95 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_185 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 104, 111, 
    116, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_141 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 10, 55357,
    56541, 32, 38454, 27573, 51, 65306, 27983, 35272, 21382, 21490, 65288,
    76, 105, 115, 116, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_90 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 53, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_70 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 90, 82, 
    69, 77, 82, 65, 78, 71, 69, 66, 89, 82, 65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_29 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 86, 
    65, 76, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_119 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 112, 49, 
    48, 48, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_23 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 71, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_11 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 83, 84, 
    82, 76, 69, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_1 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 85, 110, 
    107, 110, 111, 119, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_39 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_9 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 84, 89, 
    80, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_113 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 112, 49, 
    48, 48, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_77 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 82, 65, 
    78, 68, 79, 77, 75, 69, 89, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_15 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 80, 69, 
    88, 80, 73, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_10 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 65, 80, 
    80, 69, 78, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_48 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 67, 
    65, 82, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_47 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 82, 
    69, 77, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_193 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 32, 32, 
    25209, 37327, 35835, 21462, 32467, 26524, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_34 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_199 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 32, 32, 
    9989, 32, 36141, 29289, 36710, 32, 38, 32, 32531, 23384, 31995, 32479,
    31034, 20363, 23436, 25104, 65281, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_111 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 40, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_68 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 90, 82, 
    69, 86, 82, 65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_78 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 82, 69, 
    78, 65, 77, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_61 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 83, 
    67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_174 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 32, 32, 
    20250, 35805, 73, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_130 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 32, 40, 
    73, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_57 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_159 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 116, 97, 
    103, 58, 22806, 35774, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_140 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 32, 32, 
    21830, 21697, 31181, 31867, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_27 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 72, 69, 
    88, 73, 83, 84, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_2 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 40, 110, 
    105, 108, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_88 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 51, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_82 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 73, 78, 
    70, 79, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_8 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 75, 69, 
    89, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_7 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 69, 88, 
    73, 83, 84, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_144 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 46, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_150 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 32, 32, 
    25910, 34255, 21830, 21697, 25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_149 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 102, 97, 
    118, 111, 114, 105, 116, 101, 115, 58, 117, 115, 101, 114, 58, 49, 
    48, 48, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_67 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 90, 82, 
    65, 78, 75, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_84 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 67, 79, 
    77, 77, 65, 78, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_134 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 99, 97, 
    114, 116, 58, 117, 115, 101, 114, 58, 49, 48, 48, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_123 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 25, 10, 55357,
    56550, 32, 38454, 27573, 49, 65306, 21830, 21697, 20449, 24687, 32531,
    23384, 65288, 72, 97, 115, 104, 32, 43, 32, 36807, 26399, 65289, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_136 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 32, 
    32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_118 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 26426, 26800,
    38190, 30424, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_60 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 90, 67, 
    65, 82, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_132 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 32, 32, 
    32531, 23384, 21830, 21697, 25968, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_125 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 112, 114, 
    111, 100, 117, 99, 116, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_16 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 84, 84, 76, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_135 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 20, 32, 32, 
    29992, 25143, 32, 117, 115, 101, 114, 58, 49, 48, 48, 49, 32, 30340,
    36141, 29289, 36710, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_151 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 32, 32, 
    32, 32, 45, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_36 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_32 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 76, 80, 
    85, 83, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_26 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 76, 
    69, 78, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_190 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 118, 97, 
    108, 117, 101, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_167 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 32, 32, 
    32, 32, 31532, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_156 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 18, 10, 55356,
    57335, 65039, 32, 38454, 27573, 54, 65306, 21830, 21697, 26631, 31614,
    65288, 83, 101, 116, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[42]; 
} const moonbit_string_literal_198 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 41, 10, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 
    61, 61, 61, 61, 61, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_191 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 118, 97, 
    108, 117, 101, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_62 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 90, 82, 
    69, 77, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_45 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 83, 65, 
    68, 68, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_162 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 32, 32, 
    26082, 26159, 39, 30828, 20214, 39, 21448, 26159, 39, 22806, 35774, 
    39, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_127 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 112, 114, 
    105, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_73 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 90, 80, 
    79, 80, 77, 65, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_31 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 77, 
    71, 69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_59 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 82, 
    65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_114 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 77, 111, 
    111, 110, 66, 105, 116, 32534, 31243, 25351, 21335, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_91 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 54, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_83 =
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
} const moonbit_string_literal_54 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 83, 68, 
    73, 70, 70, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_44 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 82, 80, 
    79, 80, 76, 80, 85, 83, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_145 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 40, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_115 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 112, 49, 
    48, 48, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_128 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 115, 116, 
    111, 99, 107, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_69 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 90, 73, 
    78, 67, 82, 66, 89, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_166 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 32, 
    38144, 37327, 32, 84, 79, 80, 32, 51, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_143 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 32, 32, 
    26368, 36817, 27983, 35272, 30340, 53, 20010, 21830, 21697, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_129 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 32, 32, 
    32531, 23384, 21830, 21697, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_33 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 82, 80, 
    85, 83, 72, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_71 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 90, 82, 
    69, 77, 82, 65, 78, 71, 69, 66, 89, 83, 67, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_160 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 116, 97, 
    103, 58, 26174, 31034, 22120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_76 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 70, 76, 
    85, 83, 72, 65, 76, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_133 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 10, 55357,
    57042, 32, 38454, 27573, 50, 65306, 29992, 25143, 36141, 29289, 36710,
    65288, 72, 97, 115, 104, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_51 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 83, 73, 
    78, 84, 69, 82, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_50 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 83, 73, 
    78, 84, 69, 82, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_180 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 32, 32, 
    50, 48, 48, 48, 31186, 21518, 46, 46, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_53 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 11, 83, 85, 
    78, 73, 79, 78, 83, 84, 79, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_196 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 15, 10, 55357,
    56520, 32, 38454, 27573, 49, 48, 65306, 31995, 32479, 29366, 24577,
    24635, 35272, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_179 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    20250, 35805, 26159, 21542, 26377, 25928, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_30 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 77, 
    83, 69, 84, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_153 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 102, 97, 
    118, 111, 114, 105, 116, 101, 115, 58, 117, 115, 101, 114, 58, 49, 
    48, 48, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_187 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 118, 97, 
    108, 117, 101, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_170 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 32, 32, 
    38144, 37327, 22312, 49, 48, 48, 45, 51, 48, 48, 20043, 38388, 30340,
    21830, 21697, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_168 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 21517, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_163 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 14, 32, 32, 
    25152, 26377, 26631, 31614, 21830, 21697, 40, 21435, 37325, 41, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_52 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 83, 85, 
    78, 73, 79, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_40 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 76, 82, 
    69, 77, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_25 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 72, 71, 
    69, 84, 65, 76, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_21 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 68, 
    69, 76, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_158 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 116, 97, 
    103, 58, 30828, 20214, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_117 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 112, 49, 
    48, 48, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_80 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 80, 73, 
    78, 71, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_4 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 83, 69, 84, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_94 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 57, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_22 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 72, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_148 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 16, 10, 11088, 
    32, 38454, 27573, 52, 65306, 21830, 21697, 25910, 34255, 65288, 83, 
    101, 116, 65289, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_28 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 72, 75, 
    69, 89, 83, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_139 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 10, 32, 32, 
    36141, 29289, 36710, 24635, 20215, 58, 32, 165, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_35 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 82, 80, 
    79, 80, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_181 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 19, 10, 55357,
    56580, 32, 38454, 27573, 57, 65306, 32531, 23384, 39044, 28909, 32, 
    38, 32, 25209, 37327, 25805, 20316, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_157 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 116, 97, 
    103, 58, 32534, 31243, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_124 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 40, 45, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 
    45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 
    45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_138 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 32, 61, 
    32, 165, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_131 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 13, 44, 32, 
    84, 84, 76, 58, 32, 51, 54, 48, 48, 115, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_41 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 76, 73, 
    78, 83, 69, 82, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_14 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 69, 88, 
    80, 73, 82, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_126 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 110, 97, 
    109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_173 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 9, 117, 115, 
    101, 114, 58, 49, 48, 48, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_101 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 116, 114, 
    117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_19 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 77, 83, 
    69, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_186 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 104, 111, 
    116, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_3 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_93 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 1, 56, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_137 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 32, 120, 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_43 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 82, 80, 
    85, 83, 72, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_64 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 90, 67, 
    79, 85, 78, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_37 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 76, 82, 
    65, 78, 71, 69, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_79 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 8, 82, 69, 
    78, 65, 77, 69, 78, 88, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_6 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 68, 69, 76, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_81 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 4, 69, 67, 
    72, 79, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_120 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 52, 75, 
    26174, 31034, 22120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_189 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 6, 118, 97, 
    108, 117, 101, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_183 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 104, 111, 
    116, 58, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_100 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 3, 48, 46, 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_75 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 70, 76, 
    85, 83, 72, 68, 66, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_182 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 104, 111, 
    116, 58, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_18 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 7, 80, 69, 
    82, 83, 73, 83, 84, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_155 =
  { Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 2, 32, 20214, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_154 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 12, 32, 32, 
    20004, 20301, 29992, 25143, 20849, 21516, 25910, 34255, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_121 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_VAL_ARRAY), 5, 112, 49, 
    48, 48, 53, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  {
    Moonbit_make_static_rc(moonbit_BLOCK_KIND_REGULAR),
    Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0)
  };

uint32_t const moonbit_layout_table_data[138] =
  {
    sizeof(struct _M0TP39moonbitDB8examples14shopping__cart7Product) / 4, 
    2,
    offsetof(struct _M0TP39moonbitDB8examples14shopping__cart7Product, $0)
    / 4,
    offsetof(struct _M0TP39moonbitDB8examples14shopping__cart7Product, $1)
    / 4, sizeof(struct _M0TPB5ArrayGsE) / 4, 1,
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
    sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3141__l711__)
    / 4, 2,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3141__l711__, $0)
    / 4,
    offsetof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3141__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsbEE, $0) / 4,
    sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3151__l711__)
    / 4, 2,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3151__l711__, $0)
    / 4,
    offsetof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3151__l711__, $1)
    / 4,
    sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE)
    / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE, $0)
    / 4,
    sizeof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3161__l711__)
    / 4, 2,
    offsetof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3161__l711__, $0)
    / 4,
    offsetof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3161__l711__, $1)
    / 4, sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE) / 4, 1,
    offsetof(struct _M0TPB8MutLocalGORPB5EntryGsfEE, $0) / 4,
    sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3171__l711__)
    / 4, 2,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3171__l711__, $0)
    / 4,
    offsetof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3171__l711__, $1)
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
    sizeof(struct _M0TPB5ArrayGRP39moonbitDB8examples14shopping__cart7ProductE)
    / 4, 1,
    offsetof(struct _M0TPB5ArrayGRP39moonbitDB8examples14shopping__cart7ProductE, $0)
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

moonbit_string_t _M0FP39moonbitDB8examples14shopping__cart16show__opt__float(
  void* _M0L3optS1952
) {
  float _M0L1vS1950;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1951;
  moonbit_string_t _result_4373;
  #line 238 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  switch (Moonbit_object_tag(_M0L3optS1952)) {
    case 1: {
      struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS1953 =
        (struct _M0DTPC16option6OptionGfE4Some*)_M0L3optS1952;
      float _M0L4_2avS1954 = _M0L7_2aSomeS1953->$0;
      _M0L1vS1950 = _M0L4_2avS1954;
      goto join_1949;
      break;
    }
    default: {
      return (moonbit_string_t)moonbit_string_literal_0.data;
      break;
    }
  }
  join_1949:;
  #line 240 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1951
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 240 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1951, _M0L1vS1950);
  #line 240 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _result_4373
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1951);
  moonbit_decref(_M0L18_2astring__builderS1951);
  return _result_4373;
}

struct _M0TP39moonbitDB8examples14shopping__cart7Product* _M0MP39moonbitDB8examples14shopping__cart7Product3new(
  moonbit_string_t _M0L2idS1945,
  moonbit_string_t _M0L4nameS1946,
  float _M0L5priceS1947,
  int32_t _M0L5stockS1948
) {
  struct _M0TP39moonbitDB8examples14shopping__cart7Product* _block_4374;
  #line 61 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  moonbit_incref(_M0L2idS1945);
  moonbit_incref(_M0L4nameS1946);
  _block_4374
  = (struct _M0TP39moonbitDB8examples14shopping__cart7Product*)moonbit_malloc(sizeof(struct _M0TP39moonbitDB8examples14shopping__cart7Product));
  Moonbit_object_header(_block_4374)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 0, 0);
  _block_4374->$0 = _M0L2idS1945;
  _block_4374->$1 = _M0L4nameS1946;
  _block_4374->$2 = _M0L5priceS1947;
  _block_4374->$3 = _M0L5stockS1948;
  return _block_4374;
}

float _M0FP39moonbitDB8examples14shopping__cart12parse__float(
  moonbit_string_t _M0L1sS1939
) {
  struct _M0TPB8MutLocalGfE* _M0L6resultS1936;
  struct _M0TPB8MutLocalGiE* _M0L1iS1937;
  struct _M0TPB8MutLocalGbE* _M0L3negS1938;
  int32_t _M0L6_2atmpS3855;
  int32_t _if__result_4375;
  struct _M0TPB8MutLocalGbE* _M0L8has__dotS1940;
  struct _M0TPB8MutLocalGfE* _M0L12decimal__posS1941;
  int32_t _result_4379;
  #line 26 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6resultS1936
  = (struct _M0TPB8MutLocalGfE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGfE));
  Moonbit_object_header(_M0L6resultS1936)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6resultS1936->$0 = 0x0p+0f;
  _M0L1iS1937
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1937)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS1937->$0 = 0;
  _M0L3negS1938
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L3negS1938)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L3negS1938->$0 = 0;
  _M0L6_2atmpS3855 = Moonbit_array_length(_M0L1sS1939);
  if (_M0L6_2atmpS3855 > 0) {
    int32_t _M0L6_2atmpS3854;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1939)) {
      #line 30 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3854 = _M0L1sS1939[0];
    _if__result_4375 = _M0L6_2atmpS3854 == 45;
  } else {
    _if__result_4375 = 0;
  }
  if (_if__result_4375) {
    _M0L3negS1938->$0 = 1;
    _M0L1iS1937->$0 = 1;
  }
  _M0L8has__dotS1940
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L8has__dotS1940)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L8has__dotS1940->$0 = 0;
  _M0L12decimal__posS1941
  = (struct _M0TPB8MutLocalGfE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGfE));
  Moonbit_object_header(_M0L12decimal__posS1941)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L12decimal__posS1941->$0 = 0x1.4p+3f;
  while (1) {
    int32_t _M0L3valS3856 = _M0L1iS1937->$0;
    int32_t _M0L6_2atmpS3857 = Moonbit_array_length(_M0L1sS1939);
    if (_M0L3valS3856 < _M0L6_2atmpS3857) {
      int32_t _M0L3valS3873 = _M0L1iS1937->$0;
      int32_t _M0L1cS1942;
      int32_t _if__result_4377;
      int32_t _M0L3valS3872;
      int32_t _M0L6_2atmpS3871;
      if (
        _M0L3valS3873 < 0
        || _M0L3valS3873 >= Moonbit_array_length(_M0L1sS1939)
      ) {
        #line 37 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        moonbit_panic();
      }
      _M0L1cS1942 = _M0L1sS1939[_M0L3valS3873];
      if (_M0L1cS1942 >= 48) {
        _if__result_4377 = _M0L1cS1942 <= 57;
      } else {
        _if__result_4377 = 0;
      }
      if (_if__result_4377) {
        int32_t _M0L6_2atmpS3868 = (int32_t)_M0L1cS1942;
        int32_t _M0L6_2atmpS3869 = 48;
        int32_t _M0L6_2atmpS3867 = _M0L6_2atmpS3868 - _M0L6_2atmpS3869;
        float _M0L5digitS1943 = (float)_M0L6_2atmpS3867;
        if (_M0L8has__dotS1940->$0) {
          float _M0L3valS3859 = _M0L6resultS1936->$0;
          float _M0L3valS3861 = _M0L12decimal__posS1941->$0;
          float _M0L6_2atmpS3860 = _M0L5digitS1943 / _M0L3valS3861;
          float _M0L6_2atmpS3858 = _M0L3valS3859 + _M0L6_2atmpS3860;
          float _M0L3valS3863;
          float _M0L6_2atmpS3862;
          _M0L6resultS1936->$0 = _M0L6_2atmpS3858;
          _M0L3valS3863 = _M0L12decimal__posS1941->$0;
          _M0L6_2atmpS3862 = _M0L3valS3863 * 0x1.4p+3f;
          _M0L12decimal__posS1941->$0 = _M0L6_2atmpS3862;
        } else {
          float _M0L3valS3866 = _M0L6resultS1936->$0;
          float _M0L6_2atmpS3865 = _M0L3valS3866 * 0x1.4p+3f;
          float _M0L6_2atmpS3864 = _M0L6_2atmpS3865 + _M0L5digitS1943;
          _M0L6resultS1936->$0 = _M0L6_2atmpS3864;
        }
      } else {
        int32_t _if__result_4378;
        if (_M0L1cS1942 == 46) {
          int32_t _M0L3valS3870 = _M0L8has__dotS1940->$0;
          _if__result_4378 = !_M0L3valS3870;
        } else {
          _if__result_4378 = 0;
        }
        if (_if__result_4378) {
          _M0L8has__dotS1940->$0 = 1;
        }
      }
      _M0L3valS3872 = _M0L1iS1937->$0;
      _M0L6_2atmpS3871 = _M0L3valS3872 + 1;
      _M0L1iS1937->$0 = _M0L6_2atmpS3871;
      continue;
    } else {
      moonbit_decref(_M0L12decimal__posS1941);
      moonbit_decref(_M0L8has__dotS1940);
      moonbit_decref(_M0L1iS1937);
    }
    break;
  }
  _result_4379 = _M0L3negS1938->$0;
  moonbit_decref(_M0L3negS1938);
  if (_result_4379) {
    float _M0L3valS3874 = _M0L6resultS1936->$0;
    moonbit_decref(_M0L6resultS1936);
    return -_M0L3valS3874;
  } else {
    float _result_4380 = _M0L6resultS1936->$0;
    moonbit_decref(_M0L6resultS1936);
    return _result_4380;
  }
}

int32_t _M0FP39moonbitDB8examples14shopping__cart10parse__int(
  moonbit_string_t _M0L1sS1933
) {
  struct _M0TPB8MutLocalGiE* _M0L6resultS1930;
  struct _M0TPB8MutLocalGiE* _M0L1iS1931;
  struct _M0TPB8MutLocalGbE* _M0L3negS1932;
  int32_t _M0L6_2atmpS3841;
  int32_t _if__result_4381;
  int32_t _result_4384;
  #line 8 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6resultS1930
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L6resultS1930)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6resultS1930->$0 = 0;
  _M0L1iS1931
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1931)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS1931->$0 = 0;
  _M0L3negS1932
  = (struct _M0TPB8MutLocalGbE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGbE));
  Moonbit_object_header(_M0L3negS1932)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L3negS1932->$0 = 0;
  _M0L6_2atmpS3841 = Moonbit_array_length(_M0L1sS1933);
  if (_M0L6_2atmpS3841 > 0) {
    int32_t _M0L6_2atmpS3840;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1933)) {
      #line 12 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3840 = _M0L1sS1933[0];
    _if__result_4381 = _M0L6_2atmpS3840 == 45;
  } else {
    _if__result_4381 = 0;
  }
  if (_if__result_4381) {
    _M0L3negS1932->$0 = 1;
    _M0L1iS1931->$0 = 1;
  }
  while (1) {
    int32_t _M0L3valS3842 = _M0L1iS1931->$0;
    int32_t _M0L6_2atmpS3843 = Moonbit_array_length(_M0L1sS1933);
    if (_M0L3valS3842 < _M0L6_2atmpS3843) {
      int32_t _M0L3valS3852 = _M0L1iS1931->$0;
      int32_t _M0L1cS1934;
      int32_t _if__result_4383;
      int32_t _M0L3valS3851;
      int32_t _M0L6_2atmpS3850;
      if (
        _M0L3valS3852 < 0
        || _M0L3valS3852 >= Moonbit_array_length(_M0L1sS1933)
      ) {
        #line 17 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        moonbit_panic();
      }
      _M0L1cS1934 = _M0L1sS1933[_M0L3valS3852];
      if (_M0L1cS1934 >= 48) {
        _if__result_4383 = _M0L1cS1934 <= 57;
      } else {
        _if__result_4383 = 0;
      }
      if (_if__result_4383) {
        int32_t _M0L3valS3849 = _M0L6resultS1930->$0;
        int32_t _M0L6_2atmpS3845 = _M0L3valS3849 * 10;
        int32_t _M0L6_2atmpS3847 = (int32_t)_M0L1cS1934;
        int32_t _M0L6_2atmpS3848 = 48;
        int32_t _M0L6_2atmpS3846 = _M0L6_2atmpS3847 - _M0L6_2atmpS3848;
        int32_t _M0L6_2atmpS3844 = _M0L6_2atmpS3845 + _M0L6_2atmpS3846;
        _M0L6resultS1930->$0 = _M0L6_2atmpS3844;
      }
      _M0L3valS3851 = _M0L1iS1931->$0;
      _M0L6_2atmpS3850 = _M0L3valS3851 + 1;
      _M0L1iS1931->$0 = _M0L6_2atmpS3850;
      continue;
    } else {
      moonbit_decref(_M0L1iS1931);
    }
    break;
  }
  _result_4384 = _M0L3negS1932->$0;
  moonbit_decref(_M0L3negS1932);
  if (_result_4384) {
    int32_t _M0L3valS3853 = _M0L6resultS1930->$0;
    moonbit_decref(_M0L6resultS1930);
    return -_M0L3valS3853;
  } else {
    int32_t _result_4385 = _M0L6resultS1930->$0;
    moonbit_decref(_M0L6resultS1930);
    return _result_4385;
  }
}

moonbit_string_t _M0FP39moonbitDB8examples14shopping__cart9show__opt(
  moonbit_string_t _M0L3optS1927
) {
  moonbit_string_t _M0L1vS1926;
  #line 1 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  if (_M0L3optS1927 == 0) {
    return (moonbit_string_t)moonbit_string_literal_1.data;
  } else {
    moonbit_string_t _M0L7_2aSomeS1928 = _M0L3optS1927;
    moonbit_string_t _M0L4_2avS1929 = _M0L7_2aSomeS1928;
    moonbit_incref(_M0L4_2avS1929);
    _M0L1vS1926 = _M0L4_2avS1929;
    goto join_1925;
  }
  join_1925:;
  return _M0L1vS1926;
}

moonbit_string_t _M0FP19moonbitDB16show__opt__float(void* _M0L3optS1861) {
  float _M0L1vS1859;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1860;
  moonbit_string_t _result_4388;
  #line 15 "/home/developer/Documents2/moonbitDB/demo.mbt"
  switch (Moonbit_object_tag(_M0L3optS1861)) {
    case 1: {
      struct _M0DTPC16option6OptionGfE4Some* _M0L7_2aSomeS1862 =
        (struct _M0DTPC16option6OptionGfE4Some*)_M0L3optS1861;
      float _M0L4_2avS1863 = _M0L7_2aSomeS1862->$0;
      _M0L1vS1859 = _M0L4_2avS1863;
      goto join_1858;
      break;
    }
    default: {
      return (moonbit_string_t)moonbit_string_literal_2.data;
      break;
    }
  }
  join_1858:;
  #line 17 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1860
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 17 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1860, _M0L1vS1859);
  #line 17 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _result_4388
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1860);
  moonbit_decref(_M0L18_2astring__builderS1860);
  return _result_4388;
}

moonbit_string_t _M0FP19moonbitDB14show__opt__int(int64_t _M0L3optS1855) {
  int32_t _M0L1vS1853;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1854;
  moonbit_string_t _result_4390;
  #line 8 "/home/developer/Documents2/moonbitDB/demo.mbt"
  if (_M0L3optS1855 == 4294967296ll) {
    return (moonbit_string_t)moonbit_string_literal_2.data;
  } else {
    int64_t _M0L7_2aSomeS1856 = _M0L3optS1855;
    int32_t _M0L4_2avS1857 = (int32_t)_M0L7_2aSomeS1856;
    _M0L1vS1853 = _M0L4_2avS1857;
    goto join_1852;
  }
  join_1852:;
  #line 10 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1854
  = _M0MPB13StringBuilder21StringBuilder_2einner(0);
  #line 10 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1854, _M0L1vS1853);
  #line 10 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _result_4390
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1854);
  moonbit_decref(_M0L18_2astring__builderS1854);
  return _result_4390;
}

moonbit_string_t _M0FP19moonbitDB9show__opt(moonbit_string_t _M0L3optS1849) {
  moonbit_string_t _M0L1vS1847;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1848;
  moonbit_string_t _result_4392;
  #line 1 "/home/developer/Documents2/moonbitDB/demo.mbt"
  if (_M0L3optS1849 == 0) {
    return (moonbit_string_t)moonbit_string_literal_2.data;
  } else {
    moonbit_string_t _M0L7_2aSomeS1850 = _M0L3optS1849;
    moonbit_string_t _M0L4_2avS1851 = _M0L7_2aSomeS1850;
    moonbit_incref(_M0L4_2avS1851);
    _M0L1vS1847 = _M0L4_2avS1851;
    goto join_1846;
  }
  join_1846:;
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0L18_2astring__builderS1848
  = _M0MPB13StringBuilder21StringBuilder_2einner(2);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1848, (moonbit_string_t)moonbit_string_literal_3.data);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1848, _M0L1vS1847);
  moonbit_decref(_M0L1vS1847);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1848, (moonbit_string_t)moonbit_string_literal_3.data);
  #line 3 "/home/developer/Documents2/moonbitDB/demo.mbt"
  _result_4392
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1848);
  moonbit_decref(_M0L18_2astring__builderS1848);
  return _result_4392;
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database7command() {
  moonbit_string_t* _M0L6_2atmpS3839;
  struct _M0TPB5ArrayGsE* _block_4393;
  #line 1594 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3839 = (moonbit_string_t*)moonbit_make_ref_array_raw(81);
  _M0L6_2atmpS3839[0] = (moonbit_string_t)moonbit_string_literal_4.data;
  _M0L6_2atmpS3839[1] = (moonbit_string_t)moonbit_string_literal_5.data;
  _M0L6_2atmpS3839[2] = (moonbit_string_t)moonbit_string_literal_6.data;
  _M0L6_2atmpS3839[3] = (moonbit_string_t)moonbit_string_literal_7.data;
  _M0L6_2atmpS3839[4] = (moonbit_string_t)moonbit_string_literal_8.data;
  _M0L6_2atmpS3839[5] = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3839[6] = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3839[7] = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6_2atmpS3839[8] = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS3839[9] = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS3839[10] = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS3839[11] = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6_2atmpS3839[12] = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS3839[13] = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS3839[14] = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS3839[15] = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L6_2atmpS3839[16] = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L6_2atmpS3839[17] = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6_2atmpS3839[18] = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS3839[19] = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L6_2atmpS3839[20] = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L6_2atmpS3839[21] = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS3839[22] = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS3839[23] = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L6_2atmpS3839[24] = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L6_2atmpS3839[25] = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L6_2atmpS3839[26] = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS3839[27] = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS3839[28] = (moonbit_string_t)moonbit_string_literal_32.data;
  _M0L6_2atmpS3839[29] = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L6_2atmpS3839[30] = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L6_2atmpS3839[31] = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L6_2atmpS3839[32] = (moonbit_string_t)moonbit_string_literal_36.data;
  _M0L6_2atmpS3839[33] = (moonbit_string_t)moonbit_string_literal_37.data;
  _M0L6_2atmpS3839[34] = (moonbit_string_t)moonbit_string_literal_38.data;
  _M0L6_2atmpS3839[35] = (moonbit_string_t)moonbit_string_literal_39.data;
  _M0L6_2atmpS3839[36] = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L6_2atmpS3839[37] = (moonbit_string_t)moonbit_string_literal_41.data;
  _M0L6_2atmpS3839[38] = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS3839[39] = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS3839[40] = (moonbit_string_t)moonbit_string_literal_44.data;
  _M0L6_2atmpS3839[41] = (moonbit_string_t)moonbit_string_literal_45.data;
  _M0L6_2atmpS3839[42] = (moonbit_string_t)moonbit_string_literal_46.data;
  _M0L6_2atmpS3839[43] = (moonbit_string_t)moonbit_string_literal_47.data;
  _M0L6_2atmpS3839[44] = (moonbit_string_t)moonbit_string_literal_48.data;
  _M0L6_2atmpS3839[45] = (moonbit_string_t)moonbit_string_literal_49.data;
  _M0L6_2atmpS3839[46] = (moonbit_string_t)moonbit_string_literal_50.data;
  _M0L6_2atmpS3839[47] = (moonbit_string_t)moonbit_string_literal_51.data;
  _M0L6_2atmpS3839[48] = (moonbit_string_t)moonbit_string_literal_52.data;
  _M0L6_2atmpS3839[49] = (moonbit_string_t)moonbit_string_literal_53.data;
  _M0L6_2atmpS3839[50] = (moonbit_string_t)moonbit_string_literal_54.data;
  _M0L6_2atmpS3839[51] = (moonbit_string_t)moonbit_string_literal_55.data;
  _M0L6_2atmpS3839[52] = (moonbit_string_t)moonbit_string_literal_56.data;
  _M0L6_2atmpS3839[53] = (moonbit_string_t)moonbit_string_literal_57.data;
  _M0L6_2atmpS3839[54] = (moonbit_string_t)moonbit_string_literal_58.data;
  _M0L6_2atmpS3839[55] = (moonbit_string_t)moonbit_string_literal_59.data;
  _M0L6_2atmpS3839[56] = (moonbit_string_t)moonbit_string_literal_60.data;
  _M0L6_2atmpS3839[57] = (moonbit_string_t)moonbit_string_literal_61.data;
  _M0L6_2atmpS3839[58] = (moonbit_string_t)moonbit_string_literal_62.data;
  _M0L6_2atmpS3839[59] = (moonbit_string_t)moonbit_string_literal_63.data;
  _M0L6_2atmpS3839[60] = (moonbit_string_t)moonbit_string_literal_64.data;
  _M0L6_2atmpS3839[61] = (moonbit_string_t)moonbit_string_literal_65.data;
  _M0L6_2atmpS3839[62] = (moonbit_string_t)moonbit_string_literal_66.data;
  _M0L6_2atmpS3839[63] = (moonbit_string_t)moonbit_string_literal_67.data;
  _M0L6_2atmpS3839[64] = (moonbit_string_t)moonbit_string_literal_68.data;
  _M0L6_2atmpS3839[65] = (moonbit_string_t)moonbit_string_literal_69.data;
  _M0L6_2atmpS3839[66] = (moonbit_string_t)moonbit_string_literal_70.data;
  _M0L6_2atmpS3839[67] = (moonbit_string_t)moonbit_string_literal_71.data;
  _M0L6_2atmpS3839[68] = (moonbit_string_t)moonbit_string_literal_72.data;
  _M0L6_2atmpS3839[69] = (moonbit_string_t)moonbit_string_literal_73.data;
  _M0L6_2atmpS3839[70] = (moonbit_string_t)moonbit_string_literal_74.data;
  _M0L6_2atmpS3839[71] = (moonbit_string_t)moonbit_string_literal_75.data;
  _M0L6_2atmpS3839[72] = (moonbit_string_t)moonbit_string_literal_76.data;
  _M0L6_2atmpS3839[73] = (moonbit_string_t)moonbit_string_literal_77.data;
  _M0L6_2atmpS3839[74] = (moonbit_string_t)moonbit_string_literal_78.data;
  _M0L6_2atmpS3839[75] = (moonbit_string_t)moonbit_string_literal_79.data;
  _M0L6_2atmpS3839[76] = (moonbit_string_t)moonbit_string_literal_80.data;
  _M0L6_2atmpS3839[77] = (moonbit_string_t)moonbit_string_literal_81.data;
  _M0L6_2atmpS3839[78] = (moonbit_string_t)moonbit_string_literal_82.data;
  _M0L6_2atmpS3839[79] = (moonbit_string_t)moonbit_string_literal_83.data;
  _M0L6_2atmpS3839[80] = (moonbit_string_t)moonbit_string_literal_84.data;
  _block_4393
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_block_4393)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _block_4393->$0 = _M0L6_2atmpS3839;
  _block_4393->$1 = 81;
  return _block_4393;
}

moonbit_string_t _M0MP19moonbitDB8Database4ping() {
  #line 1556 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  return (moonbit_string_t)moonbit_string_literal_85.data;
}

moonbit_string_t _M0MP19moonbitDB8Database9randomkey(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1836
) {
  moonbit_string_t* _M0L6_2atmpS3838;
  struct _M0TPB5ArrayGsE* _M0L9all__keysS1834;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3833;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L5_2aitS1835;
  int32_t _M0L6_2atmpS3834;
  #line 1503 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3838 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L9all__keysS1834
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L9all__keysS1834)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L9all__keysS1834->$0 = _M0L6_2atmpS3838;
  _M0L9all__keysS1834->$1 = 0;
  _M0L4dataS3833 = _M0L4selfS1836->$0;
  moonbit_incref(_M0L4dataS3833);
  #line 1504 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1835
  = _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(_M0L4dataS3833);
  moonbit_decref(_M0L4dataS3833);
  while (1) {
    moonbit_string_t _M0L3keyS1838;
    struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2abindS1840;
    int32_t _M0L6_2atmpS3832;
    #line 1505 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1840
    = _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(_M0L5_2aitS1835);
    if (_M0L7_2abindS1840 == 0) {
      if (_M0L7_2abindS1840) {
        moonbit_decref(_M0L7_2abindS1840);
      }
      moonbit_decref(_M0L5_2aitS1835);
    } else {
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1841 =
        _M0L7_2abindS1840;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L4_2axS1842 =
        _M0L7_2aSomeS1841;
      moonbit_string_t _M0L8_2afieldS3875 = _M0L4_2axS1842->$0;
      int32_t _M0L6_2acntS4286 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1842));
      moonbit_string_t _M0L6_2akeyS1843;
      if (_M0L6_2acntS4286 > 1) {
        int32_t _M0L11_2anew__cntS4288 = _M0L6_2acntS4286 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1842), _M0L11_2anew__cntS4288);
        moonbit_incref(_M0L8_2afieldS3875);
      } else if (_M0L6_2acntS4286 == 1) {
        void* _M0L8_2afieldS4287 = _M0L4_2axS1842->$1;
        moonbit_decref(_M0L8_2afieldS4287);
        #line 1505 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1842);
      }
      _M0L6_2akeyS1843 = _M0L8_2afieldS3875;
      _M0L3keyS1838 = _M0L6_2akeyS1843;
      goto join_1837;
    }
    goto joinlet_4395;
    join_1837:;
    #line 1506 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3832
    = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1836, _M0L3keyS1838);
    if (!_M0L6_2atmpS3832) {
      #line 1507 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L9all__keysS1834, _M0L3keyS1838);
      moonbit_decref(_M0L3keyS1838);
    } else {
      moonbit_decref(_M0L3keyS1838);
    }
    continue;
    joinlet_4395:;
    break;
  }
  #line 1510 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3834 = _M0MPC15array5Array6lengthGsE(_M0L9all__keysS1834);
  if (_M0L6_2atmpS3834 == 0) {
    moonbit_decref(_M0L9all__keysS1834);
    return 0;
  } else {
    int32_t _M0L13current__timeS3836 = _M0L4selfS1836->$2;
    int32_t _M0L6_2atmpS3837;
    int32_t _M0L3idxS1844;
    int32_t _M0L9safe__idxS1845;
    moonbit_string_t _M0L6_2atmpS3835;
    #line 1513 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3837 = _M0MPC15array5Array6lengthGsE(_M0L9all__keysS1834);
    _M0L3idxS1844 = _M0L13current__timeS3836 % _M0L6_2atmpS3837;
    if (_M0L3idxS1844 < 0) {
      _M0L9safe__idxS1845 = -_M0L3idxS1844;
    } else {
      _M0L9safe__idxS1845 = _M0L3idxS1844;
    }
    #line 1515 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3835
    = _M0MPC15array5Array2atGsE(_M0L9all__keysS1834, _M0L9safe__idxS1845);
    moonbit_decref(_M0L9all__keysS1834);
    return _M0L6_2atmpS3835;
  }
}

int32_t _M0MP19moonbitDB8Database7flushdb(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1831
) {
  struct _M0TUsRP19moonbitDB10RedisValueE** _M0L7_2abindS1832;
  struct _M0TUsRP19moonbitDB10RedisValueE** _M0L6_2atmpS3828;
  struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE _M0L6_2atmpS3827;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3826;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS3878;
  struct _M0TUsiE** _M0L7_2abindS1833;
  struct _M0TUsiE** _M0L6_2atmpS3831;
  struct _M0TPB9ArrayViewGUsiEE _M0L6_2atmpS3830;
  struct _M0TPB3MapGsiE* _M0L6_2atmpS3829;
  struct _M0TPB3MapGsiE* _M0L6_2aoldS3877;
  #line 1494 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1832
  = (struct _M0TUsRP19moonbitDB10RedisValueE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3828 = _M0L7_2abindS1832;
  _M0L6_2atmpS3827
  = (struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE){
    .$0 = _M0L6_2atmpS3828, .$1 = 0, .$2 = 0
  };
  #line 1495 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3826
  = _M0MPB3Map3MapGsRP19moonbitDB10RedisValueE(_M0L6_2atmpS3827, 1000ll);
  moonbit_decref(_M0L6_2atmpS3827.$0);
  _M0L6_2aoldS3878 = _M0L4selfS1831->$0;
  moonbit_decref(_M0L6_2aoldS3878);
  _M0L4selfS1831->$0 = _M0L6_2atmpS3826;
  _M0L7_2abindS1833 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3831 = _M0L7_2abindS1833;
  _M0L6_2atmpS3830
  = (struct _M0TPB9ArrayViewGUsiEE){
    .$0 = _M0L6_2atmpS3831, .$1 = 0, .$2 = 0
  };
  #line 1496 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3829 = _M0MPB3Map3MapGsiE(_M0L6_2atmpS3830, 1000ll);
  moonbit_decref(_M0L6_2atmpS3830.$0);
  _M0L6_2aoldS3877 = _M0L4selfS1831->$1;
  moonbit_decref(_M0L6_2aoldS3877);
  _M0L4selfS1831->$1 = _M0L6_2atmpS3829;
  return 0;
}

int32_t _M0MP19moonbitDB8Database6dbsize(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1823
) {
  struct _M0TPB8MutLocalGiE* _M0L5countS1821;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3825;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L5_2aitS1822;
  int32_t _result_4398;
  #line 1484 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5countS1821
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L5countS1821)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5countS1821->$0 = 0;
  _M0L4dataS3825 = _M0L4selfS1823->$0;
  moonbit_incref(_M0L4dataS3825);
  #line 1485 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1822
  = _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(_M0L4dataS3825);
  moonbit_decref(_M0L4dataS3825);
  while (1) {
    moonbit_string_t _M0L3keyS1825;
    struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2abindS1827;
    int32_t _M0L6_2atmpS3822;
    #line 1486 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1827
    = _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(_M0L5_2aitS1822);
    if (_M0L7_2abindS1827 == 0) {
      if (_M0L7_2abindS1827) {
        moonbit_decref(_M0L7_2abindS1827);
      }
      moonbit_decref(_M0L5_2aitS1822);
    } else {
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1828 =
        _M0L7_2abindS1827;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L4_2axS1829 =
        _M0L7_2aSomeS1828;
      moonbit_string_t _M0L8_2afieldS3879 = _M0L4_2axS1829->$0;
      int32_t _M0L6_2acntS4289 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1829));
      moonbit_string_t _M0L6_2akeyS1830;
      if (_M0L6_2acntS4289 > 1) {
        int32_t _M0L11_2anew__cntS4291 = _M0L6_2acntS4289 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1829), _M0L11_2anew__cntS4291);
        moonbit_incref(_M0L8_2afieldS3879);
      } else if (_M0L6_2acntS4289 == 1) {
        void* _M0L8_2afieldS4290 = _M0L4_2axS1829->$1;
        moonbit_decref(_M0L8_2afieldS4290);
        #line 1486 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1829);
      }
      _M0L6_2akeyS1830 = _M0L8_2afieldS3879;
      _M0L3keyS1825 = _M0L6_2akeyS1830;
      goto join_1824;
    }
    goto joinlet_4397;
    join_1824:;
    #line 1487 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3822
    = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1823, _M0L3keyS1825);
    moonbit_decref(_M0L3keyS1825);
    if (!_M0L6_2atmpS3822) {
      int32_t _M0L3valS3824 = _M0L5countS1821->$0;
      int32_t _M0L6_2atmpS3823 = _M0L3valS3824 + 1;
      _M0L5countS1821->$0 = _M0L6_2atmpS3823;
    }
    continue;
    joinlet_4397:;
    break;
  }
  _result_4398 = _M0L5countS1821->$0;
  moonbit_decref(_M0L5countS1821);
  return _result_4398;
}

int64_t _M0MP19moonbitDB8Database8zrevrank(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1805,
  moonbit_string_t _M0L3keyS1806,
  moonbit_string_t _M0L11member__valS1810
) {
  #line 1353 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1354 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1805, _M0L3keyS1806)
  ) {
    return 4294967296ll;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1809;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3821 =
      _M0L4selfS1805->$0;
    void* _M0L7_2abindS1816;
    int32_t _M0L6_2atmpS3814;
    moonbit_incref(_M0L4dataS3821);
    #line 1357 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1816
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3821, _M0L3keyS1806);
    moonbit_decref(_M0L4dataS3821);
    if (_M0L7_2abindS1816 == 0) {
      if (_M0L7_2abindS1816) {
        moonbit_decref(_M0L7_2abindS1816);
      }
      goto join_1807;
    } else {
      void* _M0L7_2aSomeS1817 = _M0L7_2abindS1816;
      void* _M0L4_2axS1818 = _M0L7_2aSomeS1817;
      switch (Moonbit_object_tag(_M0L4_2axS1818)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1819 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1818;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3882 = _M0L7_2aZSetS1819->$0;
          int32_t _M0L6_2acntS4294 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1819));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1820;
          if (_M0L6_2acntS4294 > 1) {
            int32_t _M0L11_2anew__cntS4295 = _M0L6_2acntS4294 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1819), _M0L11_2anew__cntS4295);
            moonbit_incref(_M0L8_2afieldS3882);
          } else if (_M0L6_2acntS4294 == 1) {
            #line 1357 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1819);
          }
          _M0L7_2azsetS1820 = _M0L8_2afieldS3882;
          _M0L4zsetS1809 = _M0L7_2azsetS1820;
          goto join_1808;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1818);
          goto join_1807;
          break;
        }
      }
    }
    join_1808:;
    #line 1359 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3814
    = _M0MPB3Map8containsGsfE(_M0L4zsetS1809, _M0L11member__valS1810);
    moonbit_decref(_M0L4zsetS1809);
    if (!_M0L6_2atmpS3814) {
      return 4294967296ll;
    } else {
      struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1811;
      int32_t _M0L3lenS1812;
      struct _M0TPB8MutLocalGiE* _M0L4rankS1813;
      int32_t _M0L1iS1814;
      int32_t _M0L3valS3820;
      #line 1362 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6sortedS1811
      = _M0MP19moonbitDB8Database17get__sorted__zset(_M0L4selfS1805, _M0L3keyS1806);
      #line 1363 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L3lenS1812 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1811);
      _M0L4rankS1813
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L4rankS1813)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L4rankS1813->$0 = 0;
      _M0L1iS1814 = 0;
      while (1) {
        if (_M0L1iS1814 < _M0L3lenS1812) {
          struct _M0TUsfE* _M0L6_2atmpS3816;
          moonbit_string_t _M0L8_2afieldS3881;
          int32_t _M0L6_2acntS4292;
          moonbit_string_t _M0L6_2atmpS3815;
          int32_t _result_4402;
          int32_t _M0L6_2atmpS3819;
          #line 1366 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0L6_2atmpS3816
          = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1811, _M0L1iS1814);
          _M0L8_2afieldS3881 = _M0L6_2atmpS3816->$0;
          _M0L6_2acntS4292
          = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3816));
          if (_M0L6_2acntS4292 > 1) {
            int32_t _M0L11_2anew__cntS4293 = _M0L6_2acntS4292 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3816), _M0L11_2anew__cntS4293);
            moonbit_incref(_M0L8_2afieldS3881);
          } else if (_M0L6_2acntS4292 == 1) {
            #line 1366 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2atmpS3816);
          }
          _M0L6_2atmpS3815 = _M0L8_2afieldS3881;
          #line 1366 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _result_4402
          = _M0L6_2atmpS3815 == _M0L11member__valS1810
            || Moonbit_array_length(_M0L6_2atmpS3815)
               == Moonbit_array_length(_M0L11member__valS1810)
               && 0
                  == memcmp(_M0L6_2atmpS3815, _M0L11member__valS1810, Moonbit_array_length(_M0L6_2atmpS3815) * 2);
          moonbit_decref(_M0L6_2atmpS3815);
          if (_result_4402) {
            int32_t _M0L6_2atmpS3818;
            int32_t _M0L6_2atmpS3817;
            moonbit_decref(_M0L6sortedS1811);
            _M0L6_2atmpS3818 = _M0L3lenS1812 - 1;
            _M0L6_2atmpS3817 = _M0L6_2atmpS3818 - _M0L1iS1814;
            _M0L4rankS1813->$0 = _M0L6_2atmpS3817;
            break;
          }
          _M0L6_2atmpS3819 = _M0L1iS1814 + 1;
          _M0L1iS1814 = _M0L6_2atmpS3819;
          continue;
        } else {
          moonbit_decref(_M0L6sortedS1811);
        }
        break;
      }
      _M0L3valS3820 = _M0L4rankS1813->$0;
      moonbit_decref(_M0L4rankS1813);
      return (int64_t)_M0L3valS3820;
    }
    join_1807:;
    return 4294967296ll;
  }
}

int64_t _M0MP19moonbitDB8Database5zrank(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1790,
  moonbit_string_t _M0L3keyS1791,
  moonbit_string_t _M0L11member__valS1795
) {
  #line 1328 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1329 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1790, _M0L3keyS1791)
  ) {
    return 4294967296ll;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1794;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3813 =
      _M0L4selfS1790->$0;
    void* _M0L7_2abindS1800;
    int32_t _M0L6_2atmpS3807;
    moonbit_incref(_M0L4dataS3813);
    #line 1332 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1800
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3813, _M0L3keyS1791);
    moonbit_decref(_M0L4dataS3813);
    if (_M0L7_2abindS1800 == 0) {
      if (_M0L7_2abindS1800) {
        moonbit_decref(_M0L7_2abindS1800);
      }
      goto join_1792;
    } else {
      void* _M0L7_2aSomeS1801 = _M0L7_2abindS1800;
      void* _M0L4_2axS1802 = _M0L7_2aSomeS1801;
      switch (Moonbit_object_tag(_M0L4_2axS1802)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1803 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1802;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3885 = _M0L7_2aZSetS1803->$0;
          int32_t _M0L6_2acntS4298 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1803));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1804;
          if (_M0L6_2acntS4298 > 1) {
            int32_t _M0L11_2anew__cntS4299 = _M0L6_2acntS4298 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1803), _M0L11_2anew__cntS4299);
            moonbit_incref(_M0L8_2afieldS3885);
          } else if (_M0L6_2acntS4298 == 1) {
            #line 1332 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1803);
          }
          _M0L7_2azsetS1804 = _M0L8_2afieldS3885;
          _M0L4zsetS1794 = _M0L7_2azsetS1804;
          goto join_1793;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1802);
          goto join_1792;
          break;
        }
      }
    }
    join_1793:;
    #line 1334 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3807
    = _M0MPB3Map8containsGsfE(_M0L4zsetS1794, _M0L11member__valS1795);
    moonbit_decref(_M0L4zsetS1794);
    if (!_M0L6_2atmpS3807) {
      return 4294967296ll;
    } else {
      struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1796;
      struct _M0TPB8MutLocalGiE* _M0L4rankS1797;
      int32_t _M0L1iS1798;
      int32_t _M0L3valS3812;
      #line 1337 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6sortedS1796
      = _M0MP19moonbitDB8Database17get__sorted__zset(_M0L4selfS1790, _M0L3keyS1791);
      _M0L4rankS1797
      = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
      Moonbit_object_header(_M0L4rankS1797)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L4rankS1797->$0 = 0;
      _M0L1iS1798 = 0;
      while (1) {
        int32_t _M0L6_2atmpS3808;
        #line 1339 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3808 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1796);
        if (_M0L1iS1798 < _M0L6_2atmpS3808) {
          struct _M0TUsfE* _M0L6_2atmpS3810;
          moonbit_string_t _M0L8_2afieldS3884;
          int32_t _M0L6_2acntS4296;
          moonbit_string_t _M0L6_2atmpS3809;
          int32_t _result_4406;
          int32_t _M0L6_2atmpS3811;
          #line 1340 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0L6_2atmpS3810
          = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1796, _M0L1iS1798);
          _M0L8_2afieldS3884 = _M0L6_2atmpS3810->$0;
          _M0L6_2acntS4296
          = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3810));
          if (_M0L6_2acntS4296 > 1) {
            int32_t _M0L11_2anew__cntS4297 = _M0L6_2acntS4296 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3810), _M0L11_2anew__cntS4297);
            moonbit_incref(_M0L8_2afieldS3884);
          } else if (_M0L6_2acntS4296 == 1) {
            #line 1340 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2atmpS3810);
          }
          _M0L6_2atmpS3809 = _M0L8_2afieldS3884;
          #line 1340 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _result_4406
          = _M0L6_2atmpS3809 == _M0L11member__valS1795
            || Moonbit_array_length(_M0L6_2atmpS3809)
               == Moonbit_array_length(_M0L11member__valS1795)
               && 0
                  == memcmp(_M0L6_2atmpS3809, _M0L11member__valS1795, Moonbit_array_length(_M0L6_2atmpS3809) * 2);
          moonbit_decref(_M0L6_2atmpS3809);
          if (_result_4406) {
            moonbit_decref(_M0L6sortedS1796);
            _M0L4rankS1797->$0 = _M0L1iS1798;
            break;
          }
          _M0L6_2atmpS3811 = _M0L1iS1798 + 1;
          _M0L1iS1798 = _M0L6_2atmpS3811;
          continue;
        } else {
          moonbit_decref(_M0L6sortedS1796);
        }
        break;
      }
      _M0L3valS3812 = _M0L4rankS1797->$0;
      moonbit_decref(_M0L4rankS1797);
      return (int64_t)_M0L3valS3812;
    }
    join_1792:;
    return 4294967296ll;
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database9zrevrange(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1779,
  moonbit_string_t _M0L3keyS1780,
  int32_t _M0L5startS1784,
  int32_t _M0L3endS1786
) {
  struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1778;
  int32_t _M0L3lenS1781;
  moonbit_string_t* _M0L6_2atmpS3806;
  struct _M0TPB5ArrayGsE* _M0L6resultS1782;
  int32_t _M0L10start__idxS1783;
  int32_t _M0L8end__idxS1785;
  int32_t _M0L1iS1787;
  #line 1302 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1303 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6sortedS1778
  = _M0MP19moonbitDB8Database17get__sorted__zset(_M0L4selfS1779, _M0L3keyS1780);
  #line 1304 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L3lenS1781 = _M0MPC15array5Array6lengthGUsfEE(_M0L6sortedS1778);
  _M0L6_2atmpS3806 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1782
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1782)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6resultS1782->$0 = _M0L6_2atmpS3806;
  _M0L6resultS1782->$1 = 0;
  if (_M0L5startS1784 < 0) {
    _M0L10start__idxS1783 = _M0L3lenS1781 + _M0L5startS1784;
  } else {
    _M0L10start__idxS1783 = _M0L5startS1784;
  }
  if (_M0L3endS1786 < 0) {
    _M0L8end__idxS1785 = _M0L3lenS1781 + _M0L3endS1786;
  } else {
    _M0L8end__idxS1785 = _M0L3endS1786;
  }
  _M0L1iS1787 = _M0L10start__idxS1783;
  while (1) {
    int32_t _if__result_4408;
    if (_M0L1iS1787 <= _M0L8end__idxS1785) {
      _if__result_4408 = _M0L1iS1787 < _M0L3lenS1781;
    } else {
      _if__result_4408 = 0;
    }
    if (_if__result_4408) {
      int32_t _M0L6_2atmpS3804 = _M0L3lenS1781 - 1;
      int32_t _M0L8rev__idxS1788 = _M0L6_2atmpS3804 - _M0L1iS1787;
      int32_t _M0L6_2atmpS3805;
      if (_M0L8rev__idxS1788 >= 0) {
        struct _M0TUsfE* _M0L6_2atmpS3803;
        moonbit_string_t _M0L8_2afieldS3887;
        int32_t _M0L6_2acntS4300;
        moonbit_string_t _M0L6_2atmpS3802;
        #line 1311 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3803
        = _M0MPC15array5Array2atGUsfEE(_M0L6sortedS1778, _M0L8rev__idxS1788);
        _M0L8_2afieldS3887 = _M0L6_2atmpS3803->$0;
        _M0L6_2acntS4300
        = Moonbit_rc_count(Moonbit_object_header(_M0L6_2atmpS3803));
        if (_M0L6_2acntS4300 > 1) {
          int32_t _M0L11_2anew__cntS4301 = _M0L6_2acntS4300 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2atmpS3803), _M0L11_2anew__cntS4301);
          moonbit_incref(_M0L8_2afieldS3887);
        } else if (_M0L6_2acntS4300 == 1) {
          #line 1311 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L6_2atmpS3803);
        }
        _M0L6_2atmpS3802 = _M0L8_2afieldS3887;
        #line 1311 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1782, _M0L6_2atmpS3802);
        moonbit_decref(_M0L6_2atmpS3802);
      }
      _M0L6_2atmpS3805 = _M0L1iS1787 + 1;
      _M0L1iS1787 = _M0L6_2atmpS3805;
      continue;
    } else {
      moonbit_decref(_M0L6sortedS1778);
    }
    break;
  }
  return _M0L6resultS1782;
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database13zrangebyscore(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1769,
  moonbit_string_t _M0L3keyS1770,
  float _M0L10min__scoreS1775,
  float _M0L10max__scoreS1776
) {
  struct _M0TPB5ArrayGUsfEE* _M0L6sortedS1768;
  moonbit_string_t* _M0L6_2atmpS3801;
  struct _M0TPB5ArrayGsE* _M0L6resultS1771;
  int32_t _M0L7_2abindS1772;
  int32_t _M0L2__S1773;
  #line 1280 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1281 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6sortedS1768
  = _M0MP19moonbitDB8Database17get__sorted__zset(_M0L4selfS1769, _M0L3keyS1770);
  _M0L6_2atmpS3801 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1771
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1771)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6resultS1771->$0 = _M0L6_2atmpS3801;
  _M0L6resultS1771->$1 = 0;
  _M0L7_2abindS1772 = _M0L6sortedS1768->$1;
  _M0L2__S1773 = 0;
  while (1) {
    if (_M0L2__S1773 < _M0L7_2abindS1772) {
      struct _M0TUsfE** _M0L3bufS3800 = _M0L6sortedS1768->$0;
      struct _M0TUsfE* _M0L4itemS1774 =
        (struct _M0TUsfE*)_M0L3bufS3800[_M0L2__S1773];
      float _M0L6_2atmpS3797 = _M0L4itemS1774->$1;
      int32_t _if__result_4410;
      int32_t _M0L6_2atmpS3799;
      if (_M0L6_2atmpS3797 >= _M0L10min__scoreS1775) {
        float _M0L6_2atmpS3796 = _M0L4itemS1774->$1;
        _if__result_4410 = _M0L6_2atmpS3796 <= _M0L10max__scoreS1776;
      } else {
        _if__result_4410 = 0;
      }
      if (_if__result_4410) {
        moonbit_string_t _M0L6_2atmpS3798 = _M0L4itemS1774->$0;
        moonbit_incref(_M0L6_2atmpS3798);
        #line 1285 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1771, _M0L6_2atmpS3798);
        moonbit_decref(_M0L6_2atmpS3798);
      }
      _M0L6_2atmpS3799 = _M0L2__S1773 + 1;
      _M0L2__S1773 = _M0L6_2atmpS3799;
      continue;
    } else {
      moonbit_decref(_M0L6sortedS1768);
    }
    break;
  }
  return _M0L6resultS1771;
}

struct _M0TPB5ArrayGUsfEE* _M0MP19moonbitDB8Database17get__sorted__zset(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1747,
  moonbit_string_t _M0L3keyS1748
) {
  #line 1263 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1264 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1747, _M0L3keyS1748)
  ) {
    struct _M0TUsfE** _M0L6_2atmpS3791 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _block_4411 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_4411)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 0);
    _block_4411->$0 = _M0L6_2atmpS3791;
    _block_4411->$1 = 0;
    return _block_4411;
  } else {
    struct _M0TPB3MapGsfE* _M0L4zsetS1751;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3795 =
      _M0L4selfS1747->$0;
    void* _M0L7_2abindS1763;
    struct _M0TUsfE** _M0L6_2atmpS3794;
    struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1752;
    struct _M0TPB4IterGUsfEE* _M0L5_2aitS1753;
    struct _M0TPB5ArrayGUsfEE* _result_4416;
    struct _M0TUsfE** _M0L6_2atmpS3792;
    struct _M0TPB5ArrayGUsfEE* _block_4417;
    moonbit_incref(_M0L4dataS3795);
    #line 1267 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1763
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3795, _M0L3keyS1748);
    moonbit_decref(_M0L4dataS3795);
    if (_M0L7_2abindS1763 == 0) {
      if (_M0L7_2abindS1763) {
        moonbit_decref(_M0L7_2abindS1763);
      }
      goto join_1749;
    } else {
      void* _M0L7_2aSomeS1764 = _M0L7_2abindS1763;
      void* _M0L4_2axS1765 = _M0L7_2aSomeS1764;
      switch (Moonbit_object_tag(_M0L4_2axS1765)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1766 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1765;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3892 = _M0L7_2aZSetS1766->$0;
          int32_t _M0L6_2acntS4304 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1766));
          struct _M0TPB3MapGsfE* _M0L7_2azsetS1767;
          if (_M0L6_2acntS4304 > 1) {
            int32_t _M0L11_2anew__cntS4305 = _M0L6_2acntS4304 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1766), _M0L11_2anew__cntS4305);
            moonbit_incref(_M0L8_2afieldS3892);
          } else if (_M0L6_2acntS4304 == 1) {
            #line 1267 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1766);
          }
          _M0L7_2azsetS1767 = _M0L8_2afieldS3892;
          _M0L4zsetS1751 = _M0L7_2azsetS1767;
          goto join_1750;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1765);
          goto join_1749;
          break;
        }
      }
    }
    join_1750:;
    _M0L6_2atmpS3794 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5itemsS1752
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5itemsS1752)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 0);
    _M0L5itemsS1752->$0 = _M0L6_2atmpS3794;
    _M0L5itemsS1752->$1 = 0;
    #line 1269 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L5_2aitS1753 = _M0MPB3Map5iter2GsfE(_M0L4zsetS1751);
    moonbit_decref(_M0L4zsetS1751);
    while (1) {
      moonbit_string_t _M0L1mS1755;
      float _M0L1sS1756;
      struct _M0TUsfE* _M0L7_2abindS1758;
      struct _M0TUsfE* _M0L8_2atupleS3793;
      #line 1270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L7_2abindS1758 = _M0MPB5Iter24nextGsfE(_M0L5_2aitS1753);
      if (_M0L7_2abindS1758 == 0) {
        if (_M0L7_2abindS1758) {
          moonbit_decref(_M0L7_2abindS1758);
        }
        moonbit_decref(_M0L5_2aitS1753);
      } else {
        struct _M0TUsfE* _M0L7_2aSomeS1759 = _M0L7_2abindS1758;
        struct _M0TUsfE* _M0L4_2axS1760 = _M0L7_2aSomeS1759;
        moonbit_string_t _M0L4_2amS1761 = _M0L4_2axS1760->$0;
        float _M0L4_2asS1762 = _M0L4_2axS1760->$1;
        int32_t _M0L6_2acntS4302 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1760));
        if (_M0L6_2acntS4302 > 1) {
          int32_t _M0L11_2anew__cntS4303 = _M0L6_2acntS4302 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1760), _M0L11_2anew__cntS4303);
          moonbit_incref(_M0L4_2amS1761);
        } else if (_M0L6_2acntS4302 == 1) {
          #line 1270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L4_2axS1760);
        }
        _M0L1mS1755 = _M0L4_2amS1761;
        _M0L1sS1756 = _M0L4_2asS1762;
        goto join_1754;
      }
      goto joinlet_4415;
      join_1754:;
      _M0L8_2atupleS3793
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS3793)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
      _M0L8_2atupleS3793->$0 = _M0L1mS1755;
      _M0L8_2atupleS3793->$1 = _M0L1sS1756;
      #line 1271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L5itemsS1752, _M0L8_2atupleS3793);
      moonbit_decref(_M0L8_2atupleS3793);
      continue;
      joinlet_4415:;
      break;
    }
    #line 1273 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4416 = _M0FP19moonbitDB15sort__by__score(_M0L5itemsS1752);
    moonbit_decref(_M0L5itemsS1752);
    return _result_4416;
    join_1749:;
    _M0L6_2atmpS3792 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _block_4417
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_block_4417)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 0);
    _block_4417->$0 = _M0L6_2atmpS3792;
    _block_4417->$1 = 0;
    return _block_4417;
  }
}

void* _M0MP19moonbitDB8Database6zscore(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1736,
  moonbit_string_t _M0L3keyS1737,
  moonbit_string_t _M0L11member__valS1741
) {
  #line 1234 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1235 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1736, _M0L3keyS1737)
  ) {
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1740;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3790 =
      _M0L4selfS1736->$0;
    void* _M0L7_2abindS1742;
    void* _result_4420;
    moonbit_incref(_M0L4dataS3790);
    #line 1238 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1742
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3790, _M0L3keyS1737);
    moonbit_decref(_M0L4dataS3790);
    if (_M0L7_2abindS1742 == 0) {
      if (_M0L7_2abindS1742) {
        moonbit_decref(_M0L7_2abindS1742);
      }
      goto join_1738;
    } else {
      void* _M0L7_2aSomeS1743 = _M0L7_2abindS1742;
      void* _M0L4_2axS1744 = _M0L7_2aSomeS1743;
      switch (Moonbit_object_tag(_M0L4_2axS1744)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1745 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1744;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3894 = _M0L7_2aZSetS1745->$0;
          int32_t _M0L6_2acntS4306 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1745));
          struct _M0TPB3MapGsfE* _M0L4_2azS1746;
          if (_M0L6_2acntS4306 > 1) {
            int32_t _M0L11_2anew__cntS4307 = _M0L6_2acntS4306 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1745), _M0L11_2anew__cntS4307);
            moonbit_incref(_M0L8_2afieldS3894);
          } else if (_M0L6_2acntS4306 == 1) {
            #line 1238 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1745);
          }
          _M0L4_2azS1746 = _M0L8_2afieldS3894;
          _M0L1zS1740 = _M0L4_2azS1746;
          goto join_1739;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1744);
          goto join_1738;
          break;
        }
      }
    }
    join_1739:;
    #line 1239 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4420 = _M0MPB3Map3getGsfE(_M0L1zS1740, _M0L11member__valS1741);
    moonbit_decref(_M0L1zS1740);
    return _result_4420;
    join_1738:;
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
}

int32_t _M0MP19moonbitDB8Database5zcard(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1727,
  moonbit_string_t _M0L3keyS1728
) {
  #line 1223 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1224 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1727, _M0L3keyS1728)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsfE* _M0L1zS1730;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3789 =
      _M0L4selfS1727->$0;
    void* _M0L7_2abindS1731;
    int32_t _result_4422;
    moonbit_incref(_M0L4dataS3789);
    #line 1227 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1731
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3789, _M0L3keyS1728);
    moonbit_decref(_M0L4dataS3789);
    if (_M0L7_2abindS1731 == 0) {
      if (_M0L7_2abindS1731) {
        moonbit_decref(_M0L7_2abindS1731);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1732 = _M0L7_2abindS1731;
      void* _M0L4_2axS1733 = _M0L7_2aSomeS1732;
      switch (Moonbit_object_tag(_M0L4_2axS1733)) {
        case 4: {
          struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1734 =
            (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1733;
          struct _M0TPB3MapGsfE* _M0L8_2afieldS3896 = _M0L7_2aZSetS1734->$0;
          int32_t _M0L6_2acntS4308 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1734));
          struct _M0TPB3MapGsfE* _M0L4_2azS1735;
          if (_M0L6_2acntS4308 > 1) {
            int32_t _M0L11_2anew__cntS4309 = _M0L6_2acntS4308 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1734), _M0L11_2anew__cntS4309);
            moonbit_incref(_M0L8_2afieldS3896);
          } else if (_M0L6_2acntS4308 == 1) {
            #line 1227 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aZSetS1734);
          }
          _M0L4_2azS1735 = _M0L8_2afieldS3896;
          _M0L1zS1730 = _M0L4_2azS1735;
          goto join_1729;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1733);
          return 0;
          break;
        }
      }
    }
    join_1729:;
    #line 1228 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4422 = _M0MPB3Map6lengthGsfE(_M0L1zS1730);
    moonbit_decref(_M0L1zS1730);
    return _result_4422;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP19moonbitDB15sort__by__score(
  struct _M0TPB5ArrayGUsfEE* _M0L5itemsS1726
) {
  #line 1177 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1178 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  return _M0FP19moonbitDB11merge__sort(_M0L5itemsS1726);
}

struct _M0TPB5ArrayGUsfEE* _M0FP19moonbitDB11merge__sort(
  struct _M0TPB5ArrayGUsfEE* _M0L3arrS1718
) {
  int32_t _M0L3lenS1717;
  #line 1181 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1182 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L3lenS1717 = _M0MPC15array5Array6lengthGUsfEE(_M0L3arrS1718);
  if (_M0L3lenS1717 <= 1) {
    moonbit_incref(_M0L3arrS1718);
    return _M0L3arrS1718;
  } else {
    int32_t _M0L3midS1719 = _M0L3lenS1717 / 2;
    struct _M0TUsfE** _M0L6_2atmpS3788 =
      (struct _M0TUsfE**)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGUsfEE* _M0L4leftS1720 =
      (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    struct _M0TUsfE** _M0L6_2atmpS3787;
    struct _M0TPB5ArrayGUsfEE* _M0L5rightS1721;
    int32_t _M0L1iS1722;
    int32_t _M0L1iS1724;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS3785;
    struct _M0TPB5ArrayGUsfEE* _M0L6_2atmpS3786;
    struct _M0TPB5ArrayGUsfEE* _result_4425;
    Moonbit_object_header(_M0L4leftS1720)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 0);
    _M0L4leftS1720->$0 = _M0L6_2atmpS3788;
    _M0L4leftS1720->$1 = 0;
    _M0L6_2atmpS3787 = (struct _M0TUsfE**)moonbit_empty_ref_array;
    _M0L5rightS1721
    = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
    Moonbit_object_header(_M0L5rightS1721)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 0);
    _M0L5rightS1721->$0 = _M0L6_2atmpS3787;
    _M0L5rightS1721->$1 = 0;
    _M0L1iS1722 = 0;
    while (1) {
      if (_M0L1iS1722 < _M0L3midS1719) {
        struct _M0TUsfE* _M0L6_2atmpS3781;
        int32_t _M0L6_2atmpS3782;
        #line 1190 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3781
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1718, _M0L1iS1722);
        #line 1190 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L4leftS1720, _M0L6_2atmpS3781);
        moonbit_decref(_M0L6_2atmpS3781);
        _M0L6_2atmpS3782 = _M0L1iS1722 + 1;
        _M0L1iS1722 = _M0L6_2atmpS3782;
        continue;
      }
      break;
    }
    _M0L1iS1724 = _M0L3midS1719;
    while (1) {
      if (_M0L1iS1724 < _M0L3lenS1717) {
        struct _M0TUsfE* _M0L6_2atmpS3783;
        int32_t _M0L6_2atmpS3784;
        #line 1193 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3783
        = _M0MPC15array5Array2atGUsfEE(_M0L3arrS1718, _M0L1iS1724);
        #line 1193 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L5rightS1721, _M0L6_2atmpS3783);
        moonbit_decref(_M0L6_2atmpS3783);
        _M0L6_2atmpS3784 = _M0L1iS1724 + 1;
        _M0L1iS1724 = _M0L6_2atmpS3784;
        continue;
      }
      break;
    }
    #line 1195 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3785 = _M0FP19moonbitDB11merge__sort(_M0L4leftS1720);
    moonbit_decref(_M0L4leftS1720);
    #line 1195 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3786 = _M0FP19moonbitDB11merge__sort(_M0L5rightS1721);
    moonbit_decref(_M0L5rightS1721);
    #line 1195 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4425 = _M0FP19moonbitDB5merge(_M0L6_2atmpS3785, _M0L6_2atmpS3786);
    moonbit_decref(_M0L6_2atmpS3785);
    moonbit_decref(_M0L6_2atmpS3786);
    return _result_4425;
  }
}

struct _M0TPB5ArrayGUsfEE* _M0FP19moonbitDB5merge(
  struct _M0TPB5ArrayGUsfEE* _M0L4leftS1712,
  struct _M0TPB5ArrayGUsfEE* _M0L5rightS1713
) {
  struct _M0TUsfE** _M0L6_2atmpS3780;
  struct _M0TPB5ArrayGUsfEE* _M0L6resultS1709;
  struct _M0TPB8MutLocalGiE* _M0L1iS1710;
  struct _M0TPB8MutLocalGiE* _M0L1jS1711;
  #line 1199 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3780 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6resultS1709
  = (struct _M0TPB5ArrayGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsfEE));
  Moonbit_object_header(_M0L6resultS1709)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 7, 0);
  _M0L6resultS1709->$0 = _M0L6_2atmpS3780;
  _M0L6resultS1709->$1 = 0;
  _M0L1iS1710
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1iS1710)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1iS1710->$0 = 0;
  _M0L1jS1711
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L1jS1711)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L1jS1711->$0 = 0;
  while (1) {
    int32_t _M0L3valS3752 = _M0L1iS1710->$0;
    int32_t _M0L6_2atmpS3753;
    int32_t _if__result_4427;
    #line 1203 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3753 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1712);
    if (_M0L3valS3752 < _M0L6_2atmpS3753) {
      int32_t _M0L3valS3750 = _M0L1jS1711->$0;
      int32_t _M0L6_2atmpS3751;
      #line 1203 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3751 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1713);
      _if__result_4427 = _M0L3valS3750 < _M0L6_2atmpS3751;
    } else {
      _if__result_4427 = 0;
    }
    if (_if__result_4427) {
      int32_t _M0L3valS3759 = _M0L1iS1710->$0;
      struct _M0TUsfE* _M0L6_2atmpS3758;
      float _M0L6_2atmpS3754;
      int32_t _M0L3valS3757;
      struct _M0TUsfE* _M0L6_2atmpS3756;
      float _M0L6_2atmpS3755;
      #line 1204 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3758
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1712, _M0L3valS3759);
      _M0L6_2atmpS3754 = _M0L6_2atmpS3758->$1;
      moonbit_decref(_M0L6_2atmpS3758);
      _M0L3valS3757 = _M0L1jS1711->$0;
      #line 1204 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3756
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1713, _M0L3valS3757);
      _M0L6_2atmpS3755 = _M0L6_2atmpS3756->$1;
      moonbit_decref(_M0L6_2atmpS3756);
      if (_M0L6_2atmpS3754 <= _M0L6_2atmpS3755) {
        int32_t _M0L3valS3761 = _M0L1iS1710->$0;
        struct _M0TUsfE* _M0L6_2atmpS3760;
        int32_t _M0L3valS3763;
        int32_t _M0L6_2atmpS3762;
        #line 1205 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3760
        = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1712, _M0L3valS3761);
        #line 1205 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1709, _M0L6_2atmpS3760);
        moonbit_decref(_M0L6_2atmpS3760);
        _M0L3valS3763 = _M0L1iS1710->$0;
        _M0L6_2atmpS3762 = _M0L3valS3763 + 1;
        _M0L1iS1710->$0 = _M0L6_2atmpS3762;
      } else {
        int32_t _M0L3valS3765 = _M0L1jS1711->$0;
        struct _M0TUsfE* _M0L6_2atmpS3764;
        int32_t _M0L3valS3767;
        int32_t _M0L6_2atmpS3766;
        #line 1208 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3764
        = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1713, _M0L3valS3765);
        #line 1208 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1709, _M0L6_2atmpS3764);
        moonbit_decref(_M0L6_2atmpS3764);
        _M0L3valS3767 = _M0L1jS1711->$0;
        _M0L6_2atmpS3766 = _M0L3valS3767 + 1;
        _M0L1jS1711->$0 = _M0L6_2atmpS3766;
      }
      continue;
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS3768 = _M0L1iS1710->$0;
    int32_t _M0L6_2atmpS3769;
    #line 1212 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3769 = _M0MPC15array5Array6lengthGUsfEE(_M0L4leftS1712);
    if (_M0L3valS3768 < _M0L6_2atmpS3769) {
      int32_t _M0L3valS3771 = _M0L1iS1710->$0;
      struct _M0TUsfE* _M0L6_2atmpS3770;
      int32_t _M0L3valS3773;
      int32_t _M0L6_2atmpS3772;
      #line 1213 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3770
      = _M0MPC15array5Array2atGUsfEE(_M0L4leftS1712, _M0L3valS3771);
      #line 1213 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1709, _M0L6_2atmpS3770);
      moonbit_decref(_M0L6_2atmpS3770);
      _M0L3valS3773 = _M0L1iS1710->$0;
      _M0L6_2atmpS3772 = _M0L3valS3773 + 1;
      _M0L1iS1710->$0 = _M0L6_2atmpS3772;
      continue;
    } else {
      moonbit_decref(_M0L1iS1710);
    }
    break;
  }
  while (1) {
    int32_t _M0L3valS3774 = _M0L1jS1711->$0;
    int32_t _M0L6_2atmpS3775;
    #line 1216 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3775 = _M0MPC15array5Array6lengthGUsfEE(_M0L5rightS1713);
    if (_M0L3valS3774 < _M0L6_2atmpS3775) {
      int32_t _M0L3valS3777 = _M0L1jS1711->$0;
      struct _M0TUsfE* _M0L6_2atmpS3776;
      int32_t _M0L3valS3779;
      int32_t _M0L6_2atmpS3778;
      #line 1217 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3776
      = _M0MPC15array5Array2atGUsfEE(_M0L5rightS1713, _M0L3valS3777);
      #line 1217 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGUsfEE(_M0L6resultS1709, _M0L6_2atmpS3776);
      moonbit_decref(_M0L6_2atmpS3776);
      _M0L3valS3779 = _M0L1jS1711->$0;
      _M0L6_2atmpS3778 = _M0L3valS3779 + 1;
      _M0L1jS1711->$0 = _M0L6_2atmpS3778;
      continue;
    } else {
      moonbit_decref(_M0L1jS1711);
    }
    break;
  }
  return _M0L6resultS1709;
}

int32_t _M0MP19moonbitDB8Database4zadd(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1694,
  moonbit_string_t _M0L3keyS1695,
  float _M0L5scoreS1708,
  moonbit_string_t _M0L11member__valS1707
) {
  int32_t _M0L6_2atmpS3744;
  struct _M0TPB3MapGsfE* _M0L4zsetS1696;
  struct _M0TPB3MapGsfE* _M0L1zS1700;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3749;
  void* _M0L7_2abindS1701;
  struct _M0TUsfE** _M0L7_2abindS1698;
  struct _M0TUsfE** _M0L6_2atmpS3748;
  struct _M0TPB9ArrayViewGUsfEE _M0L6_2atmpS3747;
  int32_t _M0L7existedS1706;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3745;
  void* _M0L4ZSetS3746;
  #line 1140 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 1141 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3744
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1694, _M0L3keyS1695);
  _M0L4dataS3749 = _M0L4selfS1694->$0;
  moonbit_incref(_M0L4dataS3749);
  #line 1142 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1701
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3749, _M0L3keyS1695);
  moonbit_decref(_M0L4dataS3749);
  if (_M0L7_2abindS1701 == 0) {
    if (_M0L7_2abindS1701) {
      moonbit_decref(_M0L7_2abindS1701);
    }
    goto join_1697;
  } else {
    void* _M0L7_2aSomeS1702 = _M0L7_2abindS1701;
    void* _M0L4_2axS1703 = _M0L7_2aSomeS1702;
    switch (Moonbit_object_tag(_M0L4_2axS1703)) {
      case 4: {
        struct _M0DTP19moonbitDB10RedisValue4ZSet* _M0L7_2aZSetS1704 =
          (struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4_2axS1703;
        struct _M0TPB3MapGsfE* _M0L8_2afieldS3899 = _M0L7_2aZSetS1704->$0;
        int32_t _M0L6_2acntS4310 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aZSetS1704));
        struct _M0TPB3MapGsfE* _M0L4_2azS1705;
        if (_M0L6_2acntS4310 > 1) {
          int32_t _M0L11_2anew__cntS4311 = _M0L6_2acntS4310 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aZSetS1704), _M0L11_2anew__cntS4311);
          moonbit_incref(_M0L8_2afieldS3899);
        } else if (_M0L6_2acntS4310 == 1) {
          #line 1142 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aZSetS1704);
        }
        _M0L4_2azS1705 = _M0L8_2afieldS3899;
        _M0L1zS1700 = _M0L4_2azS1705;
        goto join_1699;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1703);
        goto join_1697;
        break;
      }
    }
  }
  goto joinlet_4431;
  join_1699:;
  _M0L4zsetS1696 = _M0L1zS1700;
  joinlet_4431:;
  goto joinlet_4430;
  join_1697:;
  _M0L7_2abindS1698 = (struct _M0TUsfE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3748 = _M0L7_2abindS1698;
  _M0L6_2atmpS3747
  = (struct _M0TPB9ArrayViewGUsfEE){
    .$0 = _M0L6_2atmpS3748, .$1 = 0, .$2 = 0
  };
  #line 1144 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4zsetS1696 = _M0MPB3Map3MapGsfE(_M0L6_2atmpS3747, 10ll);
  moonbit_decref(_M0L6_2atmpS3747.$0);
  joinlet_4430:;
  #line 1146 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7existedS1706
  = _M0MPB3Map8containsGsfE(_M0L4zsetS1696, _M0L11member__valS1707);
  #line 1147 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsfE(_M0L4zsetS1696, _M0L11member__valS1707, _M0L5scoreS1708);
  _M0L4dataS3745 = _M0L4selfS1694->$0;
  _M0L4ZSetS3746
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4ZSet));
  Moonbit_object_header(_M0L4ZSetS3746)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 13, 4);
  ((struct _M0DTP19moonbitDB10RedisValue4ZSet*)_M0L4ZSetS3746)->$0
  = _M0L4zsetS1696;
  moonbit_incref(_M0L4dataS3745);
  #line 1148 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3745, _M0L3keyS1695, _M0L4ZSetS3746);
  moonbit_decref(_M0L4dataS3745);
  moonbit_decref(_M0L4ZSetS3746);
  return !_M0L7existedS1706;
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database6sunion(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1675,
  struct _M0TPB5ArrayGsE* _M0L4keysS1671
) {
  struct _M0TUsbE** _M0L7_2abindS1669;
  struct _M0TUsbE** _M0L6_2atmpS3743;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3742;
  struct _M0TPB3MapGsbE* _M0L6resultS1668;
  int32_t _M0L7_2abindS1670;
  int32_t _M0L2__S1672;
  moonbit_string_t* _M0L6_2atmpS3741;
  struct _M0TPB5ArrayGsE* _M0L3arrS1685;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1686;
  #line 1026 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1669 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3743 = _M0L7_2abindS1669;
  _M0L6_2atmpS3742
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3743, .$1 = 0, .$2 = 0
  };
  #line 1027 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6resultS1668 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3742, 10ll);
  moonbit_decref(_M0L6_2atmpS3742.$0);
  _M0L7_2abindS1670 = _M0L4keysS1671->$1;
  _M0L2__S1672 = 0;
  while (1) {
    if (_M0L2__S1672 < _M0L7_2abindS1670) {
      moonbit_string_t* _M0L3bufS3740 = _M0L4keysS1671->$0;
      moonbit_string_t _M0L3keyS1673 =
        (moonbit_string_t)_M0L3bufS3740[_M0L2__S1672];
      struct _M0TPB3MapGsbE* _M0L12current__setS1674;
      struct _M0TPB4IterGUsbEE* _M0L5_2aitS1676;
      int32_t _M0L6_2atmpS3739;
      moonbit_incref(_M0L3keyS1673);
      #line 1029 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L12current__setS1674
      = _M0MP19moonbitDB8Database17get__set__members(_M0L4selfS1675, _M0L3keyS1673);
      moonbit_decref(_M0L3keyS1673);
      #line 1029 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L5_2aitS1676 = _M0MPB3Map5iter2GsbE(_M0L12current__setS1674);
      moonbit_decref(_M0L12current__setS1674);
      while (1) {
        moonbit_string_t _M0L1mS1678;
        struct _M0TUsbE* _M0L7_2abindS1680;
        #line 1030 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L7_2abindS1680 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1676);
        if (_M0L7_2abindS1680 == 0) {
          if (_M0L7_2abindS1680) {
            moonbit_decref(_M0L7_2abindS1680);
          }
          moonbit_decref(_M0L5_2aitS1676);
        } else {
          struct _M0TUsbE* _M0L7_2aSomeS1681 = _M0L7_2abindS1680;
          struct _M0TUsbE* _M0L4_2axS1682 = _M0L7_2aSomeS1681;
          moonbit_string_t _M0L8_2afieldS3902 = _M0L4_2axS1682->$0;
          int32_t _M0L6_2acntS4312 =
            Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1682));
          moonbit_string_t _M0L4_2amS1683;
          if (_M0L6_2acntS4312 > 1) {
            int32_t _M0L11_2anew__cntS4313 = _M0L6_2acntS4312 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1682), _M0L11_2anew__cntS4313);
            moonbit_incref(_M0L8_2afieldS3902);
          } else if (_M0L6_2acntS4312 == 1) {
            #line 1030 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L4_2axS1682);
          }
          _M0L4_2amS1683 = _M0L8_2afieldS3902;
          _M0L1mS1678 = _M0L4_2amS1683;
          goto join_1677;
        }
        goto joinlet_4434;
        join_1677:;
        #line 1031 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPB3Map3setGsbE(_M0L6resultS1668, _M0L1mS1678, 1);
        moonbit_decref(_M0L1mS1678);
        continue;
        joinlet_4434:;
        break;
      }
      _M0L6_2atmpS3739 = _M0L2__S1672 + 1;
      _M0L2__S1672 = _M0L6_2atmpS3739;
      continue;
    }
    break;
  }
  _M0L6_2atmpS3741 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3arrS1685
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3arrS1685)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L3arrS1685->$0 = _M0L6_2atmpS3741;
  _M0L3arrS1685->$1 = 0;
  #line 1034 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1686 = _M0MPB3Map5iter2GsbE(_M0L6resultS1668);
  moonbit_decref(_M0L6resultS1668);
  while (1) {
    moonbit_string_t _M0L1mS1688;
    struct _M0TUsbE* _M0L7_2abindS1690;
    #line 1035 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1690 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1686);
    if (_M0L7_2abindS1690 == 0) {
      if (_M0L7_2abindS1690) {
        moonbit_decref(_M0L7_2abindS1690);
      }
      moonbit_decref(_M0L5_2aitS1686);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1691 = _M0L7_2abindS1690;
      struct _M0TUsbE* _M0L4_2axS1692 = _M0L7_2aSomeS1691;
      moonbit_string_t _M0L8_2afieldS3901 = _M0L4_2axS1692->$0;
      int32_t _M0L6_2acntS4314 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1692));
      moonbit_string_t _M0L4_2amS1693;
      if (_M0L6_2acntS4314 > 1) {
        int32_t _M0L11_2anew__cntS4315 = _M0L6_2acntS4314 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1692), _M0L11_2anew__cntS4315);
        moonbit_incref(_M0L8_2afieldS3901);
      } else if (_M0L6_2acntS4314 == 1) {
        #line 1035 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1692);
      }
      _M0L4_2amS1693 = _M0L8_2afieldS3901;
      _M0L1mS1688 = _M0L4_2amS1693;
      goto join_1687;
    }
    goto joinlet_4436;
    join_1687:;
    #line 1036 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3arrS1685, _M0L1mS1688);
    moonbit_decref(_M0L1mS1688);
    continue;
    joinlet_4436:;
    break;
  }
  return _M0L3arrS1685;
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database6sinter(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1632,
  struct _M0TPB5ArrayGsE* _M0L4keysS1630
) {
  int32_t _M0L6_2atmpS3722;
  moonbit_string_t _M0L6_2atmpS3738;
  struct _M0TPB3MapGsbE* _M0L10first__setS1631;
  int32_t _M0L6_2atmpS3724;
  struct _M0TUsbE** _M0L7_2abindS1634;
  struct _M0TUsbE** _M0L6_2atmpS3737;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3734;
  int32_t _M0L6_2atmpS3736;
  int64_t _M0L6_2atmpS3735;
  struct _M0TPB3MapGsbE* _M0L6resultS1633;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1635;
  int32_t _M0L1iS1643;
  moonbit_string_t* _M0L6_2atmpS3733;
  struct _M0TPB5ArrayGsE* _M0L3arrS1659;
  struct _M0TPB4IterGUsbEE* _M0L5_2aitS1660;
  #line 985 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 986 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3722 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1630);
  if (_M0L6_2atmpS3722 == 0) {
    moonbit_string_t* _M0L6_2atmpS3723 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4437 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4437)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_4437->$0 = _M0L6_2atmpS3723;
    _block_4437->$1 = 0;
    return _block_4437;
  }
  #line 989 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3738 = _M0MPC15array5Array2atGsE(_M0L4keysS1630, 0);
  #line 989 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L10first__setS1631
  = _M0MP19moonbitDB8Database17get__set__members(_M0L4selfS1632, _M0L6_2atmpS3738);
  moonbit_decref(_M0L6_2atmpS3738);
  #line 990 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3724 = _M0MPB3Map6lengthGsbE(_M0L10first__setS1631);
  if (_M0L6_2atmpS3724 == 0) {
    moonbit_string_t* _M0L6_2atmpS3725;
    struct _M0TPB5ArrayGsE* _block_4438;
    moonbit_decref(_M0L10first__setS1631);
    _M0L6_2atmpS3725 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_4438
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4438)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_4438->$0 = _M0L6_2atmpS3725;
    _block_4438->$1 = 0;
    return _block_4438;
  }
  _M0L7_2abindS1634 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3737 = _M0L7_2abindS1634;
  _M0L6_2atmpS3734
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3737, .$1 = 0, .$2 = 0
  };
  #line 993 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3736 = _M0MPB3Map6lengthGsbE(_M0L10first__setS1631);
  _M0L6_2atmpS3735 = (int64_t)_M0L6_2atmpS3736;
  #line 993 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6resultS1633 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3734, _M0L6_2atmpS3735);
  moonbit_decref(_M0L6_2atmpS3734.$0);
  #line 993 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1635 = _M0MPB3Map5iter2GsbE(_M0L10first__setS1631);
  moonbit_decref(_M0L10first__setS1631);
  while (1) {
    moonbit_string_t _M0L1mS1637;
    struct _M0TUsbE* _M0L7_2abindS1639;
    #line 994 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1639 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1635);
    if (_M0L7_2abindS1639 == 0) {
      if (_M0L7_2abindS1639) {
        moonbit_decref(_M0L7_2abindS1639);
      }
      moonbit_decref(_M0L5_2aitS1635);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1640 = _M0L7_2abindS1639;
      struct _M0TUsbE* _M0L4_2axS1641 = _M0L7_2aSomeS1640;
      moonbit_string_t _M0L8_2afieldS3909 = _M0L4_2axS1641->$0;
      int32_t _M0L6_2acntS4316 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1641));
      moonbit_string_t _M0L4_2amS1642;
      if (_M0L6_2acntS4316 > 1) {
        int32_t _M0L11_2anew__cntS4317 = _M0L6_2acntS4316 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1641), _M0L11_2anew__cntS4317);
        moonbit_incref(_M0L8_2afieldS3909);
      } else if (_M0L6_2acntS4316 == 1) {
        #line 994 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1641);
      }
      _M0L4_2amS1642 = _M0L8_2afieldS3909;
      _M0L1mS1637 = _M0L4_2amS1642;
      goto join_1636;
    }
    goto joinlet_4440;
    join_1636:;
    #line 995 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsbE(_M0L6resultS1633, _M0L1mS1637, 1);
    moonbit_decref(_M0L1mS1637);
    continue;
    joinlet_4440:;
    break;
  }
  _M0L1iS1643 = 1;
  while (1) {
    int32_t _M0L6_2atmpS3726;
    #line 997 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3726 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1630);
    if (_M0L1iS1643 < _M0L6_2atmpS3726) {
      moonbit_string_t _M0L6_2atmpS3731;
      struct _M0TPB3MapGsbE* _M0L12current__setS1644;
      moonbit_string_t* _M0L6_2atmpS3730;
      struct _M0TPB5ArrayGsE* _M0L10to__removeS1645;
      struct _M0TPB4IterGUsbEE* _M0L5_2aitS1646;
      int32_t _M0L7_2abindS1654;
      int32_t _M0L2__S1655;
      int32_t _M0L6_2atmpS3732;
      #line 998 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3731
      = _M0MPC15array5Array2atGsE(_M0L4keysS1630, _M0L1iS1643);
      #line 998 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L12current__setS1644
      = _M0MP19moonbitDB8Database17get__set__members(_M0L4selfS1632, _M0L6_2atmpS3731);
      moonbit_decref(_M0L6_2atmpS3731);
      _M0L6_2atmpS3730 = (moonbit_string_t*)moonbit_empty_ref_array;
      _M0L10to__removeS1645
      = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
      Moonbit_object_header(_M0L10to__removeS1645)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
      _M0L10to__removeS1645->$0 = _M0L6_2atmpS3730;
      _M0L10to__removeS1645->$1 = 0;
      #line 999 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L5_2aitS1646 = _M0MPB3Map5iter2GsbE(_M0L6resultS1633);
      while (1) {
        moonbit_string_t _M0L1mS1648;
        struct _M0TUsbE* _M0L7_2abindS1650;
        int32_t _M0L6_2atmpS3727;
        #line 1000 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L7_2abindS1650 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1646);
        if (_M0L7_2abindS1650 == 0) {
          if (_M0L7_2abindS1650) {
            moonbit_decref(_M0L7_2abindS1650);
          }
          moonbit_decref(_M0L5_2aitS1646);
          moonbit_decref(_M0L12current__setS1644);
        } else {
          struct _M0TUsbE* _M0L7_2aSomeS1651 = _M0L7_2abindS1650;
          struct _M0TUsbE* _M0L4_2axS1652 = _M0L7_2aSomeS1651;
          moonbit_string_t _M0L8_2afieldS3908 = _M0L4_2axS1652->$0;
          int32_t _M0L6_2acntS4318 =
            Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1652));
          moonbit_string_t _M0L4_2amS1653;
          if (_M0L6_2acntS4318 > 1) {
            int32_t _M0L11_2anew__cntS4319 = _M0L6_2acntS4318 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1652), _M0L11_2anew__cntS4319);
            moonbit_incref(_M0L8_2afieldS3908);
          } else if (_M0L6_2acntS4318 == 1) {
            #line 1000 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L4_2axS1652);
          }
          _M0L4_2amS1653 = _M0L8_2afieldS3908;
          _M0L1mS1648 = _M0L4_2amS1653;
          goto join_1647;
        }
        goto joinlet_4443;
        join_1647:;
        #line 1001 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3727
        = _M0MPB3Map8containsGsbE(_M0L12current__setS1644, _M0L1mS1648);
        if (!_M0L6_2atmpS3727) {
          #line 1002 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0MPC15array5Array4pushGsE(_M0L10to__removeS1645, _M0L1mS1648);
          moonbit_decref(_M0L1mS1648);
        } else {
          moonbit_decref(_M0L1mS1648);
        }
        continue;
        joinlet_4443:;
        break;
      }
      _M0L7_2abindS1654 = _M0L10to__removeS1645->$1;
      _M0L2__S1655 = 0;
      while (1) {
        if (_M0L2__S1655 < _M0L7_2abindS1654) {
          moonbit_string_t* _M0L3bufS3729 = _M0L10to__removeS1645->$0;
          moonbit_string_t _M0L1mS1656 =
            (moonbit_string_t)_M0L3bufS3729[_M0L2__S1655];
          int32_t _M0L6_2atmpS3728;
          moonbit_incref(_M0L1mS1656);
          #line 1006 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          _M0MPB3Map6removeGsbE(_M0L6resultS1633, _M0L1mS1656);
          moonbit_decref(_M0L1mS1656);
          _M0L6_2atmpS3728 = _M0L2__S1655 + 1;
          _M0L2__S1655 = _M0L6_2atmpS3728;
          continue;
        } else {
          moonbit_decref(_M0L10to__removeS1645);
        }
        break;
      }
      _M0L6_2atmpS3732 = _M0L1iS1643 + 1;
      _M0L1iS1643 = _M0L6_2atmpS3732;
      continue;
    }
    break;
  }
  _M0L6_2atmpS3733 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3arrS1659
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3arrS1659)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L3arrS1659->$0 = _M0L6_2atmpS3733;
  _M0L3arrS1659->$1 = 0;
  #line 1009 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1660 = _M0MPB3Map5iter2GsbE(_M0L6resultS1633);
  moonbit_decref(_M0L6resultS1633);
  while (1) {
    moonbit_string_t _M0L1mS1662;
    struct _M0TUsbE* _M0L7_2abindS1664;
    #line 1010 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1664 = _M0MPB5Iter24nextGsbE(_M0L5_2aitS1660);
    if (_M0L7_2abindS1664 == 0) {
      if (_M0L7_2abindS1664) {
        moonbit_decref(_M0L7_2abindS1664);
      }
      moonbit_decref(_M0L5_2aitS1660);
    } else {
      struct _M0TUsbE* _M0L7_2aSomeS1665 = _M0L7_2abindS1664;
      struct _M0TUsbE* _M0L4_2axS1666 = _M0L7_2aSomeS1665;
      moonbit_string_t _M0L8_2afieldS3905 = _M0L4_2axS1666->$0;
      int32_t _M0L6_2acntS4320 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1666));
      moonbit_string_t _M0L4_2amS1667;
      if (_M0L6_2acntS4320 > 1) {
        int32_t _M0L11_2anew__cntS4321 = _M0L6_2acntS4320 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1666), _M0L11_2anew__cntS4321);
        moonbit_incref(_M0L8_2afieldS3905);
      } else if (_M0L6_2acntS4320 == 1) {
        #line 1010 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1666);
      }
      _M0L4_2amS1667 = _M0L8_2afieldS3905;
      _M0L1mS1662 = _M0L4_2amS1667;
      goto join_1661;
    }
    goto joinlet_4446;
    join_1661:;
    #line 1011 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3arrS1659, _M0L1mS1662);
    moonbit_decref(_M0L1mS1662);
    continue;
    joinlet_4446:;
    break;
  }
  return _M0L3arrS1659;
}

struct _M0TPB3MapGsbE* _M0MP19moonbitDB8Database17get__set__members(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1618,
  moonbit_string_t _M0L3keyS1619
) {
  #line 974 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 975 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1618, _M0L3keyS1619)
  ) {
    struct _M0TUsbE** _M0L7_2abindS1620 =
      (struct _M0TUsbE**)moonbit_empty_ref_array;
    struct _M0TUsbE** _M0L6_2atmpS3718 = _M0L7_2abindS1620;
    struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3717 =
      (struct _M0TPB9ArrayViewGUsbEE){.$0 = _M0L6_2atmpS3718,
                                        .$1 = 0,
                                        .$2 = 0};
    struct _M0TPB3MapGsbE* _result_4447;
    #line 976 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4447 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3717, 0ll);
    moonbit_decref(_M0L6_2atmpS3717.$0);
    return _result_4447;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1624;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3721 =
      _M0L4selfS1618->$0;
    void* _M0L7_2abindS1625;
    struct _M0TUsbE** _M0L7_2abindS1622;
    struct _M0TUsbE** _M0L6_2atmpS3720;
    struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3719;
    struct _M0TPB3MapGsbE* _result_4450;
    moonbit_incref(_M0L4dataS3721);
    #line 978 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1625
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3721, _M0L3keyS1619);
    moonbit_decref(_M0L4dataS3721);
    if (_M0L7_2abindS1625 == 0) {
      if (_M0L7_2abindS1625) {
        moonbit_decref(_M0L7_2abindS1625);
      }
      goto join_1621;
    } else {
      void* _M0L7_2aSomeS1626 = _M0L7_2abindS1625;
      void* _M0L4_2axS1627 = _M0L7_2aSomeS1626;
      switch (Moonbit_object_tag(_M0L4_2axS1627)) {
        case 3: {
          struct _M0DTP19moonbitDB10RedisValue3Set* _M0L6_2aSetS1628 =
            (struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L4_2axS1627;
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3910 = _M0L6_2aSetS1628->$0;
          int32_t _M0L6_2acntS4322 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1628));
          struct _M0TPB3MapGsbE* _M0L4_2asS1629;
          if (_M0L6_2acntS4322 > 1) {
            int32_t _M0L11_2anew__cntS4323 = _M0L6_2acntS4322 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1628), _M0L11_2anew__cntS4323);
            moonbit_incref(_M0L8_2afieldS3910);
          } else if (_M0L6_2acntS4322 == 1) {
            #line 978 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2aSetS1628);
          }
          _M0L4_2asS1629 = _M0L8_2afieldS3910;
          _M0L1sS1624 = _M0L4_2asS1629;
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
    join_1623:;
    return _M0L1sS1624;
    join_1621:;
    _M0L7_2abindS1622 = (struct _M0TUsbE**)moonbit_empty_ref_array;
    _M0L6_2atmpS3720 = _M0L7_2abindS1622;
    _M0L6_2atmpS3719
    = (struct _M0TPB9ArrayViewGUsbEE){
      .$0 = _M0L6_2atmpS3720, .$1 = 0, .$2 = 0
    };
    #line 980 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4450 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3719, 0ll);
    moonbit_decref(_M0L6_2atmpS3719.$0);
    return _result_4450;
  }
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
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3716 =
      _M0L4selfS1608->$0;
    void* _M0L7_2abindS1613;
    int32_t _result_4452;
    moonbit_incref(_M0L4dataS3716);
    #line 967 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1613
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3716, _M0L3keyS1609);
    moonbit_decref(_M0L4dataS3716);
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
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3912 = _M0L6_2aSetS1616->$0;
          int32_t _M0L6_2acntS4324 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1616));
          struct _M0TPB3MapGsbE* _M0L4_2asS1617;
          if (_M0L6_2acntS4324 > 1) {
            int32_t _M0L11_2anew__cntS4325 = _M0L6_2acntS4324 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1616), _M0L11_2anew__cntS4325);
            moonbit_incref(_M0L8_2afieldS3912);
          } else if (_M0L6_2acntS4324 == 1) {
            #line 967 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2aSetS1616);
          }
          _M0L4_2asS1617 = _M0L8_2afieldS3912;
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
    _result_4452 = _M0MPB3Map8containsGsbE(_M0L1sS1611, _M0L5valueS1612);
    moonbit_decref(_M0L1sS1611);
    return _result_4452;
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
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3715 =
      _M0L4selfS1599->$0;
    void* _M0L7_2abindS1603;
    int32_t _result_4454;
    moonbit_incref(_M0L4dataS3715);
    #line 956 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1603
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3715, _M0L3keyS1600);
    moonbit_decref(_M0L4dataS3715);
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
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3914 = _M0L6_2aSetS1606->$0;
          int32_t _M0L6_2acntS4326 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1606));
          struct _M0TPB3MapGsbE* _M0L4_2asS1607;
          if (_M0L6_2acntS4326 > 1) {
            int32_t _M0L11_2anew__cntS4327 = _M0L6_2acntS4326 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1606), _M0L11_2anew__cntS4327);
            moonbit_incref(_M0L8_2afieldS3914);
          } else if (_M0L6_2acntS4326 == 1) {
            #line 956 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2aSetS1606);
          }
          _M0L4_2asS1607 = _M0L8_2afieldS3914;
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
    _result_4454 = _M0MPB3Map6lengthGsbE(_M0L1sS1602);
    moonbit_decref(_M0L1sS1602);
    return _result_4454;
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
    moonbit_string_t* _M0L6_2atmpS3711 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4455 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4455)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_4455->$0 = _M0L6_2atmpS3711;
    _block_4455->$1 = 0;
    return _block_4455;
  } else {
    struct _M0TPB3MapGsbE* _M0L1sS1584;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3714 =
      _M0L4selfS1580->$0;
    void* _M0L7_2abindS1594;
    moonbit_string_t* _M0L6_2atmpS3713;
    struct _M0TPB5ArrayGsE* _M0L6resultS1585;
    struct _M0TPB4IterGUsbEE* _M0L5_2aitS1586;
    moonbit_string_t* _M0L6_2atmpS3712;
    struct _M0TPB5ArrayGsE* _block_4460;
    moonbit_incref(_M0L4dataS3714);
    #line 921 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1594
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3714, _M0L3keyS1581);
    moonbit_decref(_M0L4dataS3714);
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
          struct _M0TPB3MapGsbE* _M0L8_2afieldS3917 = _M0L6_2aSetS1597->$0;
          int32_t _M0L6_2acntS4330 =
            Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1597));
          struct _M0TPB3MapGsbE* _M0L4_2asS1598;
          if (_M0L6_2acntS4330 > 1) {
            int32_t _M0L11_2anew__cntS4331 = _M0L6_2acntS4330 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1597), _M0L11_2anew__cntS4331);
            moonbit_incref(_M0L8_2afieldS3917);
          } else if (_M0L6_2acntS4330 == 1) {
            #line 921 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L6_2aSetS1597);
          }
          _M0L4_2asS1598 = _M0L8_2afieldS3917;
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
    _M0L6_2atmpS3713 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1585
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1585)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _M0L6resultS1585->$0 = _M0L6_2atmpS3713;
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
        moonbit_string_t _M0L8_2afieldS3916 = _M0L4_2axS1592->$0;
        int32_t _M0L6_2acntS4328 =
          Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1592));
        moonbit_string_t _M0L4_2amS1593;
        if (_M0L6_2acntS4328 > 1) {
          int32_t _M0L11_2anew__cntS4329 = _M0L6_2acntS4328 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1592), _M0L11_2anew__cntS4329);
          moonbit_incref(_M0L8_2afieldS3916);
        } else if (_M0L6_2acntS4328 == 1) {
          #line 924 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L4_2axS1592);
        }
        _M0L4_2amS1593 = _M0L8_2afieldS3916;
        _M0L1mS1588 = _M0L4_2amS1593;
        goto join_1587;
      }
      goto joinlet_4459;
      join_1587:;
      #line 925 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1585, _M0L1mS1588);
      moonbit_decref(_M0L1mS1588);
      continue;
      joinlet_4459:;
      break;
    }
    return _M0L6resultS1585;
    join_1582:;
    _M0L6_2atmpS3712 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_4460
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4460)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_4460->$0 = _M0L6_2atmpS3712;
    _block_4460->$1 = 0;
    return _block_4460;
  }
}

int32_t _M0MP19moonbitDB8Database4sadd(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1567,
  moonbit_string_t _M0L3keyS1568,
  moonbit_string_t _M0L5valueS1579
) {
  int32_t _M0L6_2atmpS3705;
  struct _M0TPB3MapGsbE* _M0L3setS1569;
  struct _M0TPB3MapGsbE* _M0L1sS1573;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3710;
  void* _M0L7_2abindS1574;
  struct _M0TUsbE** _M0L7_2abindS1571;
  struct _M0TUsbE** _M0L6_2atmpS3709;
  struct _M0TPB9ArrayViewGUsbEE _M0L6_2atmpS3708;
  #line 902 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 903 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3705
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1567, _M0L3keyS1568);
  _M0L4dataS3710 = _M0L4selfS1567->$0;
  moonbit_incref(_M0L4dataS3710);
  #line 904 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1574
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3710, _M0L3keyS1568);
  moonbit_decref(_M0L4dataS3710);
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
        struct _M0TPB3MapGsbE* _M0L8_2afieldS3920 = _M0L6_2aSetS1577->$0;
        int32_t _M0L6_2acntS4332 =
          Moonbit_rc_count(Moonbit_object_header(_M0L6_2aSetS1577));
        struct _M0TPB3MapGsbE* _M0L4_2asS1578;
        if (_M0L6_2acntS4332 > 1) {
          int32_t _M0L11_2anew__cntS4333 = _M0L6_2acntS4332 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L6_2aSetS1577), _M0L11_2anew__cntS4333);
          moonbit_incref(_M0L8_2afieldS3920);
        } else if (_M0L6_2acntS4332 == 1) {
          #line 904 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L6_2aSetS1577);
        }
        _M0L4_2asS1578 = _M0L8_2afieldS3920;
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
  goto joinlet_4462;
  join_1572:;
  _M0L3setS1569 = _M0L1sS1573;
  joinlet_4462:;
  goto joinlet_4461;
  join_1570:;
  _M0L7_2abindS1571 = (struct _M0TUsbE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3709 = _M0L7_2abindS1571;
  _M0L6_2atmpS3708
  = (struct _M0TPB9ArrayViewGUsbEE){
    .$0 = _M0L6_2atmpS3709, .$1 = 0, .$2 = 0
  };
  #line 906 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L3setS1569 = _M0MPB3Map3MapGsbE(_M0L6_2atmpS3708, 10ll);
  moonbit_decref(_M0L6_2atmpS3708.$0);
  joinlet_4461:;
  #line 908 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (_M0MPB3Map8containsGsbE(_M0L3setS1569, _M0L5valueS1579)) {
    moonbit_decref(_M0L3setS1569);
    return 0;
  } else {
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3706;
    void* _M0L3SetS3707;
    #line 911 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsbE(_M0L3setS1569, _M0L5valueS1579, 1);
    _M0L4dataS3706 = _M0L4selfS1567->$0;
    _M0L3SetS3707
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue3Set));
    Moonbit_object_header(_M0L3SetS3707)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 16, 3);
    ((struct _M0DTP19moonbitDB10RedisValue3Set*)_M0L3SetS3707)->$0
    = _M0L3setS1569;
    moonbit_incref(_M0L4dataS3706);
    #line 912 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3706, _M0L3keyS1568, _M0L3SetS3707);
    moonbit_decref(_M0L4dataS3706);
    moonbit_decref(_M0L3SetS3707);
    return 1;
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database6lrange(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1548,
  moonbit_string_t _M0L3keyS1549,
  int32_t _M0L5startS1556,
  int32_t _M0L3endS1558
) {
  #line 720 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 721 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1548, _M0L3keyS1549)
  ) {
    moonbit_string_t* _M0L6_2atmpS3699 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4463 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4463)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_4463->$0 = _M0L6_2atmpS3699;
    _block_4463->$1 = 0;
    return _block_4463;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L5dequeS1552;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3704 =
      _M0L4selfS1548->$0;
    void* _M0L7_2abindS1562;
    struct _M0TPB5ArrayGsE* _M0L3arrS1553;
    int32_t _M0L3lenS1554;
    int32_t _M0L10start__idxS1555;
    int32_t _M0L8end__idxS1557;
    moonbit_string_t* _M0L6_2atmpS3703;
    struct _M0TPB5ArrayGsE* _M0L6resultS1559;
    int32_t _M0L1iS1560;
    moonbit_string_t* _M0L6_2atmpS3700;
    struct _M0TPB5ArrayGsE* _block_4468;
    moonbit_incref(_M0L4dataS3704);
    #line 724 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1562
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3704, _M0L3keyS1549);
    moonbit_decref(_M0L4dataS3704);
    if (_M0L7_2abindS1562 == 0) {
      if (_M0L7_2abindS1562) {
        moonbit_decref(_M0L7_2abindS1562);
      }
      goto join_1550;
    } else {
      void* _M0L7_2aSomeS1563 = _M0L7_2abindS1562;
      void* _M0L4_2axS1564 = _M0L7_2aSomeS1563;
      switch (Moonbit_object_tag(_M0L4_2axS1564)) {
        case 2: {
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1565 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1564;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS3922 =
            _M0L7_2aListS1565->$0;
          int32_t _M0L6_2acntS4334 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1565));
          struct _M0TP19moonbitDB5Deque* _M0L8_2adequeS1566;
          if (_M0L6_2acntS4334 > 1) {
            int32_t _M0L11_2anew__cntS4335 = _M0L6_2acntS4334 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1565), _M0L11_2anew__cntS4335);
            moonbit_incref(_M0L8_2afieldS3922);
          } else if (_M0L6_2acntS4334 == 1) {
            #line 724 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1565);
          }
          _M0L8_2adequeS1566 = _M0L8_2afieldS3922;
          _M0L5dequeS1552 = _M0L8_2adequeS1566;
          goto join_1551;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1564);
          goto join_1550;
          break;
        }
      }
    }
    join_1551:;
    #line 726 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3arrS1553 = _M0MP19moonbitDB5Deque9to__array(_M0L5dequeS1552);
    moonbit_decref(_M0L5dequeS1552);
    #line 727 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3lenS1554 = _M0MPC15array5Array6lengthGsE(_M0L3arrS1553);
    if (_M0L5startS1556 < 0) {
      _M0L10start__idxS1555 = _M0L3lenS1554 + _M0L5startS1556;
    } else {
      _M0L10start__idxS1555 = _M0L5startS1556;
    }
    if (_M0L3endS1558 < 0) {
      _M0L8end__idxS1557 = _M0L3lenS1554 + _M0L3endS1558;
    } else {
      _M0L8end__idxS1557 = _M0L3endS1558;
    }
    _M0L6_2atmpS3703 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1559
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1559)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _M0L6resultS1559->$0 = _M0L6_2atmpS3703;
    _M0L6resultS1559->$1 = 0;
    _M0L1iS1560 = _M0L10start__idxS1555;
    while (1) {
      int32_t _if__result_4467;
      if (_M0L1iS1560 <= _M0L8end__idxS1557) {
        if (_M0L1iS1560 >= 0) {
          _if__result_4467 = _M0L1iS1560 < _M0L3lenS1554;
        } else {
          _if__result_4467 = 0;
        }
      } else {
        _if__result_4467 = 0;
      }
      if (_if__result_4467) {
        moonbit_string_t _M0L6_2atmpS3701;
        int32_t _M0L6_2atmpS3702;
        #line 732 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3701
        = _M0MPC15array5Array2atGsE(_M0L3arrS1553, _M0L1iS1560);
        #line 732 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1559, _M0L6_2atmpS3701);
        moonbit_decref(_M0L6_2atmpS3701);
        _M0L6_2atmpS3702 = _M0L1iS1560 + 1;
        _M0L1iS1560 = _M0L6_2atmpS3702;
        continue;
      } else {
        moonbit_decref(_M0L3arrS1553);
      }
      break;
    }
    return _M0L6resultS1559;
    join_1550:;
    _M0L6_2atmpS3700 = (moonbit_string_t*)moonbit_empty_ref_array;
    _block_4468
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4468)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _block_4468->$0 = _M0L6_2atmpS3700;
    _block_4468->$1 = 0;
    return _block_4468;
  }
}

int32_t _M0MP19moonbitDB8Database4llen(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1539,
  moonbit_string_t _M0L3keyS1540
) {
  #line 709 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 710 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1539, _M0L3keyS1540)
  ) {
    return 0;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L1dS1542;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3698 =
      _M0L4selfS1539->$0;
    void* _M0L7_2abindS1543;
    int32_t _result_4470;
    moonbit_incref(_M0L4dataS3698);
    #line 713 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1543
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3698, _M0L3keyS1540);
    moonbit_decref(_M0L4dataS3698);
    if (_M0L7_2abindS1543 == 0) {
      if (_M0L7_2abindS1543) {
        moonbit_decref(_M0L7_2abindS1543);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1544 = _M0L7_2abindS1543;
      void* _M0L4_2axS1545 = _M0L7_2aSomeS1544;
      switch (Moonbit_object_tag(_M0L4_2axS1545)) {
        case 2: {
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1546 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1545;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS3924 =
            _M0L7_2aListS1546->$0;
          int32_t _M0L6_2acntS4336 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1546));
          struct _M0TP19moonbitDB5Deque* _M0L4_2adS1547;
          if (_M0L6_2acntS4336 > 1) {
            int32_t _M0L11_2anew__cntS4337 = _M0L6_2acntS4336 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1546), _M0L11_2anew__cntS4337);
            moonbit_incref(_M0L8_2afieldS3924);
          } else if (_M0L6_2acntS4336 == 1) {
            #line 713 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1546);
          }
          _M0L4_2adS1547 = _M0L8_2afieldS3924;
          _M0L1dS1542 = _M0L4_2adS1547;
          goto join_1541;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1545);
          return 0;
          break;
        }
      }
    }
    join_1541:;
    #line 714 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4470 = _M0MP19moonbitDB5Deque6length(_M0L1dS1542);
    moonbit_decref(_M0L1dS1542);
    return _result_4470;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database4rpop(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1528,
  moonbit_string_t _M0L3keyS1529
) {
  #line 694 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 695 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1528, _M0L3keyS1529)
  ) {
    return 0;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L5dequeS1532;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3697 =
      _M0L4selfS1528->$0;
    void* _M0L7_2abindS1534;
    moonbit_string_t _M0L3valS1533;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3695;
    void* _M0L4ListS3696;
    moonbit_incref(_M0L4dataS3697);
    #line 698 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1534
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3697, _M0L3keyS1529);
    moonbit_decref(_M0L4dataS3697);
    if (_M0L7_2abindS1534 == 0) {
      if (_M0L7_2abindS1534) {
        moonbit_decref(_M0L7_2abindS1534);
      }
      goto join_1530;
    } else {
      void* _M0L7_2aSomeS1535 = _M0L7_2abindS1534;
      void* _M0L4_2axS1536 = _M0L7_2aSomeS1535;
      switch (Moonbit_object_tag(_M0L4_2axS1536)) {
        case 2: {
          struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1537 =
            (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1536;
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS3927 =
            _M0L7_2aListS1537->$0;
          int32_t _M0L6_2acntS4338 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1537));
          struct _M0TP19moonbitDB5Deque* _M0L8_2adequeS1538;
          if (_M0L6_2acntS4338 > 1) {
            int32_t _M0L11_2anew__cntS4339 = _M0L6_2acntS4338 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1537), _M0L11_2anew__cntS4339);
            moonbit_incref(_M0L8_2afieldS3927);
          } else if (_M0L6_2acntS4338 == 1) {
            #line 698 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1537);
          }
          _M0L8_2adequeS1538 = _M0L8_2afieldS3927;
          _M0L5dequeS1532 = _M0L8_2adequeS1538;
          goto join_1531;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1536);
          goto join_1530;
          break;
        }
      }
    }
    join_1531:;
    #line 700 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3valS1533 = _M0MP19moonbitDB5Deque9pop__back(_M0L5dequeS1532);
    _M0L4dataS3695 = _M0L4selfS1528->$0;
    _M0L4ListS3696
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4List));
    Moonbit_object_header(_M0L4ListS3696)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 19, 2);
    ((struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4ListS3696)->$0
    = _M0L5dequeS1532;
    moonbit_incref(_M0L4dataS3695);
    #line 701 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3695, _M0L3keyS1529, _M0L4ListS3696);
    moonbit_decref(_M0L4dataS3695);
    moonbit_decref(_M0L4ListS3696);
    return _M0L3valS1533;
    join_1530:;
    return 0;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database4lpop(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1517,
  moonbit_string_t _M0L3keyS1518
) {
  #line 679 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 680 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1517, _M0L3keyS1518)
  ) {
    return 0;
  } else {
    struct _M0TP19moonbitDB5Deque* _M0L5dequeS1521;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3694 =
      _M0L4selfS1517->$0;
    void* _M0L7_2abindS1523;
    moonbit_string_t _M0L3valS1522;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3692;
    void* _M0L4ListS3693;
    moonbit_incref(_M0L4dataS3694);
    #line 683 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1523
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3694, _M0L3keyS1518);
    moonbit_decref(_M0L4dataS3694);
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
          struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS3930 =
            _M0L7_2aListS1526->$0;
          int32_t _M0L6_2acntS4340 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1526));
          struct _M0TP19moonbitDB5Deque* _M0L8_2adequeS1527;
          if (_M0L6_2acntS4340 > 1) {
            int32_t _M0L11_2anew__cntS4341 = _M0L6_2acntS4340 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1526), _M0L11_2anew__cntS4341);
            moonbit_incref(_M0L8_2afieldS3930);
          } else if (_M0L6_2acntS4340 == 1) {
            #line 683 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aListS1526);
          }
          _M0L8_2adequeS1527 = _M0L8_2afieldS3930;
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
    #line 685 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L3valS1522 = _M0MP19moonbitDB5Deque10pop__front(_M0L5dequeS1521);
    _M0L4dataS3692 = _M0L4selfS1517->$0;
    _M0L4ListS3693
    = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4List));
    Moonbit_object_header(_M0L4ListS3693)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 19, 2);
    ((struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4ListS3693)->$0
    = _M0L5dequeS1521;
    moonbit_incref(_M0L4dataS3692);
    #line 686 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3692, _M0L3keyS1518, _M0L4ListS3693);
    moonbit_decref(_M0L4dataS3692);
    moonbit_decref(_M0L4ListS3693);
    return _M0L3valS1522;
    join_1519:;
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database5rpush(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1505,
  moonbit_string_t _M0L3keyS1506,
  moonbit_string_t _M0L5valueS1516
) {
  int32_t _M0L6_2atmpS3688;
  struct _M0TP19moonbitDB5Deque* _M0L5dequeS1507;
  struct _M0TP19moonbitDB5Deque* _M0L1dS1510;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3691;
  void* _M0L7_2abindS1511;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3689;
  void* _M0L4ListS3690;
  int32_t _result_4477;
  #line 668 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 669 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3688
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1505, _M0L3keyS1506);
  _M0L4dataS3691 = _M0L4selfS1505->$0;
  moonbit_incref(_M0L4dataS3691);
  #line 670 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1511
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3691, _M0L3keyS1506);
  moonbit_decref(_M0L4dataS3691);
  if (_M0L7_2abindS1511 == 0) {
    if (_M0L7_2abindS1511) {
      moonbit_decref(_M0L7_2abindS1511);
    }
    goto join_1508;
  } else {
    void* _M0L7_2aSomeS1512 = _M0L7_2abindS1511;
    void* _M0L4_2axS1513 = _M0L7_2aSomeS1512;
    switch (Moonbit_object_tag(_M0L4_2axS1513)) {
      case 2: {
        struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1514 =
          (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1513;
        struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS3933 =
          _M0L7_2aListS1514->$0;
        int32_t _M0L6_2acntS4342 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1514));
        struct _M0TP19moonbitDB5Deque* _M0L4_2adS1515;
        if (_M0L6_2acntS4342 > 1) {
          int32_t _M0L11_2anew__cntS4343 = _M0L6_2acntS4342 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1514), _M0L11_2anew__cntS4343);
          moonbit_incref(_M0L8_2afieldS3933);
        } else if (_M0L6_2acntS4342 == 1) {
          #line 670 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aListS1514);
        }
        _M0L4_2adS1515 = _M0L8_2afieldS3933;
        _M0L1dS1510 = _M0L4_2adS1515;
        goto join_1509;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1513);
        goto join_1508;
        break;
      }
    }
  }
  goto joinlet_4476;
  join_1509:;
  _M0L5dequeS1507 = _M0L1dS1510;
  joinlet_4476:;
  goto joinlet_4475;
  join_1508:;
  #line 672 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5dequeS1507 = _M0MP19moonbitDB5Deque3new();
  joinlet_4475:;
  #line 674 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MP19moonbitDB5Deque10push__back(_M0L5dequeS1507, _M0L5valueS1516);
  _M0L4dataS3689 = _M0L4selfS1505->$0;
  moonbit_incref(_M0L5dequeS1507);
  _M0L4ListS3690
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4List));
  Moonbit_object_header(_M0L4ListS3690)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 19, 2);
  ((struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4ListS3690)->$0
  = _M0L5dequeS1507;
  moonbit_incref(_M0L4dataS3689);
  #line 675 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3689, _M0L3keyS1506, _M0L4ListS3690);
  moonbit_decref(_M0L4dataS3689);
  moonbit_decref(_M0L4ListS3690);
  #line 676 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_4477 = _M0MP19moonbitDB5Deque6length(_M0L5dequeS1507);
  moonbit_decref(_M0L5dequeS1507);
  return _result_4477;
}

int32_t _M0MP19moonbitDB8Database5lpush(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1493,
  moonbit_string_t _M0L3keyS1494,
  moonbit_string_t _M0L5valueS1504
) {
  int32_t _M0L6_2atmpS3684;
  struct _M0TP19moonbitDB5Deque* _M0L5dequeS1495;
  struct _M0TP19moonbitDB5Deque* _M0L1dS1498;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3687;
  void* _M0L7_2abindS1499;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3685;
  void* _M0L4ListS3686;
  int32_t _result_4480;
  #line 657 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 658 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3684
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1493, _M0L3keyS1494);
  _M0L4dataS3687 = _M0L4selfS1493->$0;
  moonbit_incref(_M0L4dataS3687);
  #line 659 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1499
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3687, _M0L3keyS1494);
  moonbit_decref(_M0L4dataS3687);
  if (_M0L7_2abindS1499 == 0) {
    if (_M0L7_2abindS1499) {
      moonbit_decref(_M0L7_2abindS1499);
    }
    goto join_1496;
  } else {
    void* _M0L7_2aSomeS1500 = _M0L7_2abindS1499;
    void* _M0L4_2axS1501 = _M0L7_2aSomeS1500;
    switch (Moonbit_object_tag(_M0L4_2axS1501)) {
      case 2: {
        struct _M0DTP19moonbitDB10RedisValue4List* _M0L7_2aListS1502 =
          (struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4_2axS1501;
        struct _M0TP19moonbitDB5Deque* _M0L8_2afieldS3936 =
          _M0L7_2aListS1502->$0;
        int32_t _M0L6_2acntS4344 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aListS1502));
        struct _M0TP19moonbitDB5Deque* _M0L4_2adS1503;
        if (_M0L6_2acntS4344 > 1) {
          int32_t _M0L11_2anew__cntS4345 = _M0L6_2acntS4344 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aListS1502), _M0L11_2anew__cntS4345);
          moonbit_incref(_M0L8_2afieldS3936);
        } else if (_M0L6_2acntS4344 == 1) {
          #line 659 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aListS1502);
        }
        _M0L4_2adS1503 = _M0L8_2afieldS3936;
        _M0L1dS1498 = _M0L4_2adS1503;
        goto join_1497;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1501);
        goto join_1496;
        break;
      }
    }
  }
  goto joinlet_4479;
  join_1497:;
  _M0L5dequeS1495 = _M0L1dS1498;
  joinlet_4479:;
  goto joinlet_4478;
  join_1496:;
  #line 661 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5dequeS1495 = _M0MP19moonbitDB5Deque3new();
  joinlet_4478:;
  #line 663 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MP19moonbitDB5Deque11push__front(_M0L5dequeS1495, _M0L5valueS1504);
  _M0L4dataS3685 = _M0L4selfS1493->$0;
  moonbit_incref(_M0L5dequeS1495);
  _M0L4ListS3686
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4List));
  Moonbit_object_header(_M0L4ListS3686)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 19, 2);
  ((struct _M0DTP19moonbitDB10RedisValue4List*)_M0L4ListS3686)->$0
  = _M0L5dequeS1495;
  moonbit_incref(_M0L4dataS3685);
  #line 664 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3685, _M0L3keyS1494, _M0L4ListS3686);
  moonbit_decref(_M0L4dataS3685);
  moonbit_decref(_M0L4ListS3686);
  #line 665 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_4480 = _M0MP19moonbitDB5Deque6length(_M0L5dequeS1495);
  moonbit_decref(_M0L5dequeS1495);
  return _result_4480;
}

int32_t _M0MP19moonbitDB8Database4hlen(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1484,
  moonbit_string_t _M0L3keyS1485
) {
  #line 646 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 647 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1484, _M0L3keyS1485)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1487;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3683 =
      _M0L4selfS1484->$0;
    void* _M0L7_2abindS1488;
    int32_t _result_4482;
    moonbit_incref(_M0L4dataS3683);
    #line 650 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1488
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3683, _M0L3keyS1485);
    moonbit_decref(_M0L4dataS3683);
    if (_M0L7_2abindS1488 == 0) {
      if (_M0L7_2abindS1488) {
        moonbit_decref(_M0L7_2abindS1488);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1489 = _M0L7_2abindS1488;
      void* _M0L4_2axS1490 = _M0L7_2aSomeS1489;
      switch (Moonbit_object_tag(_M0L4_2axS1490)) {
        case 1: {
          struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1491 =
            (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1490;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3938 = _M0L7_2aHashS1491->$0;
          int32_t _M0L6_2acntS4346 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1491));
          struct _M0TPB3MapGssE* _M0L4_2ahS1492;
          if (_M0L6_2acntS4346 > 1) {
            int32_t _M0L11_2anew__cntS4347 = _M0L6_2acntS4346 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1491), _M0L11_2anew__cntS4347);
            moonbit_incref(_M0L8_2afieldS3938);
          } else if (_M0L6_2acntS4346 == 1) {
            #line 650 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aHashS1491);
          }
          _M0L4_2ahS1492 = _M0L8_2afieldS3938;
          _M0L1hS1487 = _M0L4_2ahS1492;
          goto join_1486;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1490);
          return 0;
          break;
        }
      }
    }
    join_1486:;
    #line 651 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4482 = _M0MPB3Map6lengthGssE(_M0L1hS1487);
    moonbit_decref(_M0L1hS1487);
    return _result_4482;
  }
}

struct _M0TPB3MapGssE* _M0MP19moonbitDB8Database7hgetall(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1472,
  moonbit_string_t _M0L3keyS1473
) {
  #line 635 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 636 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1472, _M0L3keyS1473)
  ) {
    struct _M0TUssE** _M0L7_2abindS1474 =
      (struct _M0TUssE**)moonbit_empty_ref_array;
    struct _M0TUssE** _M0L6_2atmpS3679 = _M0L7_2abindS1474;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3678 =
      (struct _M0TPB9ArrayViewGUssEE){.$0 = _M0L6_2atmpS3679,
                                        .$1 = 0,
                                        .$2 = 0};
    struct _M0TPB3MapGssE* _result_4483;
    #line 637 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4483 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3678, 0ll);
    moonbit_decref(_M0L6_2atmpS3678.$0);
    return _result_4483;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1478;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3682 =
      _M0L4selfS1472->$0;
    void* _M0L7_2abindS1479;
    struct _M0TUssE** _M0L7_2abindS1476;
    struct _M0TUssE** _M0L6_2atmpS3681;
    struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3680;
    struct _M0TPB3MapGssE* _result_4486;
    moonbit_incref(_M0L4dataS3682);
    #line 639 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1479
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3682, _M0L3keyS1473);
    moonbit_decref(_M0L4dataS3682);
    if (_M0L7_2abindS1479 == 0) {
      if (_M0L7_2abindS1479) {
        moonbit_decref(_M0L7_2abindS1479);
      }
      goto join_1475;
    } else {
      void* _M0L7_2aSomeS1480 = _M0L7_2abindS1479;
      void* _M0L4_2axS1481 = _M0L7_2aSomeS1480;
      switch (Moonbit_object_tag(_M0L4_2axS1481)) {
        case 1: {
          struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1482 =
            (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1481;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3940 = _M0L7_2aHashS1482->$0;
          int32_t _M0L6_2acntS4348 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1482));
          struct _M0TPB3MapGssE* _M0L4_2ahS1483;
          if (_M0L6_2acntS4348 > 1) {
            int32_t _M0L11_2anew__cntS4349 = _M0L6_2acntS4348 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1482), _M0L11_2anew__cntS4349);
            moonbit_incref(_M0L8_2afieldS3940);
          } else if (_M0L6_2acntS4348 == 1) {
            #line 639 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aHashS1482);
          }
          _M0L4_2ahS1483 = _M0L8_2afieldS3940;
          _M0L1hS1478 = _M0L4_2ahS1483;
          goto join_1477;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1481);
          goto join_1475;
          break;
        }
      }
    }
    join_1477:;
    return _M0L1hS1478;
    join_1475:;
    _M0L7_2abindS1476 = (struct _M0TUssE**)moonbit_empty_ref_array;
    _M0L6_2atmpS3681 = _M0L7_2abindS1476;
    _M0L6_2atmpS3680
    = (struct _M0TPB9ArrayViewGUssEE){
      .$0 = _M0L6_2atmpS3681, .$1 = 0, .$2 = 0
    };
    #line 641 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4486 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3680, 0ll);
    moonbit_decref(_M0L6_2atmpS3680.$0);
    return _result_4486;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database4hget(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1461,
  moonbit_string_t _M0L3keyS1462,
  moonbit_string_t _M0L5fieldS1466
) {
  #line 606 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 607 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1461, _M0L3keyS1462)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGssE* _M0L1hS1465;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3677 =
      _M0L4selfS1461->$0;
    void* _M0L7_2abindS1467;
    moonbit_string_t _result_4489;
    moonbit_incref(_M0L4dataS3677);
    #line 610 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1467
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3677, _M0L3keyS1462);
    moonbit_decref(_M0L4dataS3677);
    if (_M0L7_2abindS1467 == 0) {
      if (_M0L7_2abindS1467) {
        moonbit_decref(_M0L7_2abindS1467);
      }
      goto join_1463;
    } else {
      void* _M0L7_2aSomeS1468 = _M0L7_2abindS1467;
      void* _M0L4_2axS1469 = _M0L7_2aSomeS1468;
      switch (Moonbit_object_tag(_M0L4_2axS1469)) {
        case 1: {
          struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1470 =
            (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1469;
          struct _M0TPB3MapGssE* _M0L8_2afieldS3942 = _M0L7_2aHashS1470->$0;
          int32_t _M0L6_2acntS4350 =
            Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1470));
          struct _M0TPB3MapGssE* _M0L4_2ahS1471;
          if (_M0L6_2acntS4350 > 1) {
            int32_t _M0L11_2anew__cntS4351 = _M0L6_2acntS4350 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1470), _M0L11_2anew__cntS4351);
            moonbit_incref(_M0L8_2afieldS3942);
          } else if (_M0L6_2acntS4350 == 1) {
            #line 610 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L7_2aHashS1470);
          }
          _M0L4_2ahS1471 = _M0L8_2afieldS3942;
          _M0L1hS1465 = _M0L4_2ahS1471;
          goto join_1464;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1469);
          goto join_1463;
          break;
        }
      }
    }
    join_1464:;
    #line 611 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4489 = _M0MPB3Map3getGssE(_M0L1hS1465, _M0L5fieldS1466);
    moonbit_decref(_M0L1hS1465);
    return _result_4489;
    join_1463:;
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database4hset(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1447,
  moonbit_string_t _M0L3keyS1448,
  moonbit_string_t _M0L5fieldS1459,
  moonbit_string_t _M0L5valueS1460
) {
  int32_t _M0L6_2atmpS3671;
  struct _M0TPB3MapGssE* _M0L4hashS1449;
  struct _M0TPB3MapGssE* _M0L1hS1453;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3676;
  void* _M0L7_2abindS1454;
  struct _M0TUssE** _M0L7_2abindS1451;
  struct _M0TUssE** _M0L6_2atmpS3675;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS3674;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3672;
  void* _M0L4HashS3673;
  #line 596 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 597 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3671
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1447, _M0L3keyS1448);
  _M0L4dataS3676 = _M0L4selfS1447->$0;
  moonbit_incref(_M0L4dataS3676);
  #line 598 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1454
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3676, _M0L3keyS1448);
  moonbit_decref(_M0L4dataS3676);
  if (_M0L7_2abindS1454 == 0) {
    if (_M0L7_2abindS1454) {
      moonbit_decref(_M0L7_2abindS1454);
    }
    goto join_1450;
  } else {
    void* _M0L7_2aSomeS1455 = _M0L7_2abindS1454;
    void* _M0L4_2axS1456 = _M0L7_2aSomeS1455;
    switch (Moonbit_object_tag(_M0L4_2axS1456)) {
      case 1: {
        struct _M0DTP19moonbitDB10RedisValue4Hash* _M0L7_2aHashS1457 =
          (struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4_2axS1456;
        struct _M0TPB3MapGssE* _M0L8_2afieldS3945 = _M0L7_2aHashS1457->$0;
        int32_t _M0L6_2acntS4352 =
          Moonbit_rc_count(Moonbit_object_header(_M0L7_2aHashS1457));
        struct _M0TPB3MapGssE* _M0L4_2ahS1458;
        if (_M0L6_2acntS4352 > 1) {
          int32_t _M0L11_2anew__cntS4353 = _M0L6_2acntS4352 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L7_2aHashS1457), _M0L11_2anew__cntS4353);
          moonbit_incref(_M0L8_2afieldS3945);
        } else if (_M0L6_2acntS4352 == 1) {
          #line 598 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L7_2aHashS1457);
        }
        _M0L4_2ahS1458 = _M0L8_2afieldS3945;
        _M0L1hS1453 = _M0L4_2ahS1458;
        goto join_1452;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1456);
        goto join_1450;
        break;
      }
    }
  }
  goto joinlet_4491;
  join_1452:;
  _M0L4hashS1449 = _M0L1hS1453;
  joinlet_4491:;
  goto joinlet_4490;
  join_1450:;
  _M0L7_2abindS1451 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3675 = _M0L7_2abindS1451;
  _M0L6_2atmpS3674
  = (struct _M0TPB9ArrayViewGUssEE){
    .$0 = _M0L6_2atmpS3675, .$1 = 0, .$2 = 0
  };
  #line 600 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4hashS1449 = _M0MPB3Map3MapGssE(_M0L6_2atmpS3674, 10ll);
  moonbit_decref(_M0L6_2atmpS3674.$0);
  joinlet_4490:;
  #line 602 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGssE(_M0L4hashS1449, _M0L5fieldS1459, _M0L5valueS1460);
  _M0L4dataS3672 = _M0L4selfS1447->$0;
  _M0L4HashS3673
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue4Hash));
  Moonbit_object_header(_M0L4HashS3673)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 22, 1);
  ((struct _M0DTP19moonbitDB10RedisValue4Hash*)_M0L4HashS3673)->$0
  = _M0L4hashS1449;
  moonbit_incref(_M0L4dataS3672);
  #line 603 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3672, _M0L3keyS1448, _M0L4HashS3673);
  moonbit_decref(_M0L4dataS3672);
  moonbit_decref(_M0L4HashS3673);
  return 0;
}

int64_t _M0MP19moonbitDB8Database4decr(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1431,
  moonbit_string_t _M0L3keyS1432
) {
  int32_t _M0L6_2atmpS3664;
  moonbit_string_t _M0L1sS1435;
  moonbit_string_t _M0L7currentS1433;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3670;
  void* _M0L7_2abindS1436;
  int32_t _M0L1nS1442;
  int64_t _M0L7_2abindS1444;
  int32_t _M0L6_2atmpS3669;
  moonbit_string_t _M0L8new__valS1443;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3665;
  void* _M0L6StringS3666;
  struct _M0TPB3MapGsiE* _M0L7expiresS3667;
  int32_t _M0L6_2atmpS3668;
  #line 579 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 580 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3664
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1431, _M0L3keyS1432);
  _M0L4dataS3670 = _M0L4selfS1431->$0;
  moonbit_incref(_M0L4dataS3670);
  #line 581 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1436
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3670, _M0L3keyS1432);
  moonbit_decref(_M0L4dataS3670);
  if (_M0L7_2abindS1436 == 0) {
    if (_M0L7_2abindS1436) {
      moonbit_decref(_M0L7_2abindS1436);
    }
    _M0L7currentS1433 = (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    void* _M0L7_2aSomeS1437 = _M0L7_2abindS1436;
    void* _M0L4_2axS1438 = _M0L7_2aSomeS1437;
    switch (Moonbit_object_tag(_M0L4_2axS1438)) {
      case 0: {
        struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1439 =
          (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1438;
        moonbit_string_t _M0L8_2afieldS3949 = _M0L9_2aStringS1439->$0;
        int32_t _M0L6_2acntS4354 =
          Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1439));
        moonbit_string_t _M0L4_2asS1440;
        if (_M0L6_2acntS4354 > 1) {
          int32_t _M0L11_2anew__cntS4355 = _M0L6_2acntS4354 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1439), _M0L11_2anew__cntS4355);
          moonbit_incref(_M0L8_2afieldS3949);
        } else if (_M0L6_2acntS4354 == 1) {
          #line 581 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L9_2aStringS1439);
        }
        _M0L4_2asS1440 = _M0L8_2afieldS3949;
        _M0L1sS1435 = _M0L4_2asS1440;
        goto join_1434;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1438);
        _M0L7currentS1433 = (moonbit_string_t)moonbit_string_literal_0.data;
        break;
      }
    }
  }
  goto joinlet_4492;
  join_1434:;
  _M0L7currentS1433 = _M0L1sS1435;
  joinlet_4492:;
  #line 585 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1444 = _M0FP19moonbitDB10parse__int(_M0L7currentS1433);
  moonbit_decref(_M0L7currentS1433);
  if (_M0L7_2abindS1444 == 4294967296ll) {
    return 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS1445 = _M0L7_2abindS1444;
    int32_t _M0L4_2anS1446 = (int32_t)_M0L7_2aSomeS1445;
    _M0L1nS1442 = _M0L4_2anS1446;
    goto join_1441;
  }
  join_1441:;
  _M0L6_2atmpS3669 = _M0L1nS1442 - 1;
  #line 587 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L8new__valS1443 = _M0FP19moonbitDB15int__to__string(_M0L6_2atmpS3669);
  _M0L4dataS3665 = _M0L4selfS1431->$0;
  _M0L6StringS3666
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3666)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 25, 0);
  ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS3666)->$0
  = _M0L8new__valS1443;
  moonbit_incref(_M0L4dataS3665);
  #line 588 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3665, _M0L3keyS1432, _M0L6StringS3666);
  moonbit_decref(_M0L4dataS3665);
  moonbit_decref(_M0L6StringS3666);
  _M0L7expiresS3667 = _M0L4selfS1431->$1;
  moonbit_incref(_M0L7expiresS3667);
  #line 589 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3667, _M0L3keyS1432);
  moonbit_decref(_M0L7expiresS3667);
  _M0L6_2atmpS3668 = _M0L1nS1442 - 1;
  return (int64_t)_M0L6_2atmpS3668;
}

int64_t _M0MP19moonbitDB8Database4incr(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1415,
  moonbit_string_t _M0L3keyS1416
) {
  int32_t _M0L6_2atmpS3657;
  moonbit_string_t _M0L1sS1419;
  moonbit_string_t _M0L7currentS1417;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3663;
  void* _M0L7_2abindS1420;
  int32_t _M0L1nS1426;
  int64_t _M0L7_2abindS1428;
  int32_t _M0L6_2atmpS3662;
  moonbit_string_t _M0L8new__valS1427;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3658;
  void* _M0L6StringS3659;
  struct _M0TPB3MapGsiE* _M0L7expiresS3660;
  int32_t _M0L6_2atmpS3661;
  #line 562 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 563 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3657
  = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1415, _M0L3keyS1416);
  _M0L4dataS3663 = _M0L4selfS1415->$0;
  moonbit_incref(_M0L4dataS3663);
  #line 564 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1420
  = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3663, _M0L3keyS1416);
  moonbit_decref(_M0L4dataS3663);
  if (_M0L7_2abindS1420 == 0) {
    if (_M0L7_2abindS1420) {
      moonbit_decref(_M0L7_2abindS1420);
    }
    _M0L7currentS1417 = (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    void* _M0L7_2aSomeS1421 = _M0L7_2abindS1420;
    void* _M0L4_2axS1422 = _M0L7_2aSomeS1421;
    switch (Moonbit_object_tag(_M0L4_2axS1422)) {
      case 0: {
        struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1423 =
          (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1422;
        moonbit_string_t _M0L8_2afieldS3953 = _M0L9_2aStringS1423->$0;
        int32_t _M0L6_2acntS4356 =
          Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1423));
        moonbit_string_t _M0L4_2asS1424;
        if (_M0L6_2acntS4356 > 1) {
          int32_t _M0L11_2anew__cntS4357 = _M0L6_2acntS4356 - 1;
          Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1423), _M0L11_2anew__cntS4357);
          moonbit_incref(_M0L8_2afieldS3953);
        } else if (_M0L6_2acntS4356 == 1) {
          #line 564 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_free(_M0L9_2aStringS1423);
        }
        _M0L4_2asS1424 = _M0L8_2afieldS3953;
        _M0L1sS1419 = _M0L4_2asS1424;
        goto join_1418;
        break;
      }
      default: {
        moonbit_decref(_M0L4_2axS1422);
        _M0L7currentS1417 = (moonbit_string_t)moonbit_string_literal_0.data;
        break;
      }
    }
  }
  goto joinlet_4494;
  join_1418:;
  _M0L7currentS1417 = _M0L1sS1419;
  joinlet_4494:;
  #line 568 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1428 = _M0FP19moonbitDB10parse__int(_M0L7currentS1417);
  moonbit_decref(_M0L7currentS1417);
  if (_M0L7_2abindS1428 == 4294967296ll) {
    return 4294967296ll;
  } else {
    int64_t _M0L7_2aSomeS1429 = _M0L7_2abindS1428;
    int32_t _M0L4_2anS1430 = (int32_t)_M0L7_2aSomeS1429;
    _M0L1nS1426 = _M0L4_2anS1430;
    goto join_1425;
  }
  join_1425:;
  _M0L6_2atmpS3662 = _M0L1nS1426 + 1;
  #line 570 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L8new__valS1427 = _M0FP19moonbitDB15int__to__string(_M0L6_2atmpS3662);
  _M0L4dataS3658 = _M0L4selfS1415->$0;
  _M0L6StringS3659
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3659)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 25, 0);
  ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS3659)->$0
  = _M0L8new__valS1427;
  moonbit_incref(_M0L4dataS3658);
  #line 571 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3658, _M0L3keyS1416, _M0L6StringS3659);
  moonbit_decref(_M0L4dataS3658);
  moonbit_decref(_M0L6StringS3659);
  _M0L7expiresS3660 = _M0L4selfS1415->$1;
  moonbit_incref(_M0L7expiresS3660);
  #line 572 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3660, _M0L3keyS1416);
  moonbit_decref(_M0L7expiresS3660);
  _M0L6_2atmpS3661 = _M0L1nS1426 + 1;
  return (int64_t)_M0L6_2atmpS3661;
}

moonbit_string_t _M0FP19moonbitDB15int__to__string(int32_t _M0L1nS1406) {
  #line 526 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (_M0L1nS1406 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    struct _M0TPB8MutLocalGiE* _M0L3numS1407 =
      (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    moonbit_string_t* _M0L6_2atmpS3656;
    struct _M0TPB5ArrayGsE* _M0L5charsS1408;
    int32_t _M0L3valS3642;
    moonbit_string_t* _M0L6_2atmpS3655;
    struct _M0TPB5ArrayGsE* _M0L6resultS1411;
    int32_t _M0L6_2atmpS3652;
    int32_t _M0L6_2atmpS3651;
    int32_t _M0L1iS1412;
    moonbit_string_t _M0L7_2abindS1414;
    int32_t _M0L6_2atmpS3654;
    struct _M0TPC16string10StringView _M0L6_2atmpS3653;
    moonbit_string_t _result_4498;
    Moonbit_object_header(_M0L3numS1407)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L3numS1407->$0 = _M0L1nS1406;
    _M0L6_2atmpS3656 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L5charsS1408
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L5charsS1408)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _M0L5charsS1408->$0 = _M0L6_2atmpS3656;
    _M0L5charsS1408->$1 = 0;
    _M0L3valS3642 = _M0L3numS1407->$0;
    if (_M0L3valS3642 < 0) {
      int32_t _M0L3valS3644 = _M0L3numS1407->$0;
      int32_t _M0L6_2atmpS3643 = -_M0L3valS3644;
      _M0L3numS1407->$0 = _M0L6_2atmpS3643;
    }
    while (1) {
      int32_t _M0L3valS3645 = _M0L3numS1407->$0;
      if (_M0L3valS3645 > 0) {
        int32_t _M0L3valS3646 = _M0L3numS1407->$0;
        int32_t _M0L7_2abindS1409 = _M0L3valS3646 % 10;
        int32_t _M0L3valS3648;
        int32_t _M0L6_2atmpS3647;
        switch (_M0L7_2abindS1409) {
          case 0: {
            #line 537 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1408, (moonbit_string_t)moonbit_string_literal_0.data);
            break;
          }
          
          case 1: {
            #line 538 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1408, (moonbit_string_t)moonbit_string_literal_86.data);
            break;
          }
          
          case 2: {
            #line 539 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1408, (moonbit_string_t)moonbit_string_literal_87.data);
            break;
          }
          
          case 3: {
            #line 540 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1408, (moonbit_string_t)moonbit_string_literal_88.data);
            break;
          }
          
          case 4: {
            #line 541 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1408, (moonbit_string_t)moonbit_string_literal_89.data);
            break;
          }
          
          case 5: {
            #line 542 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1408, (moonbit_string_t)moonbit_string_literal_90.data);
            break;
          }
          
          case 6: {
            #line 543 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1408, (moonbit_string_t)moonbit_string_literal_91.data);
            break;
          }
          
          case 7: {
            #line 544 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1408, (moonbit_string_t)moonbit_string_literal_92.data);
            break;
          }
          
          case 8: {
            #line 545 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1408, (moonbit_string_t)moonbit_string_literal_93.data);
            break;
          }
          
          case 9: {
            #line 546 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1408, (moonbit_string_t)moonbit_string_literal_94.data);
            break;
          }
          default: {
            #line 547 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            _M0MPC15array5Array4pushGsE(_M0L5charsS1408, (moonbit_string_t)moonbit_string_literal_0.data);
            break;
          }
        }
        _M0L3valS3648 = _M0L3numS1407->$0;
        _M0L6_2atmpS3647 = _M0L3valS3648 / 10;
        _M0L3numS1407->$0 = _M0L6_2atmpS3647;
        continue;
      } else {
        moonbit_decref(_M0L3numS1407);
      }
      break;
    }
    if (_M0L1nS1406 < 0) {
      #line 552 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L5charsS1408, (moonbit_string_t)moonbit_string_literal_95.data);
    }
    _M0L6_2atmpS3655 = (moonbit_string_t*)moonbit_empty_ref_array;
    _M0L6resultS1411
    = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_M0L6resultS1411)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
    _M0L6resultS1411->$0 = _M0L6_2atmpS3655;
    _M0L6resultS1411->$1 = 0;
    #line 555 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3652 = _M0MPC15array5Array6lengthGsE(_M0L5charsS1408);
    _M0L6_2atmpS3651 = _M0L6_2atmpS3652 - 1;
    _M0L1iS1412 = _M0L6_2atmpS3651;
    while (1) {
      if (_M0L1iS1412 >= 0) {
        moonbit_string_t _M0L6_2atmpS3649;
        int32_t _M0L6_2atmpS3650;
        #line 556 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L6_2atmpS3649
        = _M0MPC15array5Array2atGsE(_M0L5charsS1408, _M0L1iS1412);
        #line 556 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L6resultS1411, _M0L6_2atmpS3649);
        moonbit_decref(_M0L6_2atmpS3649);
        _M0L6_2atmpS3650 = _M0L1iS1412 - 1;
        _M0L1iS1412 = _M0L6_2atmpS3650;
        continue;
      } else {
        moonbit_decref(_M0L5charsS1408);
      }
      break;
    }
    _M0L7_2abindS1414 = (moonbit_string_t)moonbit_string_literal_96.data;
    _M0L6_2atmpS3654 = Moonbit_array_length(_M0L7_2abindS1414);
    _M0L6_2atmpS3653
    = (struct _M0TPC16string10StringView){
      .$0 = _M0L7_2abindS1414, .$1 = 0, .$2 = _M0L6_2atmpS3654
    };
    #line 558 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4498
    = _M0MPC15array5Array4joinGsE(_M0L6resultS1411, _M0L6_2atmpS3653);
    moonbit_decref(_M0L6resultS1411);
    moonbit_decref(_M0L6_2atmpS3653.$0);
    return _result_4498;
  }
}

int64_t _M0FP19moonbitDB10parse__int(moonbit_string_t _M0L1sS1395) {
  int32_t _M0L6_2atmpS3629;
  #line 503 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3629 = Moonbit_array_length(_M0L1sS1395);
  if (_M0L6_2atmpS3629 == 0) {
    return 4294967296ll;
  } else {
    struct _M0TPB8MutLocalGiE* _M0L6resultS1396 =
      (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    struct _M0TPB8MutLocalGiE* _M0L4signS1397;
    struct _M0TPB8MutLocalGiE* _M0L5startS1398;
    int32_t _M0L6_2atmpS3630;
    int32_t _M0L3valS3638;
    int32_t _M0L1iS1399;
    int32_t _M0L3valS3640;
    int32_t _M0L3valS3641;
    int32_t _M0L6_2atmpS3639;
    Moonbit_object_header(_M0L6resultS1396)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L6resultS1396->$0 = 0;
    _M0L4signS1397
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L4signS1397)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L4signS1397->$0 = 1;
    _M0L5startS1398
    = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
    Moonbit_object_header(_M0L5startS1398)->meta
    = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
    _M0L5startS1398->$0 = 0;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1395)) {
      #line 510 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3630 = _M0L1sS1395[0];
    if (_M0L6_2atmpS3630 == 45) {
      _M0L4signS1397->$0 = -1;
      _M0L5startS1398->$0 = 1;
    } else {
      int32_t _M0L6_2atmpS3631;
      if (0 < 0 || 0 >= Moonbit_array_length(_M0L1sS1395)) {
        #line 513 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3631 = _M0L1sS1395[0];
      if (_M0L6_2atmpS3631 == 43) {
        _M0L5startS1398->$0 = 1;
      }
    }
    _M0L3valS3638 = _M0L5startS1398->$0;
    moonbit_decref(_M0L5startS1398);
    _M0L1iS1399 = _M0L3valS3638;
    while (1) {
      int32_t _M0L6_2atmpS3632 = Moonbit_array_length(_M0L1sS1395);
      if (_M0L1iS1399 < _M0L6_2atmpS3632) {
        int32_t _M0L5digitS1401;
        int32_t _M0L6_2atmpS3636;
        int64_t _M0L7_2abindS1402;
        int32_t _M0L3valS3635;
        int32_t _M0L6_2atmpS3634;
        int32_t _M0L6_2atmpS3633;
        int32_t _M0L6_2atmpS3637;
        if (
          _M0L1iS1399 < 0 || _M0L1iS1399 >= Moonbit_array_length(_M0L1sS1395)
        ) {
          #line 517 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3636 = _M0L1sS1395[_M0L1iS1399];
        #line 517 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L7_2abindS1402
        = _M0FP19moonbitDB17uint16__to__digit(_M0L6_2atmpS3636);
        if (_M0L7_2abindS1402 == 4294967296ll) {
          moonbit_decref(_M0L4signS1397);
          moonbit_decref(_M0L6resultS1396);
          return 4294967296ll;
        } else {
          int64_t _M0L7_2aSomeS1403 = _M0L7_2abindS1402;
          int32_t _M0L8_2adigitS1404 = (int32_t)_M0L7_2aSomeS1403;
          _M0L5digitS1401 = _M0L8_2adigitS1404;
          goto join_1400;
        }
        goto joinlet_4500;
        join_1400:;
        _M0L3valS3635 = _M0L6resultS1396->$0;
        _M0L6_2atmpS3634 = _M0L3valS3635 * 10;
        _M0L6_2atmpS3633 = _M0L6_2atmpS3634 + _M0L5digitS1401;
        _M0L6resultS1396->$0 = _M0L6_2atmpS3633;
        joinlet_4500:;
        _M0L6_2atmpS3637 = _M0L1iS1399 + 1;
        _M0L1iS1399 = _M0L6_2atmpS3637;
        continue;
      }
      break;
    }
    _M0L3valS3640 = _M0L6resultS1396->$0;
    moonbit_decref(_M0L6resultS1396);
    _M0L3valS3641 = _M0L4signS1397->$0;
    moonbit_decref(_M0L4signS1397);
    _M0L6_2atmpS3639 = _M0L3valS3640 * _M0L3valS3641;
    return (int64_t)_M0L6_2atmpS3639;
  }
}

int64_t _M0FP19moonbitDB17uint16__to__digit(int32_t _M0L1cS1394) {
  #line 471 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  switch (_M0L1cS1394) {
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
  struct _M0TP19moonbitDB8Database* _M0L4selfS1385,
  moonbit_string_t _M0L3keyS1386
) {
  #line 460 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 461 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1385, _M0L3keyS1386)
  ) {
    return 0;
  } else {
    moonbit_string_t _M0L1sS1388;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3628 =
      _M0L4selfS1385->$0;
    void* _M0L7_2abindS1389;
    int32_t _result_4502;
    moonbit_incref(_M0L4dataS3628);
    #line 464 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1389
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3628, _M0L3keyS1386);
    moonbit_decref(_M0L4dataS3628);
    if (_M0L7_2abindS1389 == 0) {
      if (_M0L7_2abindS1389) {
        moonbit_decref(_M0L7_2abindS1389);
      }
      return 0;
    } else {
      void* _M0L7_2aSomeS1390 = _M0L7_2abindS1389;
      void* _M0L4_2axS1391 = _M0L7_2aSomeS1390;
      switch (Moonbit_object_tag(_M0L4_2axS1391)) {
        case 0: {
          struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1392 =
            (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1391;
          moonbit_string_t _M0L8_2afieldS3955 = _M0L9_2aStringS1392->$0;
          int32_t _M0L6_2acntS4358 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1392));
          moonbit_string_t _M0L4_2asS1393;
          if (_M0L6_2acntS4358 > 1) {
            int32_t _M0L11_2anew__cntS4359 = _M0L6_2acntS4358 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1392), _M0L11_2anew__cntS4359);
            moonbit_incref(_M0L8_2afieldS3955);
          } else if (_M0L6_2acntS4358 == 1) {
            #line 464 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L9_2aStringS1392);
          }
          _M0L4_2asS1393 = _M0L8_2afieldS3955;
          _M0L1sS1388 = _M0L4_2asS1393;
          goto join_1387;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1391);
          return 0;
          break;
        }
      }
    }
    join_1387:;
    _result_4502 = Moonbit_array_length(_M0L1sS1388);
    moonbit_decref(_M0L1sS1388);
    return _result_4502;
  }
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB8Database4keys(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1377
) {
  moonbit_string_t* _M0L6_2atmpS3627;
  struct _M0TPB5ArrayGsE* _M0L6resultS1375;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3626;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L5_2aitS1376;
  #line 377 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3627 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1375
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1375)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6resultS1375->$0 = _M0L6_2atmpS3627;
  _M0L6resultS1375->$1 = 0;
  _M0L4dataS3626 = _M0L4selfS1377->$0;
  moonbit_incref(_M0L4dataS3626);
  #line 378 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5_2aitS1376
  = _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(_M0L4dataS3626);
  moonbit_decref(_M0L4dataS3626);
  while (1) {
    moonbit_string_t _M0L3keyS1379;
    struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2abindS1381;
    int32_t _M0L6_2atmpS3625;
    #line 379 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1381
    = _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(_M0L5_2aitS1376);
    if (_M0L7_2abindS1381 == 0) {
      if (_M0L7_2abindS1381) {
        moonbit_decref(_M0L7_2abindS1381);
      }
      moonbit_decref(_M0L5_2aitS1376);
    } else {
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1382 =
        _M0L7_2abindS1381;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L4_2axS1383 =
        _M0L7_2aSomeS1382;
      moonbit_string_t _M0L8_2afieldS3957 = _M0L4_2axS1383->$0;
      int32_t _M0L6_2acntS4360 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1383));
      moonbit_string_t _M0L6_2akeyS1384;
      if (_M0L6_2acntS4360 > 1) {
        int32_t _M0L11_2anew__cntS4362 = _M0L6_2acntS4360 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1383), _M0L11_2anew__cntS4362);
        moonbit_incref(_M0L8_2afieldS3957);
      } else if (_M0L6_2acntS4360 == 1) {
        void* _M0L8_2afieldS4361 = _M0L4_2axS1383->$1;
        moonbit_decref(_M0L8_2afieldS4361);
        #line 379 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        moonbit_free(_M0L4_2axS1383);
      }
      _M0L6_2akeyS1384 = _M0L8_2afieldS3957;
      _M0L3keyS1379 = _M0L6_2akeyS1384;
      goto join_1378;
    }
    goto joinlet_4504;
    join_1378:;
    #line 380 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3625
    = _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1377, _M0L3keyS1379);
    if (!_M0L6_2atmpS3625) {
      #line 381 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1375, _M0L3keyS1379);
      moonbit_decref(_M0L3keyS1379);
    } else {
      moonbit_decref(_M0L3keyS1379);
    }
    continue;
    joinlet_4504:;
    break;
  }
  return _M0L6resultS1375;
}

int32_t _M0MP19moonbitDB8Database6exists(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1373,
  moonbit_string_t _M0L3keyS1374
) {
  #line 369 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 370 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1373, _M0L3keyS1374)
  ) {
    return 0;
  } else {
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3624 =
      _M0L4selfS1373->$0;
    int32_t _result_4505;
    moonbit_incref(_M0L4dataS3624);
    #line 373 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4505
    = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS3624, _M0L3keyS1374);
    moonbit_decref(_M0L4dataS3624);
    return _result_4505;
  }
}

moonbit_string_t _M0MP19moonbitDB8Database3get(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1363,
  moonbit_string_t _M0L3keyS1364
) {
  #line 345 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  #line 346 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  if (
    _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1363, _M0L3keyS1364)
  ) {
    return 0;
  } else {
    moonbit_string_t _M0L1sS1367;
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3623 =
      _M0L4selfS1363->$0;
    void* _M0L7_2abindS1368;
    moonbit_incref(_M0L4dataS3623);
    #line 349 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L7_2abindS1368
    = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3623, _M0L3keyS1364);
    moonbit_decref(_M0L4dataS3623);
    if (_M0L7_2abindS1368 == 0) {
      if (_M0L7_2abindS1368) {
        moonbit_decref(_M0L7_2abindS1368);
      }
      goto join_1365;
    } else {
      void* _M0L7_2aSomeS1369 = _M0L7_2abindS1368;
      void* _M0L4_2axS1370 = _M0L7_2aSomeS1369;
      switch (Moonbit_object_tag(_M0L4_2axS1370)) {
        case 0: {
          struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1371 =
            (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1370;
          moonbit_string_t _M0L8_2afieldS3960 = _M0L9_2aStringS1371->$0;
          int32_t _M0L6_2acntS4363 =
            Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1371));
          moonbit_string_t _M0L4_2asS1372;
          if (_M0L6_2acntS4363 > 1) {
            int32_t _M0L11_2anew__cntS4364 = _M0L6_2acntS4363 - 1;
            Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1371), _M0L11_2anew__cntS4364);
            moonbit_incref(_M0L8_2afieldS3960);
          } else if (_M0L6_2acntS4363 == 1) {
            #line 349 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
            moonbit_free(_M0L9_2aStringS1371);
          }
          _M0L4_2asS1372 = _M0L8_2afieldS3960;
          _M0L1sS1367 = _M0L4_2asS1372;
          goto join_1366;
          break;
        }
        default: {
          moonbit_decref(_M0L4_2axS1370);
          goto join_1365;
          break;
        }
      }
    }
    join_1366:;
    return _M0L1sS1367;
    join_1365:;
    return 0;
  }
}

struct _M0TPB5ArrayGOsE* _M0MP19moonbitDB8Database4mget(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1353,
  struct _M0TPB5ArrayGsE* _M0L4keysS1350
) {
  moonbit_string_t* _M0L6_2atmpS3622;
  struct _M0TPB5ArrayGOsE* _M0L6resultS1348;
  int32_t _M0L7_2abindS1349;
  int32_t _M0L2__S1351;
  #line 276 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3622 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1348
  = (struct _M0TPB5ArrayGOsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGOsE));
  Moonbit_object_header(_M0L6resultS1348)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 28, 0);
  _M0L6resultS1348->$0 = _M0L6_2atmpS3622;
  _M0L6resultS1348->$1 = 0;
  _M0L7_2abindS1349 = _M0L4keysS1350->$1;
  _M0L2__S1351 = 0;
  while (1) {
    if (_M0L2__S1351 < _M0L7_2abindS1349) {
      moonbit_string_t* _M0L3bufS3621 = _M0L4keysS1350->$0;
      moonbit_string_t _M0L3keyS1352 =
        (moonbit_string_t)_M0L3bufS3621[_M0L2__S1351];
      int32_t _M0L6_2atmpS3620;
      moonbit_incref(_M0L3keyS1352);
      #line 279 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      if (
        _M0MP19moonbitDB8Database14check__expired(_M0L4selfS1353, _M0L3keyS1352)
      ) {
        moonbit_string_t _M0L6_2atmpS3616;
        moonbit_decref(_M0L3keyS1352);
        _M0L6_2atmpS3616 = 0;
        #line 280 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1348, _M0L6_2atmpS3616);
        if (_M0L6_2atmpS3616) {
          moonbit_decref(_M0L6_2atmpS3616);
        }
      } else {
        moonbit_string_t _M0L1sS1356;
        struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3619 =
          _M0L4selfS1353->$0;
        void* _M0L7_2abindS1357;
        moonbit_string_t _M0L6_2atmpS3618;
        moonbit_string_t _M0L6_2atmpS3617;
        moonbit_incref(_M0L4dataS3619);
        #line 282 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L7_2abindS1357
        = _M0MPB3Map3getGsRP19moonbitDB10RedisValueE(_M0L4dataS3619, _M0L3keyS1352);
        moonbit_decref(_M0L4dataS3619);
        moonbit_decref(_M0L3keyS1352);
        if (_M0L7_2abindS1357 == 0) {
          if (_M0L7_2abindS1357) {
            moonbit_decref(_M0L7_2abindS1357);
          }
          goto join_1354;
        } else {
          void* _M0L7_2aSomeS1358 = _M0L7_2abindS1357;
          void* _M0L4_2axS1359 = _M0L7_2aSomeS1358;
          switch (Moonbit_object_tag(_M0L4_2axS1359)) {
            case 0: {
              struct _M0DTP19moonbitDB10RedisValue6String* _M0L9_2aStringS1360 =
                (struct _M0DTP19moonbitDB10RedisValue6String*)_M0L4_2axS1359;
              moonbit_string_t _M0L8_2afieldS3962 = _M0L9_2aStringS1360->$0;
              int32_t _M0L6_2acntS4365 =
                Moonbit_rc_count(Moonbit_object_header(_M0L9_2aStringS1360));
              moonbit_string_t _M0L4_2asS1361;
              if (_M0L6_2acntS4365 > 1) {
                int32_t _M0L11_2anew__cntS4366 = _M0L6_2acntS4365 - 1;
                Moonbit_set_rc_count(Moonbit_object_header(_M0L9_2aStringS1360), _M0L11_2anew__cntS4366);
                moonbit_incref(_M0L8_2afieldS3962);
              } else if (_M0L6_2acntS4365 == 1) {
                #line 282 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
                moonbit_free(_M0L9_2aStringS1360);
              }
              _M0L4_2asS1361 = _M0L8_2afieldS3962;
              _M0L1sS1356 = _M0L4_2asS1361;
              goto join_1355;
              break;
            }
            default: {
              moonbit_decref(_M0L4_2axS1359);
              goto join_1354;
              break;
            }
          }
        }
        goto joinlet_4510;
        join_1355:;
        _M0L6_2atmpS3618 = _M0L1sS1356;
        #line 283 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1348, _M0L6_2atmpS3618);
        if (_M0L6_2atmpS3618) {
          moonbit_decref(_M0L6_2atmpS3618);
        }
        joinlet_4510:;
        goto joinlet_4509;
        join_1354:;
        _M0L6_2atmpS3617 = 0;
        #line 284 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGOsE(_M0L6resultS1348, _M0L6_2atmpS3617);
        if (_M0L6_2atmpS3617) {
          moonbit_decref(_M0L6_2atmpS3617);
        }
        joinlet_4509:;
      }
      _M0L6_2atmpS3620 = _M0L2__S1351 + 1;
      _M0L2__S1351 = _M0L6_2atmpS3620;
      continue;
    }
    break;
  }
  return _M0L6resultS1348;
}

int32_t _M0MP19moonbitDB8Database4mset(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1346,
  struct _M0TPB5ArrayGsE* _M0L4keysS1344,
  struct _M0TPB5ArrayGsE* _M0L6valuesS1345
) {
  int32_t _M0L1iS1343;
  #line 269 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L1iS1343 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3608;
    int32_t _if__result_4512;
    #line 270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0L6_2atmpS3608 = _M0MPC15array5Array6lengthGsE(_M0L4keysS1344);
    if (_M0L1iS1343 < _M0L6_2atmpS3608) {
      int32_t _M0L6_2atmpS3607;
      #line 270 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3607 = _M0MPC15array5Array6lengthGsE(_M0L6valuesS1345);
      _if__result_4512 = _M0L1iS1343 < _M0L6_2atmpS3607;
    } else {
      _if__result_4512 = 0;
    }
    if (_if__result_4512) {
      struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3609 =
        _M0L4selfS1346->$0;
      moonbit_string_t _M0L6_2atmpS3610;
      moonbit_string_t _M0L6_2atmpS3612;
      void* _M0L6StringS3611;
      struct _M0TPB3MapGsiE* _M0L7expiresS3613;
      moonbit_string_t _M0L6_2atmpS3614;
      int32_t _M0L6_2atmpS3615;
      moonbit_incref(_M0L4dataS3609);
      #line 271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3610
      = _M0MPC15array5Array2atGsE(_M0L4keysS1344, _M0L1iS1343);
      #line 271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3612
      = _M0MPC15array5Array2atGsE(_M0L6valuesS1345, _M0L1iS1343);
      _M0L6StringS3611
      = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
      Moonbit_object_header(_M0L6StringS3611)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 25, 0);
      ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS3611)->$0
      = _M0L6_2atmpS3612;
      #line 271 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3609, _M0L6_2atmpS3610, _M0L6StringS3611);
      moonbit_decref(_M0L4dataS3609);
      moonbit_decref(_M0L6_2atmpS3610);
      moonbit_decref(_M0L6StringS3611);
      _M0L7expiresS3613 = _M0L4selfS1346->$1;
      moonbit_incref(_M0L7expiresS3613);
      #line 272 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3614
      = _M0MPC15array5Array2atGsE(_M0L4keysS1344, _M0L1iS1343);
      #line 272 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPB3Map6removeGsiE(_M0L7expiresS3613, _M0L6_2atmpS3614);
      moonbit_decref(_M0L7expiresS3613);
      moonbit_decref(_M0L6_2atmpS3614);
      _M0L6_2atmpS3615 = _M0L1iS1343 + 1;
      _M0L1iS1343 = _M0L6_2atmpS3615;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MP19moonbitDB8Database3ttl(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1340,
  moonbit_string_t _M0L3keyS1341
) {
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3599;
  int32_t _M0L6_2atmpS3598;
  #line 226 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4dataS3599 = _M0L4selfS1340->$0;
  moonbit_incref(_M0L4dataS3599);
  #line 227 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3598
  = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS3599, _M0L3keyS1341);
  moonbit_decref(_M0L4dataS3599);
  if (!_M0L6_2atmpS3598) {
    return -2;
  } else {
    struct _M0TPB3MapGsiE* _M0L7expiresS3600 = _M0L4selfS1340->$1;
    int32_t _result_4513;
    moonbit_incref(_M0L7expiresS3600);
    #line 229 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _result_4513 = _M0MPB3Map8containsGsiE(_M0L7expiresS3600, _M0L3keyS1341);
    moonbit_decref(_M0L7expiresS3600);
    if (_result_4513) {
      struct _M0TPB3MapGsiE* _M0L7expiresS3606 = _M0L4selfS1340->$1;
      int64_t _M0L6_2atmpS3605;
      int32_t _M0L6_2atmpS3603;
      int32_t _M0L13current__timeS3604;
      int32_t _M0L9remainingS1342;
      moonbit_incref(_M0L7expiresS3606);
      #line 230 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3605 = _M0MPB3Map3getGsiE(_M0L7expiresS3606, _M0L3keyS1341);
      moonbit_decref(_M0L7expiresS3606);
      #line 230 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3603 = _M0MPC16option6Option6unwrapGiE(_M0L6_2atmpS3605);
      _M0L13current__timeS3604 = _M0L4selfS1340->$2;
      _M0L9remainingS1342 = _M0L6_2atmpS3603 - _M0L13current__timeS3604;
      if (_M0L9remainingS1342 <= 0) {
        struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3601 =
          _M0L4selfS1340->$0;
        struct _M0TPB3MapGsiE* _M0L7expiresS3602;
        moonbit_incref(_M0L4dataS3601);
        #line 232 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(_M0L4dataS3601, _M0L3keyS1341);
        moonbit_decref(_M0L4dataS3601);
        _M0L7expiresS3602 = _M0L4selfS1340->$1;
        moonbit_incref(_M0L7expiresS3602);
        #line 233 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPB3Map6removeGsiE(_M0L7expiresS3602, _M0L3keyS1341);
        moonbit_decref(_M0L7expiresS3602);
        return -2;
      } else {
        return _M0L9remainingS1342 / 1000;
      }
    } else {
      return -1;
    }
  }
}

int32_t _M0MP19moonbitDB8Database6expire(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1337,
  moonbit_string_t _M0L3keyS1338,
  int32_t _M0L7secondsS1339
) {
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3593;
  int32_t _result_4514;
  #line 208 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4dataS3593 = _M0L4selfS1337->$0;
  moonbit_incref(_M0L4dataS3593);
  #line 209 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_4514
  = _M0MPB3Map8containsGsRP19moonbitDB10RedisValueE(_M0L4dataS3593, _M0L3keyS1338);
  moonbit_decref(_M0L4dataS3593);
  if (_result_4514) {
    struct _M0TPB3MapGsiE* _M0L7expiresS3594 = _M0L4selfS1337->$1;
    int32_t _M0L13current__timeS3596 = _M0L4selfS1337->$2;
    int32_t _M0L6_2atmpS3597 = _M0L7secondsS1339 * 1000;
    int32_t _M0L6_2atmpS3595 = _M0L13current__timeS3596 + _M0L6_2atmpS3597;
    moonbit_incref(_M0L7expiresS3594);
    #line 210 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map3setGsiE(_M0L7expiresS3594, _M0L3keyS1338, _M0L6_2atmpS3595);
    moonbit_decref(_M0L7expiresS3594);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database3set(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1334,
  moonbit_string_t _M0L3keyS1335,
  moonbit_string_t _M0L5valueS1336
) {
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3590;
  void* _M0L6StringS3591;
  struct _M0TPB3MapGsiE* _M0L7expiresS3592;
  #line 203 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4dataS3590 = _M0L4selfS1334->$0;
  moonbit_incref(_M0L5valueS1336);
  _M0L6StringS3591
  = (void*)moonbit_malloc(sizeof(struct _M0DTP19moonbitDB10RedisValue6String));
  Moonbit_object_header(_M0L6StringS3591)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 25, 0);
  ((struct _M0DTP19moonbitDB10RedisValue6String*)_M0L6StringS3591)->$0
  = _M0L5valueS1336;
  moonbit_incref(_M0L4dataS3590);
  #line 204 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L4dataS3590, _M0L3keyS1335, _M0L6StringS3591);
  moonbit_decref(_M0L4dataS3590);
  moonbit_decref(_M0L6StringS3591);
  _M0L7expiresS3592 = _M0L4selfS1334->$1;
  moonbit_incref(_M0L7expiresS3592);
  #line 205 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPB3Map6removeGsiE(_M0L7expiresS3592, _M0L3keyS1335);
  moonbit_decref(_M0L7expiresS3592);
  return 0;
}

int32_t _M0MP19moonbitDB8Database14check__expired(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1329,
  moonbit_string_t _M0L3keyS1330
) {
  int32_t _M0L12expire__timeS1328;
  struct _M0TPB3MapGsiE* _M0L7expiresS3589;
  int64_t _M0L7_2abindS1331;
  int32_t _M0L13current__timeS3586;
  #line 188 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7expiresS3589 = _M0L4selfS1329->$1;
  moonbit_incref(_M0L7expiresS3589);
  #line 189 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1331 = _M0MPB3Map3getGsiE(_M0L7expiresS3589, _M0L3keyS1330);
  moonbit_decref(_M0L7expiresS3589);
  if (_M0L7_2abindS1331 == 4294967296ll) {
    return 0;
  } else {
    int64_t _M0L7_2aSomeS1332 = _M0L7_2abindS1331;
    int32_t _M0L15_2aexpire__timeS1333 = (int32_t)_M0L7_2aSomeS1332;
    _M0L12expire__timeS1328 = _M0L15_2aexpire__timeS1333;
    goto join_1327;
  }
  join_1327:;
  _M0L13current__timeS3586 = _M0L4selfS1329->$2;
  if (_M0L12expire__timeS1328 <= _M0L13current__timeS3586) {
    struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4dataS3587 =
      _M0L4selfS1329->$0;
    struct _M0TPB3MapGsiE* _M0L7expiresS3588;
    moonbit_incref(_M0L4dataS3587);
    #line 192 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(_M0L4dataS3587, _M0L3keyS1330);
    moonbit_decref(_M0L4dataS3587);
    _M0L7expiresS3588 = _M0L4selfS1329->$1;
    moonbit_incref(_M0L7expiresS3588);
    #line 193 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
    _M0MPB3Map6removeGsiE(_M0L7expiresS3588, _M0L3keyS1330);
    moonbit_decref(_M0L7expiresS3588);
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0MP19moonbitDB8Database13advance__time(
  struct _M0TP19moonbitDB8Database* _M0L4selfS1325,
  int32_t _M0L2msS1326
) {
  int32_t _M0L13current__timeS3585;
  int32_t _M0L6_2atmpS3584;
  #line 184 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L13current__timeS3585 = _M0L4selfS1325->$2;
  _M0L6_2atmpS3584 = _M0L13current__timeS3585 + _M0L2msS1326;
  _M0L4selfS1325->$2 = _M0L6_2atmpS3584;
  return 0;
}

struct _M0TP19moonbitDB8Database* _M0MP19moonbitDB8Database3new() {
  struct _M0TUsRP19moonbitDB10RedisValueE** _M0L7_2abindS1323;
  struct _M0TUsRP19moonbitDB10RedisValueE** _M0L6_2atmpS3583;
  struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE _M0L6_2atmpS3582;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3578;
  struct _M0TUsiE** _M0L7_2abindS1324;
  struct _M0TUsiE** _M0L6_2atmpS3581;
  struct _M0TPB9ArrayViewGUsiEE _M0L6_2atmpS3580;
  struct _M0TPB3MapGsiE* _M0L6_2atmpS3579;
  struct _M0TP19moonbitDB8Database* _block_4516;
  #line 176 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L7_2abindS1323
  = (struct _M0TUsRP19moonbitDB10RedisValueE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3583 = _M0L7_2abindS1323;
  _M0L6_2atmpS3582
  = (struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE){
    .$0 = _M0L6_2atmpS3583, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3578
  = _M0MPB3Map3MapGsRP19moonbitDB10RedisValueE(_M0L6_2atmpS3582, 1000ll);
  moonbit_decref(_M0L6_2atmpS3582.$0);
  _M0L7_2abindS1324 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3581 = _M0L7_2abindS1324;
  _M0L6_2atmpS3580
  = (struct _M0TPB9ArrayViewGUsiEE){
    .$0 = _M0L6_2atmpS3581, .$1 = 0, .$2 = 0
  };
  #line 177 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3579 = _M0MPB3Map3MapGsiE(_M0L6_2atmpS3580, 1000ll);
  moonbit_decref(_M0L6_2atmpS3580.$0);
  _block_4516
  = (struct _M0TP19moonbitDB8Database*)moonbit_malloc(sizeof(struct _M0TP19moonbitDB8Database));
  Moonbit_object_header(_block_4516)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 31, 0);
  _block_4516->$0 = _M0L6_2atmpS3578;
  _block_4516->$1 = _M0L6_2atmpS3579;
  _block_4516->$2 = 0;
  return _block_4516;
}

struct _M0TPB5ArrayGsE* _M0MP19moonbitDB5Deque9to__array(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1316
) {
  moonbit_string_t* _M0L6_2atmpS3577;
  struct _M0TPB5ArrayGsE* _M0L6resultS1314;
  struct _M0TPB5ArrayGsE* _M0L5frontS3574;
  int32_t _M0L6_2atmpS3573;
  int32_t _M0L6_2atmpS3572;
  int32_t _M0L1iS1315;
  struct _M0TPB5ArrayGsE* _M0L7_2abindS1318;
  int32_t _M0L7_2abindS1319;
  int32_t _M0L2__S1320;
  #line 48 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3577 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6resultS1314
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6resultS1314)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6resultS1314->$0 = _M0L6_2atmpS3577;
  _M0L6resultS1314->$1 = 0;
  _M0L5frontS3574 = _M0L4selfS1316->$0;
  moonbit_incref(_M0L5frontS3574);
  #line 50 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3573 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3574);
  moonbit_decref(_M0L5frontS3574);
  _M0L6_2atmpS3572 = _M0L6_2atmpS3573 - 1;
  _M0L1iS1315 = _M0L6_2atmpS3572;
  while (1) {
    if (_M0L1iS1315 >= 0) {
      struct _M0TPB5ArrayGsE* _M0L5frontS3570 = _M0L4selfS1316->$0;
      moonbit_string_t _M0L6_2atmpS3569;
      int32_t _M0L6_2atmpS3571;
      moonbit_incref(_M0L5frontS3570);
      #line 51 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3569
      = _M0MPC15array5Array2atGsE(_M0L5frontS3570, _M0L1iS1315);
      moonbit_decref(_M0L5frontS3570);
      #line 51 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1314, _M0L6_2atmpS3569);
      moonbit_decref(_M0L6_2atmpS3569);
      _M0L6_2atmpS3571 = _M0L1iS1315 - 1;
      _M0L1iS1315 = _M0L6_2atmpS3571;
      continue;
    }
    break;
  }
  _M0L7_2abindS1318 = _M0L4selfS1316->$1;
  _M0L7_2abindS1319 = _M0L7_2abindS1318->$1;
  moonbit_incref(_M0L7_2abindS1318);
  _M0L2__S1320 = 0;
  while (1) {
    if (_M0L2__S1320 < _M0L7_2abindS1319) {
      moonbit_string_t* _M0L3bufS3576 = _M0L7_2abindS1318->$0;
      moonbit_string_t _M0L4itemS1321 =
        (moonbit_string_t)_M0L3bufS3576[_M0L2__S1320];
      int32_t _M0L6_2atmpS3575;
      moonbit_incref(_M0L4itemS1321);
      #line 54 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0MPC15array5Array4pushGsE(_M0L6resultS1314, _M0L4itemS1321);
      moonbit_decref(_M0L4itemS1321);
      _M0L6_2atmpS3575 = _M0L2__S1320 + 1;
      _M0L2__S1320 = _M0L6_2atmpS3575;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1318);
    }
    break;
  }
  return _M0L6resultS1314;
}

int32_t _M0MP19moonbitDB5Deque6length(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1313
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3568;
  int32_t _M0L6_2atmpS3565;
  struct _M0TPB5ArrayGsE* _M0L4backS3567;
  int32_t _M0L6_2atmpS3566;
  #line 44 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5frontS3568 = _M0L4selfS1313->$0;
  moonbit_incref(_M0L5frontS3568);
  #line 45 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3565 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3568);
  moonbit_decref(_M0L5frontS3568);
  _M0L4backS3567 = _M0L4selfS1313->$1;
  moonbit_incref(_M0L4backS3567);
  #line 45 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3566 = _M0MPC15array5Array6lengthGsE(_M0L4backS3567);
  moonbit_decref(_M0L4backS3567);
  return _M0L6_2atmpS3565 + _M0L6_2atmpS3566;
}

moonbit_string_t _M0MP19moonbitDB5Deque9pop__back(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1306
) {
  struct _M0TPB5ArrayGsE* _M0L4backS3559;
  int32_t _M0L6_2atmpS3558;
  struct _M0TPB5ArrayGsE* _M0L4backS3564;
  moonbit_string_t _result_4521;
  #line 31 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4backS3559 = _M0L4selfS1306->$1;
  moonbit_incref(_M0L4backS3559);
  #line 32 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3558 = _M0MPC15array5Array6lengthGsE(_M0L4backS3559);
  moonbit_decref(_M0L4backS3559);
  if (_M0L6_2atmpS3558 == 0) {
    while (1) {
      struct _M0TPB5ArrayGsE* _M0L5frontS3561 = _M0L4selfS1306->$0;
      int32_t _M0L6_2atmpS3560;
      moonbit_incref(_M0L5frontS3561);
      #line 33 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3560 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3561);
      moonbit_decref(_M0L5frontS3561);
      if (_M0L6_2atmpS3560 > 0) {
        struct _M0TPB5ArrayGsE* _M0L5frontS3563 = _M0L4selfS1306->$0;
        moonbit_string_t _M0L4itemS1307;
        moonbit_string_t _M0L1vS1309;
        struct _M0TPB5ArrayGsE* _M0L4backS3562;
        moonbit_incref(_M0L5frontS3563);
        #line 34 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L4itemS1307 = _M0MPC15array5Array3popGsE(_M0L5frontS3563);
        moonbit_decref(_M0L5frontS3563);
        if (_M0L4itemS1307 == 0) {
          if (_M0L4itemS1307) {
            moonbit_decref(_M0L4itemS1307);
          }
          break;
        } else {
          moonbit_string_t _M0L7_2aSomeS1310 = _M0L4itemS1307;
          moonbit_string_t _M0L4_2avS1311 = _M0L7_2aSomeS1310;
          _M0L1vS1309 = _M0L4_2avS1311;
          goto join_1308;
        }
        goto joinlet_4520;
        join_1308:;
        _M0L4backS3562 = _M0L4selfS1306->$1;
        moonbit_incref(_M0L4backS3562);
        #line 36 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L4backS3562, _M0L1vS1309);
        moonbit_decref(_M0L4backS3562);
        moonbit_decref(_M0L1vS1309);
        joinlet_4520:;
        continue;
      }
      break;
    }
  }
  _M0L4backS3564 = _M0L4selfS1306->$1;
  moonbit_incref(_M0L4backS3564);
  #line 41 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_4521 = _M0MPC15array5Array3popGsE(_M0L4backS3564);
  moonbit_decref(_M0L4backS3564);
  return _result_4521;
}

moonbit_string_t _M0MP19moonbitDB5Deque10pop__front(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1299
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3552;
  int32_t _M0L6_2atmpS3551;
  struct _M0TPB5ArrayGsE* _M0L5frontS3557;
  moonbit_string_t _result_4524;
  #line 18 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5frontS3552 = _M0L4selfS1299->$0;
  moonbit_incref(_M0L5frontS3552);
  #line 19 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3551 = _M0MPC15array5Array6lengthGsE(_M0L5frontS3552);
  moonbit_decref(_M0L5frontS3552);
  if (_M0L6_2atmpS3551 == 0) {
    while (1) {
      struct _M0TPB5ArrayGsE* _M0L4backS3554 = _M0L4selfS1299->$1;
      int32_t _M0L6_2atmpS3553;
      moonbit_incref(_M0L4backS3554);
      #line 20 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
      _M0L6_2atmpS3553 = _M0MPC15array5Array6lengthGsE(_M0L4backS3554);
      moonbit_decref(_M0L4backS3554);
      if (_M0L6_2atmpS3553 > 0) {
        struct _M0TPB5ArrayGsE* _M0L4backS3556 = _M0L4selfS1299->$1;
        moonbit_string_t _M0L4itemS1300;
        moonbit_string_t _M0L1vS1302;
        struct _M0TPB5ArrayGsE* _M0L5frontS3555;
        moonbit_incref(_M0L4backS3556);
        #line 21 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0L4itemS1300 = _M0MPC15array5Array3popGsE(_M0L4backS3556);
        moonbit_decref(_M0L4backS3556);
        if (_M0L4itemS1300 == 0) {
          if (_M0L4itemS1300) {
            moonbit_decref(_M0L4itemS1300);
          }
          break;
        } else {
          moonbit_string_t _M0L7_2aSomeS1303 = _M0L4itemS1300;
          moonbit_string_t _M0L4_2avS1304 = _M0L7_2aSomeS1303;
          _M0L1vS1302 = _M0L4_2avS1304;
          goto join_1301;
        }
        goto joinlet_4523;
        join_1301:;
        _M0L5frontS3555 = _M0L4selfS1299->$0;
        moonbit_incref(_M0L5frontS3555);
        #line 23 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
        _M0MPC15array5Array4pushGsE(_M0L5frontS3555, _M0L1vS1302);
        moonbit_decref(_M0L5frontS3555);
        moonbit_decref(_M0L1vS1302);
        joinlet_4523:;
        continue;
      }
      break;
    }
  }
  _M0L5frontS3557 = _M0L4selfS1299->$0;
  moonbit_incref(_M0L5frontS3557);
  #line 28 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _result_4524 = _M0MPC15array5Array3popGsE(_M0L5frontS3557);
  moonbit_decref(_M0L5frontS3557);
  return _result_4524;
}

int32_t _M0MP19moonbitDB5Deque10push__back(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1297,
  moonbit_string_t _M0L5valueS1298
) {
  struct _M0TPB5ArrayGsE* _M0L4backS3550;
  #line 14 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L4backS3550 = _M0L4selfS1297->$1;
  moonbit_incref(_M0L4backS3550);
  #line 15 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPC15array5Array4pushGsE(_M0L4backS3550, _M0L5valueS1298);
  moonbit_decref(_M0L4backS3550);
  return 0;
}

int32_t _M0MP19moonbitDB5Deque11push__front(
  struct _M0TP19moonbitDB5Deque* _M0L4selfS1295,
  moonbit_string_t _M0L5valueS1296
) {
  struct _M0TPB5ArrayGsE* _M0L5frontS3549;
  #line 10 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L5frontS3549 = _M0L4selfS1295->$0;
  moonbit_incref(_M0L5frontS3549);
  #line 11 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0MPC15array5Array4pushGsE(_M0L5frontS3549, _M0L5valueS1296);
  moonbit_decref(_M0L5frontS3549);
  return 0;
}

struct _M0TP19moonbitDB5Deque* _M0MP19moonbitDB5Deque3new() {
  moonbit_string_t* _M0L6_2atmpS3548;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3545;
  moonbit_string_t* _M0L6_2atmpS3547;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS3546;
  struct _M0TP19moonbitDB5Deque* _block_4525;
  #line 6 "/home/developer/Documents2/moonbitDB/moonbitDB.mbt"
  _M0L6_2atmpS3548 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3545
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3545)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS3545->$0 = _M0L6_2atmpS3548;
  _M0L6_2atmpS3545->$1 = 0;
  _M0L6_2atmpS3547 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS3546
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS3546)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS3546->$0 = _M0L6_2atmpS3547;
  _M0L6_2atmpS3546->$1 = 0;
  _block_4525
  = (struct _M0TP19moonbitDB5Deque*)moonbit_malloc(sizeof(struct _M0TP19moonbitDB5Deque));
  Moonbit_object_header(_block_4525)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 35, 0);
  _block_4525->$0 = _M0L6_2atmpS3545;
  _block_4525->$1 = _M0L6_2atmpS3546;
  return _block_4525;
}

moonbit_string_t _M0IPC15float5FloatPB4Show10to__string(float _M0L4selfS1294) {
  double _M0L6_2atmpS3544;
  #line 16 "/home/developer/.moon/lib/core/float/methods.mbt"
  _M0L6_2atmpS3544 = (double)_M0L4selfS1294;
  #line 17 "/home/developer/.moon/lib/core/float/methods.mbt"
  return _M0MPC16double6Double10to__string(_M0L6_2atmpS3544);
}

moonbit_string_t _M0MPC15array5Array4joinGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1292,
  struct _M0TPC16string10StringView _M0L9separatorS1293
) {
  moonbit_string_t* _M0L3bufS3542;
  int32_t _M0L3lenS3543;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS3541;
  moonbit_string_t _result_4526;
  #line 2184 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3bufS3542 = _M0L4selfS1292->$0;
  _M0L3lenS3543 = _M0L4selfS1292->$1;
  moonbit_incref(_M0L3bufS3542);
  _M0L6_2atmpS3541
  = (struct _M0TPB9ArrayViewGsE){
    .$0 = _M0L3bufS3542, .$1 = 0, .$2 = _M0L3lenS3543
  };
  #line 2188 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _result_4526
  = _M0MPC15array9ArrayView4joinGsE(_M0L6_2atmpS3541, _M0L9separatorS1293);
  moonbit_decref(_M0L6_2atmpS3541.$0);
  return _result_4526;
}

moonbit_string_t _M0MPC15array5Array3popGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1289
) {
  int32_t _M0L3lenS1288;
  #line 325 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS1288 = _M0L4selfS1289->$1;
  if (_M0L3lenS1288 == 0) {
    return 0;
  } else {
    int32_t _M0L5indexS1290 = _M0L3lenS1288 - 1;
    moonbit_string_t* _M0L3bufS3540 = _M0L4selfS1289->$0;
    moonbit_string_t _M0L1vS1291 =
      (moonbit_string_t)_M0L3bufS3540[_M0L5indexS1290];
    moonbit_string_t* _M0L3bufS3539 = _M0L4selfS1289->$0;
    moonbit_string_t _M0L6_2aoldS4000;
    if (
      _M0L5indexS1290 < 0
      || _M0L5indexS1290 >= Moonbit_array_length(_M0L3bufS3539)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4000 = (moonbit_string_t)_M0L3bufS3539[_M0L5indexS1290];
    moonbit_incref(_M0L1vS1291);
    moonbit_decref(_M0L6_2aoldS4000);
    if (
      _M0L5indexS1290 < 0
      || _M0L5indexS1290 >= Moonbit_array_length(_M0L3bufS3539)
    ) {
      #line 332 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS3539[_M0L5indexS1290]
    = (moonbit_string_t)moonbit_string_literal_96.data;
    _M0L4selfS1289->$1 = _M0L5indexS1290;
    return _M0L1vS1291;
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS1280,
  int32_t _M0L5indexS1281
) {
  int32_t _M0L3lenS1279;
  int32_t _if__result_4527;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1279 = _M0L4selfS1280->$1;
  if (_M0L5indexS1281 >= 0) {
    _if__result_4527 = _M0L5indexS1281 < _M0L3lenS1279;
  } else {
    _if__result_4527 = 0;
  }
  if (_if__result_4527) {
    moonbit_string_t* _M0L6_2atmpS3536;
    moonbit_string_t _M0L6_2atmpS4004;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3536 = _M0MPC15array5Array6bufferGsE(_M0L4selfS1280);
    _M0L6_2atmpS4004 = (moonbit_string_t)_M0L6_2atmpS3536[_M0L5indexS1281];
    moonbit_incref(_M0L6_2atmpS4004);
    moonbit_decref(_M0L6_2atmpS3536);
    return _M0L6_2atmpS4004;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t _M0MPC15array5Array2atGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS1283,
  int32_t _M0L5indexS1284
) {
  int32_t _M0L3lenS1282;
  int32_t _if__result_4528;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1282 = _M0L4selfS1283->$1;
  if (_M0L5indexS1284 >= 0) {
    _if__result_4528 = _M0L5indexS1284 < _M0L3lenS1282;
  } else {
    _if__result_4528 = 0;
  }
  if (_if__result_4528) {
    moonbit_string_t* _M0L6_2atmpS3537;
    moonbit_string_t _M0L6_2atmpS4005;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3537 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS1283);
    _M0L6_2atmpS4005 = (moonbit_string_t)_M0L6_2atmpS3537[_M0L5indexS1284];
    if (_M0L6_2atmpS4005) {
      moonbit_incref(_M0L6_2atmpS4005);
    }
    moonbit_decref(_M0L6_2atmpS3537);
    return _M0L6_2atmpS4005;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

struct _M0TUsfE* _M0MPC15array5Array2atGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS1286,
  int32_t _M0L5indexS1287
) {
  int32_t _M0L3lenS1285;
  int32_t _if__result_4529;
  #line 181 "/home/developer/.moon/lib/core/builtin/array.mbt"
  _M0L3lenS1285 = _M0L4selfS1286->$1;
  if (_M0L5indexS1287 >= 0) {
    _if__result_4529 = _M0L5indexS1287 < _M0L3lenS1285;
  } else {
    _if__result_4529 = 0;
  }
  if (_if__result_4529) {
    struct _M0TUsfE** _M0L6_2atmpS3538;
    struct _M0TUsfE* _M0L6_2atmpS4006;
    #line 186 "/home/developer/.moon/lib/core/builtin/array.mbt"
    _M0L6_2atmpS3538 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS1286);
    _M0L6_2atmpS4006 = (struct _M0TUsfE*)_M0L6_2atmpS3538[_M0L5indexS1287];
    if (_M0L6_2atmpS4006) {
      moonbit_incref(_M0L6_2atmpS4006);
    }
    moonbit_decref(_M0L6_2atmpS3538);
    return _M0L6_2atmpS4006;
  } else {
    #line 185 "/home/developer/.moon/lib/core/builtin/array.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1278) {
  moonbit_string_t _M0L6_2atmpS3535;
  #line 36 "/home/developer/.moon/lib/core/builtin/console.mbt"
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  _M0L6_2atmpS3535
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1278);
  #line 37 "/home/developer/.moon/lib/core/builtin/console.mbt"
  moonbit_println(_M0L6_2atmpS3535);
  moonbit_decref(_M0L6_2atmpS3535);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1277) {
  #line 282 "/home/developer/.moon/lib/core/builtin/double.mbt"
  #line 284 "/home/developer/.moon/lib/core/builtin/double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1277);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1264) {
  uint64_t _M0L4bitsS1265;
  uint64_t _M0L6_2atmpS3534;
  uint64_t _M0L6_2atmpS3533;
  int32_t _M0L8ieeeSignS1266;
  uint64_t _M0L12ieeeMantissaS1267;
  uint64_t _M0L6_2atmpS3532;
  uint64_t _M0L6_2atmpS3531;
  int32_t _M0L12ieeeExponentS1268;
  int32_t _if__result_4530;
  struct _M0TPB17FloatingDecimal64* _M0L7_2abindS1269;
  struct _M0TPB17FloatingDecimal64* _M0L1vS1270;
  moonbit_string_t _result_4532;
  #line 659 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L3valS1264 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  }
  _M0L4bitsS1265 = *(int64_t*)&_M0L3valS1264;
  _M0L6_2atmpS3534 = _M0L4bitsS1265 >> 63;
  _M0L6_2atmpS3533 = _M0L6_2atmpS3534 & 1ull;
  _M0L8ieeeSignS1266 = _M0L6_2atmpS3533 != 0ull;
  _M0L12ieeeMantissaS1267 = _M0L4bitsS1265 & 4503599627370495ull;
  _M0L6_2atmpS3532 = _M0L4bitsS1265 >> 52;
  _M0L6_2atmpS3531 = _M0L6_2atmpS3532 & 2047ull;
  _M0L12ieeeExponentS1268 = (int32_t)_M0L6_2atmpS3531;
  if (_M0L12ieeeExponentS1268 == 2047) {
    _if__result_4530 = 1;
  } else if (_M0L12ieeeExponentS1268 == 0) {
    _if__result_4530 = _M0L12ieeeMantissaS1267 == 0ull;
  } else {
    _if__result_4530 = 0;
  }
  if (_if__result_4530) {
    int32_t _M0L6_2atmpS3522 = _M0L12ieeeExponentS1268 != 0;
    int32_t _M0L6_2atmpS3523 = _M0L12ieeeMantissaS1267 != 0ull;
    #line 676 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1266, _M0L6_2atmpS3522, _M0L6_2atmpS3523);
  }
  #line 678 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1269
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1267, _M0L12ieeeExponentS1268);
  if (_M0L7_2abindS1269 == 0) {
    uint32_t _M0L6_2atmpS3524;
    if (_M0L7_2abindS1269) {
      moonbit_decref(_M0L7_2abindS1269);
    }
    _M0L6_2atmpS3524 = *(uint32_t*)&_M0L12ieeeExponentS1268;
    #line 688 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L1vS1270 = _M0FPB3d2d(_M0L12ieeeMantissaS1267, _M0L6_2atmpS3524);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1271 = _M0L7_2abindS1269;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1272 = _M0L7_2aSomeS1271;
    struct _M0TPB17FloatingDecimal64* _M0L1xS1273 = _M0L4_2afS1272;
    while (1) {
      uint64_t _M0L8mantissaS3530 = _M0L1xS1273->$0;
      uint64_t _M0L1qS1274 = _M0L8mantissaS3530 / 10ull;
      uint64_t _M0L8mantissaS3528 = _M0L1xS1273->$0;
      uint64_t _M0L6_2atmpS3529 = 10ull * _M0L1qS1274;
      uint64_t _M0L1rS1275 = _M0L8mantissaS3528 - _M0L6_2atmpS3529;
      int32_t _M0L8exponentS3527;
      int32_t _M0L6_2atmpS3526;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3525;
      if (_M0L1rS1275 != 0ull) {
        _M0L1vS1270 = _M0L1xS1273;
        break;
      }
      _M0L8exponentS3527 = _M0L1xS1273->$1;
      moonbit_decref(_M0L1xS1273);
      _M0L6_2atmpS3526 = _M0L8exponentS3527 + 1;
      _M0L6_2atmpS3525
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0L6_2atmpS3525)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
      _M0L6_2atmpS3525->$0 = _M0L1qS1274;
      _M0L6_2atmpS3525->$1 = _M0L6_2atmpS3526;
      _M0L1xS1273 = _M0L6_2atmpS3525;
      continue;
      break;
    }
  }
  #line 690 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_4532 = _M0FPB9to__chars(_M0L1vS1270, _M0L8ieeeSignS1266);
  moonbit_decref(_M0L1vS1270);
  return _result_4532;
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1259,
  int32_t _M0L12ieeeExponentS1261
) {
  uint64_t _M0L2m2S1258;
  int32_t _M0L6_2atmpS3521;
  int32_t _M0L2e2S1260;
  int32_t _M0L6_2atmpS3520;
  uint64_t _M0L6_2atmpS3519;
  uint64_t _M0L4maskS1262;
  uint64_t _M0L8fractionS1263;
  int32_t _M0L6_2atmpS3518;
  uint64_t _M0L6_2atmpS3517;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3516;
  #line 637 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2m2S1258 = 4503599627370496ull | _M0L12ieeeMantissaS1259;
  _M0L6_2atmpS3521 = _M0L12ieeeExponentS1261 - 1023;
  _M0L2e2S1260 = _M0L6_2atmpS3521 - 52;
  if (_M0L2e2S1260 > 0) {
    return 0;
  }
  if (_M0L2e2S1260 < -52) {
    return 0;
  }
  _M0L6_2atmpS3520 = -_M0L2e2S1260;
  _M0L6_2atmpS3519 = 1ull << (_M0L6_2atmpS3520 & 63);
  _M0L4maskS1262 = _M0L6_2atmpS3519 - 1ull;
  _M0L8fractionS1263 = _M0L2m2S1258 & _M0L4maskS1262;
  if (_M0L8fractionS1263 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3518 = -_M0L2e2S1260;
  _M0L6_2atmpS3517 = _M0L2m2S1258 >> (_M0L6_2atmpS3518 & 63);
  _M0L6_2atmpS3516
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS3516)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L6_2atmpS3516->$0 = _M0L6_2atmpS3517;
  _M0L6_2atmpS3516->$1 = 0;
  return _M0L6_2atmpS3516;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1226,
  int32_t _M0L4signS1224
) {
  int32_t _M0L6_2atmpS3515;
  moonbit_bytes_t _M0L6resultS1222;
  int32_t _M0Lm5indexS1223;
  uint64_t _M0L6outputS1225;
  int32_t _M0L7olengthS1227;
  int32_t _M0L8exponentS3514;
  int32_t _M0L6_2atmpS3513;
  int32_t _M0Lm3expS1228;
  int32_t _M0L6_2atmpS3512;
  int32_t _M0L6_2atmpS3510;
  int32_t _M0L18scientificNotationS1229;
  #line 530 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 532 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3515 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1222
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS3515);
  _M0Lm5indexS1223 = 0;
  if (_M0L4signS1224) {
    int32_t _M0L6_2atmpS3384 = _M0Lm5indexS1223;
    int32_t _M0L6_2atmpS3385;
    if (
      _M0L6_2atmpS3384 < 0
      || _M0L6_2atmpS3384 >= Moonbit_array_length(_M0L6resultS1222)
    ) {
      #line 535 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1222[_M0L6_2atmpS3384] = 45;
    _M0L6_2atmpS3385 = _M0Lm5indexS1223;
    _M0Lm5indexS1223 = _M0L6_2atmpS3385 + 1;
  }
  _M0L6outputS1225 = _M0L1vS1226->$0;
  #line 539 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7olengthS1227 = _M0FPB17decimal__length17(_M0L6outputS1225);
  _M0L8exponentS3514 = _M0L1vS1226->$1;
  _M0L6_2atmpS3513 = _M0L8exponentS3514 + _M0L7olengthS1227;
  _M0Lm3expS1228 = _M0L6_2atmpS3513 - 1;
  _M0L6_2atmpS3512 = _M0Lm3expS1228;
  if (_M0L6_2atmpS3512 >= -6) {
    int32_t _M0L6_2atmpS3511 = _M0Lm3expS1228;
    _M0L6_2atmpS3510 = _M0L6_2atmpS3511 < 21;
  } else {
    _M0L6_2atmpS3510 = 0;
  }
  _M0L18scientificNotationS1229 = !_M0L6_2atmpS3510;
  if (_M0L18scientificNotationS1229) {
    int32_t _M0L7_2abindS1230 = _M0L7olengthS1227 - 1;
    uint64_t _M0L6outputS1231;
    int32_t _M0L1iS1232 = 0;
    uint64_t _M0L6outputS1233 = _M0L6outputS1225;
    int32_t _M0L6_2atmpS3386;
    int32_t _M0L6_2atmpS3390;
    int32_t _M0L6_2atmpS3389;
    int32_t _M0L6_2atmpS3388;
    int32_t _M0L6_2atmpS3387;
    int32_t _M0L6_2atmpS3394;
    int32_t _M0L6_2atmpS3395;
    int32_t _M0L6_2atmpS3396;
    int32_t _M0L6_2atmpS3397;
    int32_t _M0L6_2atmpS3398;
    int32_t _M0L6_2atmpS3404;
    int32_t _M0L6_2atmpS3437;
    moonbit_string_t _result_4534;
    while (1) {
      if (_M0L1iS1232 < _M0L7_2abindS1230) {
        uint64_t _M0L1cS1234 = _M0L6outputS1233 % 10ull;
        int32_t _M0L6_2atmpS3443 = _M0Lm5indexS1223;
        int32_t _M0L6_2atmpS3442 = _M0L6_2atmpS3443 + _M0L7olengthS1227;
        int32_t _M0L6_2atmpS3438 = _M0L6_2atmpS3442 - _M0L1iS1232;
        int32_t _M0L6_2atmpS3441 = (int32_t)_M0L1cS1234;
        int32_t _M0L6_2atmpS3440 = 48 + _M0L6_2atmpS3441;
        int32_t _M0L6_2atmpS3439 = _M0L6_2atmpS3440 & 0xff;
        int32_t _M0L6_2atmpS3444;
        uint64_t _M0L6_2atmpS3445;
        if (
          _M0L6_2atmpS3438 < 0
          || _M0L6_2atmpS3438 >= Moonbit_array_length(_M0L6resultS1222)
        ) {
          #line 547 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1222[_M0L6_2atmpS3438] = _M0L6_2atmpS3439;
        _M0L6_2atmpS3444 = _M0L1iS1232 + 1;
        _M0L6_2atmpS3445 = _M0L6outputS1233 / 10ull;
        _M0L1iS1232 = _M0L6_2atmpS3444;
        _M0L6outputS1233 = _M0L6_2atmpS3445;
        continue;
      } else {
        _M0L6outputS1231 = _M0L6outputS1233;
      }
      break;
    }
    _M0L6_2atmpS3386 = _M0Lm5indexS1223;
    _M0L6_2atmpS3390 = (int32_t)_M0L6outputS1231;
    _M0L6_2atmpS3389 = _M0L6_2atmpS3390 % 10;
    _M0L6_2atmpS3388 = 48 + _M0L6_2atmpS3389;
    _M0L6_2atmpS3387 = _M0L6_2atmpS3388 & 0xff;
    if (
      _M0L6_2atmpS3386 < 0
      || _M0L6_2atmpS3386 >= Moonbit_array_length(_M0L6resultS1222)
    ) {
      #line 552 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1222[_M0L6_2atmpS3386] = _M0L6_2atmpS3387;
    if (_M0L7olengthS1227 > 1) {
      int32_t _M0L6_2atmpS3392 = _M0Lm5indexS1223;
      int32_t _M0L6_2atmpS3391 = _M0L6_2atmpS3392 + 1;
      if (
        _M0L6_2atmpS3391 < 0
        || _M0L6_2atmpS3391 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 554 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3391] = 46;
    } else {
      int32_t _M0L6_2atmpS3393 = _M0Lm5indexS1223;
      _M0Lm5indexS1223 = _M0L6_2atmpS3393 - 1;
    }
    _M0L6_2atmpS3394 = _M0Lm5indexS1223;
    _M0L6_2atmpS3395 = _M0L7olengthS1227 + 1;
    _M0Lm5indexS1223 = _M0L6_2atmpS3394 + _M0L6_2atmpS3395;
    _M0L6_2atmpS3396 = _M0Lm5indexS1223;
    if (
      _M0L6_2atmpS3396 < 0
      || _M0L6_2atmpS3396 >= Moonbit_array_length(_M0L6resultS1222)
    ) {
      #line 562 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1222[_M0L6_2atmpS3396] = 101;
    _M0L6_2atmpS3397 = _M0Lm5indexS1223;
    _M0Lm5indexS1223 = _M0L6_2atmpS3397 + 1;
    _M0L6_2atmpS3398 = _M0Lm3expS1228;
    if (_M0L6_2atmpS3398 < 0) {
      int32_t _M0L6_2atmpS3399 = _M0Lm5indexS1223;
      int32_t _M0L6_2atmpS3400;
      int32_t _M0L6_2atmpS3401;
      if (
        _M0L6_2atmpS3399 < 0
        || _M0L6_2atmpS3399 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 565 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3399] = 45;
      _M0L6_2atmpS3400 = _M0Lm5indexS1223;
      _M0Lm5indexS1223 = _M0L6_2atmpS3400 + 1;
      _M0L6_2atmpS3401 = _M0Lm3expS1228;
      _M0Lm3expS1228 = -_M0L6_2atmpS3401;
    } else {
      int32_t _M0L6_2atmpS3402 = _M0Lm5indexS1223;
      int32_t _M0L6_2atmpS3403;
      if (
        _M0L6_2atmpS3402 < 0
        || _M0L6_2atmpS3402 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 569 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3402] = 43;
      _M0L6_2atmpS3403 = _M0Lm5indexS1223;
      _M0Lm5indexS1223 = _M0L6_2atmpS3403 + 1;
    }
    _M0L6_2atmpS3404 = _M0Lm3expS1228;
    if (_M0L6_2atmpS3404 >= 100) {
      int32_t _M0L6_2atmpS3420 = _M0Lm3expS1228;
      int32_t _M0L1aS1236 = _M0L6_2atmpS3420 / 100;
      int32_t _M0L6_2atmpS3419 = _M0Lm3expS1228;
      int32_t _M0L6_2atmpS3418 = _M0L6_2atmpS3419 / 10;
      int32_t _M0L1bS1237 = _M0L6_2atmpS3418 % 10;
      int32_t _M0L6_2atmpS3417 = _M0Lm3expS1228;
      int32_t _M0L1cS1238 = _M0L6_2atmpS3417 % 10;
      int32_t _M0L6_2atmpS3405 = _M0Lm5indexS1223;
      int32_t _M0L6_2atmpS3407 = 48 + _M0L1aS1236;
      int32_t _M0L6_2atmpS3406 = _M0L6_2atmpS3407 & 0xff;
      int32_t _M0L6_2atmpS3411;
      int32_t _M0L6_2atmpS3408;
      int32_t _M0L6_2atmpS3410;
      int32_t _M0L6_2atmpS3409;
      int32_t _M0L6_2atmpS3415;
      int32_t _M0L6_2atmpS3412;
      int32_t _M0L6_2atmpS3414;
      int32_t _M0L6_2atmpS3413;
      int32_t _M0L6_2atmpS3416;
      if (
        _M0L6_2atmpS3405 < 0
        || _M0L6_2atmpS3405 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 576 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3405] = _M0L6_2atmpS3406;
      _M0L6_2atmpS3411 = _M0Lm5indexS1223;
      _M0L6_2atmpS3408 = _M0L6_2atmpS3411 + 1;
      _M0L6_2atmpS3410 = 48 + _M0L1bS1237;
      _M0L6_2atmpS3409 = _M0L6_2atmpS3410 & 0xff;
      if (
        _M0L6_2atmpS3408 < 0
        || _M0L6_2atmpS3408 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 577 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3408] = _M0L6_2atmpS3409;
      _M0L6_2atmpS3415 = _M0Lm5indexS1223;
      _M0L6_2atmpS3412 = _M0L6_2atmpS3415 + 2;
      _M0L6_2atmpS3414 = 48 + _M0L1cS1238;
      _M0L6_2atmpS3413 = _M0L6_2atmpS3414 & 0xff;
      if (
        _M0L6_2atmpS3412 < 0
        || _M0L6_2atmpS3412 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 578 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3412] = _M0L6_2atmpS3413;
      _M0L6_2atmpS3416 = _M0Lm5indexS1223;
      _M0Lm5indexS1223 = _M0L6_2atmpS3416 + 3;
    } else {
      int32_t _M0L6_2atmpS3421 = _M0Lm3expS1228;
      if (_M0L6_2atmpS3421 >= 10) {
        int32_t _M0L6_2atmpS3431 = _M0Lm3expS1228;
        int32_t _M0L1aS1239 = _M0L6_2atmpS3431 / 10;
        int32_t _M0L6_2atmpS3430 = _M0Lm3expS1228;
        int32_t _M0L1bS1240 = _M0L6_2atmpS3430 % 10;
        int32_t _M0L6_2atmpS3422 = _M0Lm5indexS1223;
        int32_t _M0L6_2atmpS3424 = 48 + _M0L1aS1239;
        int32_t _M0L6_2atmpS3423 = _M0L6_2atmpS3424 & 0xff;
        int32_t _M0L6_2atmpS3428;
        int32_t _M0L6_2atmpS3425;
        int32_t _M0L6_2atmpS3427;
        int32_t _M0L6_2atmpS3426;
        int32_t _M0L6_2atmpS3429;
        if (
          _M0L6_2atmpS3422 < 0
          || _M0L6_2atmpS3422 >= Moonbit_array_length(_M0L6resultS1222)
        ) {
          #line 583 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1222[_M0L6_2atmpS3422] = _M0L6_2atmpS3423;
        _M0L6_2atmpS3428 = _M0Lm5indexS1223;
        _M0L6_2atmpS3425 = _M0L6_2atmpS3428 + 1;
        _M0L6_2atmpS3427 = 48 + _M0L1bS1240;
        _M0L6_2atmpS3426 = _M0L6_2atmpS3427 & 0xff;
        if (
          _M0L6_2atmpS3425 < 0
          || _M0L6_2atmpS3425 >= Moonbit_array_length(_M0L6resultS1222)
        ) {
          #line 584 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1222[_M0L6_2atmpS3425] = _M0L6_2atmpS3426;
        _M0L6_2atmpS3429 = _M0Lm5indexS1223;
        _M0Lm5indexS1223 = _M0L6_2atmpS3429 + 2;
      } else {
        int32_t _M0L6_2atmpS3432 = _M0Lm5indexS1223;
        int32_t _M0L6_2atmpS3435 = _M0Lm3expS1228;
        int32_t _M0L6_2atmpS3434 = 48 + _M0L6_2atmpS3435;
        int32_t _M0L6_2atmpS3433 = _M0L6_2atmpS3434 & 0xff;
        int32_t _M0L6_2atmpS3436;
        if (
          _M0L6_2atmpS3432 < 0
          || _M0L6_2atmpS3432 >= Moonbit_array_length(_M0L6resultS1222)
        ) {
          #line 587 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1222[_M0L6_2atmpS3432] = _M0L6_2atmpS3433;
        _M0L6_2atmpS3436 = _M0Lm5indexS1223;
        _M0Lm5indexS1223 = _M0L6_2atmpS3436 + 1;
      }
    }
    _M0L6_2atmpS3437 = _M0Lm5indexS1223;
    #line 590 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4534
    = _M0FPB19string__from__bytes(_M0L6resultS1222, 0, _M0L6_2atmpS3437);
    moonbit_decref(_M0L6resultS1222);
    return _result_4534;
  } else {
    int32_t _M0L6_2atmpS3446 = _M0Lm3expS1228;
    int32_t _M0L6_2atmpS3509;
    moonbit_string_t _result_4540;
    if (_M0L6_2atmpS3446 < 0) {
      int32_t _M0L6_2atmpS3447 = _M0Lm5indexS1223;
      int32_t _M0L6_2atmpS3449;
      int32_t _M0L6_2atmpS3448;
      int32_t _M0L6_2atmpS3450;
      int32_t _M0L1iS1241;
      int32_t _M0L6_2atmpS3465;
      int32_t _M0L6_2atmpS3467;
      int32_t _M0L6_2atmpS3466;
      int32_t _M0L7currentS1243;
      int32_t _M0L1iS1244;
      uint64_t _M0L6outputS1245;
      if (
        _M0L6_2atmpS3447 < 0
        || _M0L6_2atmpS3447 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 595 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3447] = 48;
      _M0L6_2atmpS3449 = _M0Lm5indexS1223;
      _M0L6_2atmpS3448 = _M0L6_2atmpS3449 + 1;
      if (
        _M0L6_2atmpS3448 < 0
        || _M0L6_2atmpS3448 >= Moonbit_array_length(_M0L6resultS1222)
      ) {
        #line 596 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1222[_M0L6_2atmpS3448] = 46;
      _M0L6_2atmpS3450 = _M0Lm5indexS1223;
      _M0Lm5indexS1223 = _M0L6_2atmpS3450 + 2;
      _M0L1iS1241 = -1;
      while (1) {
        int32_t _M0L6_2atmpS3451 = _M0Lm3expS1228;
        if (_M0L1iS1241 > _M0L6_2atmpS3451) {
          int32_t _M0L6_2atmpS3454 = _M0Lm5indexS1223;
          int32_t _M0L6_2atmpS3453 = _M0L6_2atmpS3454 - _M0L1iS1241;
          int32_t _M0L6_2atmpS3452 = _M0L6_2atmpS3453 - 1;
          int32_t _M0L6_2atmpS3455;
          if (
            _M0L6_2atmpS3452 < 0
            || _M0L6_2atmpS3452 >= Moonbit_array_length(_M0L6resultS1222)
          ) {
            #line 599 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1222[_M0L6_2atmpS3452] = 48;
          _M0L6_2atmpS3455 = _M0L1iS1241 - 1;
          _M0L1iS1241 = _M0L6_2atmpS3455;
          continue;
        }
        break;
      }
      _M0L6_2atmpS3465 = _M0Lm5indexS1223;
      _M0L6_2atmpS3467 = _M0Lm3expS1228;
      _M0L6_2atmpS3466 = -1 - _M0L6_2atmpS3467;
      _M0L7currentS1243 = _M0L6_2atmpS3465 + _M0L6_2atmpS3466;
      _M0L1iS1244 = 0;
      _M0L6outputS1245 = _M0L6outputS1225;
      while (1) {
        if (_M0L1iS1244 < _M0L7olengthS1227) {
          int32_t _M0L6_2atmpS3462 = _M0L7currentS1243 + _M0L7olengthS1227;
          int32_t _M0L6_2atmpS3461 = _M0L6_2atmpS3462 - _M0L1iS1244;
          int32_t _M0L6_2atmpS3456 = _M0L6_2atmpS3461 - 1;
          uint64_t _M0L6_2atmpS3460 = _M0L6outputS1245 % 10ull;
          int32_t _M0L6_2atmpS3459 = (int32_t)_M0L6_2atmpS3460;
          int32_t _M0L6_2atmpS3458 = 48 + _M0L6_2atmpS3459;
          int32_t _M0L6_2atmpS3457 = _M0L6_2atmpS3458 & 0xff;
          int32_t _M0L6_2atmpS3463;
          uint64_t _M0L6_2atmpS3464;
          if (
            _M0L6_2atmpS3456 < 0
            || _M0L6_2atmpS3456 >= Moonbit_array_length(_M0L6resultS1222)
          ) {
            #line 603 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1222[_M0L6_2atmpS3456] = _M0L6_2atmpS3457;
          _M0L6_2atmpS3463 = _M0L1iS1244 + 1;
          _M0L6_2atmpS3464 = _M0L6outputS1245 / 10ull;
          _M0L1iS1244 = _M0L6_2atmpS3463;
          _M0L6outputS1245 = _M0L6_2atmpS3464;
          continue;
        }
        break;
      }
      _M0Lm5indexS1223 = _M0L7currentS1243 + _M0L7olengthS1227;
    } else {
      int32_t _M0L6_2atmpS3469 = _M0Lm3expS1228;
      int32_t _M0L6_2atmpS3468 = _M0L6_2atmpS3469 + 1;
      if (_M0L6_2atmpS3468 >= _M0L7olengthS1227) {
        int32_t _M0L1iS1247 = 0;
        uint64_t _M0L6outputS1248 = _M0L6outputS1225;
        int32_t _M0L6_2atmpS3480;
        int32_t _M0L6_2atmpS3485;
        int32_t _M0L7_2abindS1250;
        int32_t _M0L1iS1251;
        int32_t _M0L6_2atmpS3486;
        int32_t _M0L6_2atmpS3489;
        int32_t _M0L6_2atmpS3488;
        int32_t _M0L6_2atmpS3487;
        while (1) {
          if (_M0L1iS1247 < _M0L7olengthS1227) {
            int32_t _M0L6_2atmpS3477 = _M0Lm5indexS1223;
            int32_t _M0L6_2atmpS3476 = _M0L6_2atmpS3477 + _M0L7olengthS1227;
            int32_t _M0L6_2atmpS3475 = _M0L6_2atmpS3476 - _M0L1iS1247;
            int32_t _M0L6_2atmpS3470 = _M0L6_2atmpS3475 - 1;
            uint64_t _M0L6_2atmpS3474 = _M0L6outputS1248 % 10ull;
            int32_t _M0L6_2atmpS3473 = (int32_t)_M0L6_2atmpS3474;
            int32_t _M0L6_2atmpS3472 = 48 + _M0L6_2atmpS3473;
            int32_t _M0L6_2atmpS3471 = _M0L6_2atmpS3472 & 0xff;
            int32_t _M0L6_2atmpS3478;
            uint64_t _M0L6_2atmpS3479;
            if (
              _M0L6_2atmpS3470 < 0
              || _M0L6_2atmpS3470 >= Moonbit_array_length(_M0L6resultS1222)
            ) {
              #line 610 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1222[_M0L6_2atmpS3470] = _M0L6_2atmpS3471;
            _M0L6_2atmpS3478 = _M0L1iS1247 + 1;
            _M0L6_2atmpS3479 = _M0L6outputS1248 / 10ull;
            _M0L1iS1247 = _M0L6_2atmpS3478;
            _M0L6outputS1248 = _M0L6_2atmpS3479;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3480 = _M0Lm5indexS1223;
        _M0Lm5indexS1223 = _M0L6_2atmpS3480 + _M0L7olengthS1227;
        _M0L6_2atmpS3485 = _M0Lm3expS1228;
        _M0L7_2abindS1250 = _M0L6_2atmpS3485 + 1;
        _M0L1iS1251 = _M0L7olengthS1227;
        while (1) {
          if (_M0L1iS1251 < _M0L7_2abindS1250) {
            int32_t _M0L6_2atmpS3483 = _M0Lm5indexS1223;
            int32_t _M0L6_2atmpS3482 = _M0L6_2atmpS3483 + _M0L1iS1251;
            int32_t _M0L6_2atmpS3481 = _M0L6_2atmpS3482 - _M0L7olengthS1227;
            int32_t _M0L6_2atmpS3484;
            if (
              _M0L6_2atmpS3481 < 0
              || _M0L6_2atmpS3481 >= Moonbit_array_length(_M0L6resultS1222)
            ) {
              #line 615 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1222[_M0L6_2atmpS3481] = 48;
            _M0L6_2atmpS3484 = _M0L1iS1251 + 1;
            _M0L1iS1251 = _M0L6_2atmpS3484;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3486 = _M0Lm5indexS1223;
        _M0L6_2atmpS3489 = _M0Lm3expS1228;
        _M0L6_2atmpS3488 = _M0L6_2atmpS3489 + 1;
        _M0L6_2atmpS3487 = _M0L6_2atmpS3488 - _M0L7olengthS1227;
        _M0Lm5indexS1223 = _M0L6_2atmpS3486 + _M0L6_2atmpS3487;
      } else {
        int32_t _M0L6_2atmpS3506 = _M0Lm5indexS1223;
        int32_t _M0L6_2atmpS3505 = _M0L6_2atmpS3506 + 1;
        int32_t _M0L1iS1253 = 0;
        int32_t _M0L7currentS1254 = _M0L6_2atmpS3505;
        uint64_t _M0L6outputS1255 = _M0L6outputS1225;
        int32_t _M0L6_2atmpS3507;
        int32_t _M0L6_2atmpS3508;
        while (1) {
          if (_M0L1iS1253 < _M0L7olengthS1227) {
            int32_t _M0L6_2atmpS3501 = _M0L7olengthS1227 - _M0L1iS1253;
            int32_t _M0L6_2atmpS3499 = _M0L6_2atmpS3501 - 1;
            int32_t _M0L6_2atmpS3500 = _M0Lm3expS1228;
            int32_t _M0L7currentS1256;
            int32_t _M0L6_2atmpS3496;
            int32_t _M0L6_2atmpS3495;
            int32_t _M0L6_2atmpS3490;
            uint64_t _M0L6_2atmpS3494;
            int32_t _M0L6_2atmpS3493;
            int32_t _M0L6_2atmpS3492;
            int32_t _M0L6_2atmpS3491;
            int32_t _M0L6_2atmpS3497;
            uint64_t _M0L6_2atmpS3498;
            if (_M0L6_2atmpS3499 == _M0L6_2atmpS3500) {
              int32_t _M0L6_2atmpS3504 =
                _M0L7currentS1254 + _M0L7olengthS1227;
              int32_t _M0L6_2atmpS3503 = _M0L6_2atmpS3504 - _M0L1iS1253;
              int32_t _M0L6_2atmpS3502 = _M0L6_2atmpS3503 - 1;
              if (
                _M0L6_2atmpS3502 < 0
                || _M0L6_2atmpS3502 >= Moonbit_array_length(_M0L6resultS1222)
              ) {
                #line 622 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1222[_M0L6_2atmpS3502] = 46;
              _M0L7currentS1256 = _M0L7currentS1254 - 1;
            } else {
              _M0L7currentS1256 = _M0L7currentS1254;
            }
            _M0L6_2atmpS3496 = _M0L7currentS1256 + _M0L7olengthS1227;
            _M0L6_2atmpS3495 = _M0L6_2atmpS3496 - _M0L1iS1253;
            _M0L6_2atmpS3490 = _M0L6_2atmpS3495 - 1;
            _M0L6_2atmpS3494 = _M0L6outputS1255 % 10ull;
            _M0L6_2atmpS3493 = (int32_t)_M0L6_2atmpS3494;
            _M0L6_2atmpS3492 = 48 + _M0L6_2atmpS3493;
            _M0L6_2atmpS3491 = _M0L6_2atmpS3492 & 0xff;
            if (
              _M0L6_2atmpS3490 < 0
              || _M0L6_2atmpS3490 >= Moonbit_array_length(_M0L6resultS1222)
            ) {
              #line 627 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1222[_M0L6_2atmpS3490] = _M0L6_2atmpS3491;
            _M0L6_2atmpS3497 = _M0L1iS1253 + 1;
            _M0L6_2atmpS3498 = _M0L6outputS1255 / 10ull;
            _M0L1iS1253 = _M0L6_2atmpS3497;
            _M0L7currentS1254 = _M0L7currentS1256;
            _M0L6outputS1255 = _M0L6_2atmpS3498;
            continue;
          }
          break;
        }
        _M0L6_2atmpS3507 = _M0Lm5indexS1223;
        _M0L6_2atmpS3508 = _M0L7olengthS1227 + 1;
        _M0Lm5indexS1223 = _M0L6_2atmpS3507 + _M0L6_2atmpS3508;
      }
    }
    _M0L6_2atmpS3509 = _M0Lm5indexS1223;
    #line 632 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4540
    = _M0FPB19string__from__bytes(_M0L6resultS1222, 0, _M0L6_2atmpS3509);
    moonbit_decref(_M0L6resultS1222);
    return _result_4540;
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1168,
  uint32_t _M0L12ieeeExponentS1167
) {
  int32_t _M0Lm2e2S1165;
  uint64_t _M0Lm2m2S1166;
  uint64_t _M0L6_2atmpS3383;
  uint64_t _M0L6_2atmpS3382;
  int32_t _M0L4evenS1169;
  uint64_t _M0L6_2atmpS3381;
  uint64_t _M0L2mvS1170;
  int32_t _M0L7mmShiftS1171;
  uint64_t _M0Lm2vrS1172;
  uint64_t _M0Lm2vpS1173;
  uint64_t _M0Lm2vmS1174;
  int32_t _M0Lm3e10S1175;
  int32_t _M0Lm17vmIsTrailingZerosS1176;
  int32_t _M0Lm17vrIsTrailingZerosS1177;
  int32_t _M0L6_2atmpS3283;
  int32_t _M0Lm7removedS1196;
  int32_t _M0Lm16lastRemovedDigitS1197;
  uint64_t _M0Lm6outputS1198;
  int32_t _M0L6_2atmpS3379;
  int32_t _M0L6_2atmpS3380;
  int32_t _M0L3expS1221;
  uint64_t _M0L6_2atmpS3378;
  struct _M0TPB17FloatingDecimal64* _block_4546;
  #line 347 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0Lm2e2S1165 = 0;
  _M0Lm2m2S1166 = 0ull;
  if (_M0L12ieeeExponentS1167 == 0u) {
    _M0Lm2e2S1165 = -1076;
    _M0Lm2m2S1166 = _M0L12ieeeMantissaS1168;
  } else {
    int32_t _M0L6_2atmpS3282 = *(int32_t*)&_M0L12ieeeExponentS1167;
    int32_t _M0L6_2atmpS3281 = _M0L6_2atmpS3282 - 1023;
    int32_t _M0L6_2atmpS3280 = _M0L6_2atmpS3281 - 52;
    _M0Lm2e2S1165 = _M0L6_2atmpS3280 - 2;
    _M0Lm2m2S1166 = 4503599627370496ull | _M0L12ieeeMantissaS1168;
  }
  _M0L6_2atmpS3383 = _M0Lm2m2S1166;
  _M0L6_2atmpS3382 = _M0L6_2atmpS3383 & 1ull;
  _M0L4evenS1169 = _M0L6_2atmpS3382 == 0ull;
  _M0L6_2atmpS3381 = _M0Lm2m2S1166;
  _M0L2mvS1170 = 4ull * _M0L6_2atmpS3381;
  if (_M0L12ieeeMantissaS1168 != 0ull) {
    _M0L7mmShiftS1171 = 1;
  } else {
    _M0L7mmShiftS1171 = _M0L12ieeeExponentS1167 <= 1u;
  }
  _M0Lm2vrS1172 = 0ull;
  _M0Lm2vpS1173 = 0ull;
  _M0Lm2vmS1174 = 0ull;
  _M0Lm3e10S1175 = 0;
  _M0Lm17vmIsTrailingZerosS1176 = 0;
  _M0Lm17vrIsTrailingZerosS1177 = 0;
  _M0L6_2atmpS3283 = _M0Lm2e2S1165;
  if (_M0L6_2atmpS3283 >= 0) {
    int32_t _M0L6_2atmpS3305 = _M0Lm2e2S1165;
    int32_t _M0L6_2atmpS3301;
    int32_t _M0L6_2atmpS3304;
    int32_t _M0L6_2atmpS3303;
    int32_t _M0L6_2atmpS3302;
    int32_t _M0L1qS1178;
    int32_t _M0L6_2atmpS3300;
    int32_t _M0L6_2atmpS3299;
    int32_t _M0L1kS1179;
    int32_t _M0L6_2atmpS3298;
    int32_t _M0L6_2atmpS3297;
    int32_t _M0L6_2atmpS3296;
    int32_t _M0L1iS1180;
    struct _M0TPB8Pow5Pair _M0L4pow5S1181;
    uint64_t _M0L6_2atmpS3295;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1182;
    uint64_t _M0L8_2avrOutS1183;
    uint64_t _M0L8_2avpOutS1184;
    uint64_t _M0L8_2avmOutS1185;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3301 = _M0FPB9log10Pow2(_M0L6_2atmpS3305);
    _M0L6_2atmpS3304 = _M0Lm2e2S1165;
    _M0L6_2atmpS3303 = _M0L6_2atmpS3304 > 3;
    #line 383 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3302 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS3303);
    _M0L1qS1178 = _M0L6_2atmpS3301 - _M0L6_2atmpS3302;
    _M0Lm3e10S1175 = _M0L1qS1178;
    #line 385 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3300 = _M0FPB8pow5bits(_M0L1qS1178);
    _M0L6_2atmpS3299 = 125 + _M0L6_2atmpS3300;
    _M0L1kS1179 = _M0L6_2atmpS3299 - 1;
    _M0L6_2atmpS3298 = _M0Lm2e2S1165;
    _M0L6_2atmpS3297 = -_M0L6_2atmpS3298;
    _M0L6_2atmpS3296 = _M0L6_2atmpS3297 + _M0L1qS1178;
    _M0L1iS1180 = _M0L6_2atmpS3296 + _M0L1kS1179;
    #line 387 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1181 = _M0FPB22double__computeInvPow5(_M0L1qS1178);
    _M0L6_2atmpS3295 = _M0Lm2m2S1166;
    #line 388 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1182
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS3295, _M0L4pow5S1181, _M0L1iS1180, _M0L7mmShiftS1171);
    _M0L8_2avrOutS1183 = _M0L7_2abindS1182.$0;
    _M0L8_2avpOutS1184 = _M0L7_2abindS1182.$1;
    _M0L8_2avmOutS1185 = _M0L7_2abindS1182.$2;
    _M0Lm2vrS1172 = _M0L8_2avrOutS1183;
    _M0Lm2vpS1173 = _M0L8_2avpOutS1184;
    _M0Lm2vmS1174 = _M0L8_2avmOutS1185;
    if (_M0L1qS1178 <= 21) {
      int32_t _M0L6_2atmpS3291 = (int32_t)_M0L2mvS1170;
      uint64_t _M0L6_2atmpS3294 = _M0L2mvS1170 / 5ull;
      int32_t _M0L6_2atmpS3293 = (int32_t)_M0L6_2atmpS3294;
      int32_t _M0L6_2atmpS3292 = 5 * _M0L6_2atmpS3293;
      int32_t _M0L6mvMod5S1186 = _M0L6_2atmpS3291 - _M0L6_2atmpS3292;
      if (_M0L6mvMod5S1186 == 0) {
        #line 400 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1177
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1170, _M0L1qS1178);
      } else if (_M0L4evenS1169) {
        uint64_t _M0L6_2atmpS3285 = _M0L2mvS1170 - 1ull;
        uint64_t _M0L6_2atmpS3286;
        uint64_t _M0L6_2atmpS3284;
        #line 406 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3286 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1171);
        _M0L6_2atmpS3284 = _M0L6_2atmpS3285 - _M0L6_2atmpS3286;
        #line 405 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1176
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS3284, _M0L1qS1178);
      } else {
        uint64_t _M0L6_2atmpS3287 = _M0Lm2vpS1173;
        uint64_t _M0L6_2atmpS3290 = _M0L2mvS1170 + 2ull;
        int32_t _M0L6_2atmpS3289;
        uint64_t _M0L6_2atmpS3288;
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3289
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS3290, _M0L1qS1178);
        #line 410 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3288 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS3289);
        _M0Lm2vpS1173 = _M0L6_2atmpS3287 - _M0L6_2atmpS3288;
      }
    }
  } else {
    int32_t _M0L6_2atmpS3319 = _M0Lm2e2S1165;
    int32_t _M0L6_2atmpS3318 = -_M0L6_2atmpS3319;
    int32_t _M0L6_2atmpS3313;
    int32_t _M0L6_2atmpS3317;
    int32_t _M0L6_2atmpS3316;
    int32_t _M0L6_2atmpS3315;
    int32_t _M0L6_2atmpS3314;
    int32_t _M0L1qS1187;
    int32_t _M0L6_2atmpS3306;
    int32_t _M0L6_2atmpS3312;
    int32_t _M0L6_2atmpS3311;
    int32_t _M0L1iS1188;
    int32_t _M0L6_2atmpS3310;
    int32_t _M0L1kS1189;
    int32_t _M0L1jS1190;
    struct _M0TPB8Pow5Pair _M0L4pow5S1191;
    uint64_t _M0L6_2atmpS3309;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1192;
    uint64_t _M0L8_2avrOutS1193;
    uint64_t _M0L8_2avpOutS1194;
    uint64_t _M0L8_2avmOutS1195;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3313 = _M0FPB9log10Pow5(_M0L6_2atmpS3318);
    _M0L6_2atmpS3317 = _M0Lm2e2S1165;
    _M0L6_2atmpS3316 = -_M0L6_2atmpS3317;
    _M0L6_2atmpS3315 = _M0L6_2atmpS3316 > 1;
    #line 415 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3314 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS3315);
    _M0L1qS1187 = _M0L6_2atmpS3313 - _M0L6_2atmpS3314;
    _M0L6_2atmpS3306 = _M0Lm2e2S1165;
    _M0Lm3e10S1175 = _M0L1qS1187 + _M0L6_2atmpS3306;
    _M0L6_2atmpS3312 = _M0Lm2e2S1165;
    _M0L6_2atmpS3311 = -_M0L6_2atmpS3312;
    _M0L1iS1188 = _M0L6_2atmpS3311 - _M0L1qS1187;
    #line 418 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3310 = _M0FPB8pow5bits(_M0L1iS1188);
    _M0L1kS1189 = _M0L6_2atmpS3310 - 125;
    _M0L1jS1190 = _M0L1qS1187 - _M0L1kS1189;
    #line 420 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L4pow5S1191 = _M0FPB19double__computePow5(_M0L1iS1188);
    _M0L6_2atmpS3309 = _M0Lm2m2S1166;
    #line 421 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L7_2abindS1192
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS3309, _M0L4pow5S1191, _M0L1jS1190, _M0L7mmShiftS1171);
    _M0L8_2avrOutS1193 = _M0L7_2abindS1192.$0;
    _M0L8_2avpOutS1194 = _M0L7_2abindS1192.$1;
    _M0L8_2avmOutS1195 = _M0L7_2abindS1192.$2;
    _M0Lm2vrS1172 = _M0L8_2avrOutS1193;
    _M0Lm2vpS1173 = _M0L8_2avpOutS1194;
    _M0Lm2vmS1174 = _M0L8_2avmOutS1195;
    if (_M0L1qS1187 <= 1) {
      _M0Lm17vrIsTrailingZerosS1177 = 1;
      if (_M0L4evenS1169) {
        int32_t _M0L6_2atmpS3307;
        #line 432 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        _M0L6_2atmpS3307 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1171);
        _M0Lm17vmIsTrailingZerosS1176 = _M0L6_2atmpS3307 == 1;
      } else {
        uint64_t _M0L6_2atmpS3308 = _M0Lm2vpS1173;
        _M0Lm2vpS1173 = _M0L6_2atmpS3308 - 1ull;
      }
    } else if (_M0L1qS1187 < 63) {
      #line 437 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS1177
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS1170, _M0L1qS1187);
    }
  }
  _M0Lm7removedS1196 = 0;
  _M0Lm16lastRemovedDigitS1197 = 0;
  _M0Lm6outputS1198 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS1176 || _M0Lm17vrIsTrailingZerosS1177) {
    int32_t _if__result_4543;
    uint64_t _M0L6_2atmpS3349;
    uint64_t _M0L6_2atmpS3355;
    uint64_t _M0L6_2atmpS3356;
    int32_t _if__result_4544;
    int32_t _M0L6_2atmpS3352;
    int64_t _M0L6_2atmpS3351;
    uint64_t _M0L6_2atmpS3350;
    while (1) {
      uint64_t _M0L6_2atmpS3332 = _M0Lm2vpS1173;
      uint64_t _M0L7vpDiv10S1199 = _M0L6_2atmpS3332 / 10ull;
      uint64_t _M0L6_2atmpS3331 = _M0Lm2vmS1174;
      uint64_t _M0L7vmDiv10S1200 = _M0L6_2atmpS3331 / 10ull;
      uint64_t _M0L6_2atmpS3330;
      int32_t _M0L6_2atmpS3327;
      int32_t _M0L6_2atmpS3329;
      int32_t _M0L6_2atmpS3328;
      int32_t _M0L7vmMod10S1202;
      uint64_t _M0L6_2atmpS3326;
      uint64_t _M0L7vrDiv10S1203;
      uint64_t _M0L6_2atmpS3325;
      int32_t _M0L6_2atmpS3322;
      int32_t _M0L6_2atmpS3324;
      int32_t _M0L6_2atmpS3323;
      int32_t _M0L7vrMod10S1204;
      int32_t _M0L6_2atmpS3321;
      if (_M0L7vpDiv10S1199 <= _M0L7vmDiv10S1200) {
        break;
      }
      _M0L6_2atmpS3330 = _M0Lm2vmS1174;
      _M0L6_2atmpS3327 = (int32_t)_M0L6_2atmpS3330;
      _M0L6_2atmpS3329 = (int32_t)_M0L7vmDiv10S1200;
      _M0L6_2atmpS3328 = 10 * _M0L6_2atmpS3329;
      _M0L7vmMod10S1202 = _M0L6_2atmpS3327 - _M0L6_2atmpS3328;
      _M0L6_2atmpS3326 = _M0Lm2vrS1172;
      _M0L7vrDiv10S1203 = _M0L6_2atmpS3326 / 10ull;
      _M0L6_2atmpS3325 = _M0Lm2vrS1172;
      _M0L6_2atmpS3322 = (int32_t)_M0L6_2atmpS3325;
      _M0L6_2atmpS3324 = (int32_t)_M0L7vrDiv10S1203;
      _M0L6_2atmpS3323 = 10 * _M0L6_2atmpS3324;
      _M0L7vrMod10S1204 = _M0L6_2atmpS3322 - _M0L6_2atmpS3323;
      if (_M0Lm17vmIsTrailingZerosS1176) {
        _M0Lm17vmIsTrailingZerosS1176 = _M0L7vmMod10S1202 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1176 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1177) {
        int32_t _M0L6_2atmpS3320 = _M0Lm16lastRemovedDigitS1197;
        _M0Lm17vrIsTrailingZerosS1177 = _M0L6_2atmpS3320 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1177 = 0;
      }
      _M0Lm16lastRemovedDigitS1197 = _M0L7vrMod10S1204;
      _M0Lm2vrS1172 = _M0L7vrDiv10S1203;
      _M0Lm2vpS1173 = _M0L7vpDiv10S1199;
      _M0Lm2vmS1174 = _M0L7vmDiv10S1200;
      _M0L6_2atmpS3321 = _M0Lm7removedS1196;
      _M0Lm7removedS1196 = _M0L6_2atmpS3321 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1176) {
      while (1) {
        uint64_t _M0L6_2atmpS3345 = _M0Lm2vmS1174;
        uint64_t _M0L7vmDiv10S1205 = _M0L6_2atmpS3345 / 10ull;
        uint64_t _M0L6_2atmpS3344 = _M0Lm2vmS1174;
        int32_t _M0L6_2atmpS3341 = (int32_t)_M0L6_2atmpS3344;
        int32_t _M0L6_2atmpS3343 = (int32_t)_M0L7vmDiv10S1205;
        int32_t _M0L6_2atmpS3342 = 10 * _M0L6_2atmpS3343;
        int32_t _M0L7vmMod10S1206 = _M0L6_2atmpS3341 - _M0L6_2atmpS3342;
        uint64_t _M0L6_2atmpS3340;
        uint64_t _M0L7vpDiv10S1208;
        uint64_t _M0L6_2atmpS3339;
        uint64_t _M0L7vrDiv10S1209;
        uint64_t _M0L6_2atmpS3338;
        int32_t _M0L6_2atmpS3335;
        int32_t _M0L6_2atmpS3337;
        int32_t _M0L6_2atmpS3336;
        int32_t _M0L7vrMod10S1210;
        int32_t _M0L6_2atmpS3334;
        if (_M0L7vmMod10S1206 != 0) {
          break;
        }
        _M0L6_2atmpS3340 = _M0Lm2vpS1173;
        _M0L7vpDiv10S1208 = _M0L6_2atmpS3340 / 10ull;
        _M0L6_2atmpS3339 = _M0Lm2vrS1172;
        _M0L7vrDiv10S1209 = _M0L6_2atmpS3339 / 10ull;
        _M0L6_2atmpS3338 = _M0Lm2vrS1172;
        _M0L6_2atmpS3335 = (int32_t)_M0L6_2atmpS3338;
        _M0L6_2atmpS3337 = (int32_t)_M0L7vrDiv10S1209;
        _M0L6_2atmpS3336 = 10 * _M0L6_2atmpS3337;
        _M0L7vrMod10S1210 = _M0L6_2atmpS3335 - _M0L6_2atmpS3336;
        if (_M0Lm17vrIsTrailingZerosS1177) {
          int32_t _M0L6_2atmpS3333 = _M0Lm16lastRemovedDigitS1197;
          _M0Lm17vrIsTrailingZerosS1177 = _M0L6_2atmpS3333 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1177 = 0;
        }
        _M0Lm16lastRemovedDigitS1197 = _M0L7vrMod10S1210;
        _M0Lm2vrS1172 = _M0L7vrDiv10S1209;
        _M0Lm2vpS1173 = _M0L7vpDiv10S1208;
        _M0Lm2vmS1174 = _M0L7vmDiv10S1205;
        _M0L6_2atmpS3334 = _M0Lm7removedS1196;
        _M0Lm7removedS1196 = _M0L6_2atmpS3334 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1177) {
      int32_t _M0L6_2atmpS3348 = _M0Lm16lastRemovedDigitS1197;
      if (_M0L6_2atmpS3348 == 5) {
        uint64_t _M0L6_2atmpS3347 = _M0Lm2vrS1172;
        uint64_t _M0L6_2atmpS3346 = _M0L6_2atmpS3347 % 2ull;
        _if__result_4543 = _M0L6_2atmpS3346 == 0ull;
      } else {
        _if__result_4543 = 0;
      }
    } else {
      _if__result_4543 = 0;
    }
    if (_if__result_4543) {
      _M0Lm16lastRemovedDigitS1197 = 4;
    }
    _M0L6_2atmpS3349 = _M0Lm2vrS1172;
    _M0L6_2atmpS3355 = _M0Lm2vrS1172;
    _M0L6_2atmpS3356 = _M0Lm2vmS1174;
    if (_M0L6_2atmpS3355 == _M0L6_2atmpS3356) {
      if (!_M0L4evenS1169) {
        _if__result_4544 = 1;
      } else {
        int32_t _M0L6_2atmpS3354 = _M0Lm17vmIsTrailingZerosS1176;
        _if__result_4544 = !_M0L6_2atmpS3354;
      }
    } else {
      _if__result_4544 = 0;
    }
    if (_if__result_4544) {
      _M0L6_2atmpS3352 = 1;
    } else {
      int32_t _M0L6_2atmpS3353 = _M0Lm16lastRemovedDigitS1197;
      _M0L6_2atmpS3352 = _M0L6_2atmpS3353 >= 5;
    }
    #line 487 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3351 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS3352);
    _M0L6_2atmpS3350 = *(uint64_t*)&_M0L6_2atmpS3351;
    _M0Lm6outputS1198 = _M0L6_2atmpS3349 + _M0L6_2atmpS3350;
  } else {
    int32_t _M0Lm7roundUpS1211 = 0;
    uint64_t _M0L6_2atmpS3377 = _M0Lm2vpS1173;
    uint64_t _M0L8vpDiv100S1212 = _M0L6_2atmpS3377 / 100ull;
    uint64_t _M0L6_2atmpS3376 = _M0Lm2vmS1174;
    uint64_t _M0L8vmDiv100S1213 = _M0L6_2atmpS3376 / 100ull;
    uint64_t _M0L6_2atmpS3371;
    uint64_t _M0L6_2atmpS3374;
    uint64_t _M0L6_2atmpS3375;
    int32_t _M0L6_2atmpS3373;
    uint64_t _M0L6_2atmpS3372;
    if (_M0L8vpDiv100S1212 > _M0L8vmDiv100S1213) {
      uint64_t _M0L6_2atmpS3362 = _M0Lm2vrS1172;
      uint64_t _M0L8vrDiv100S1214 = _M0L6_2atmpS3362 / 100ull;
      uint64_t _M0L6_2atmpS3361 = _M0Lm2vrS1172;
      int32_t _M0L6_2atmpS3358 = (int32_t)_M0L6_2atmpS3361;
      int32_t _M0L6_2atmpS3360 = (int32_t)_M0L8vrDiv100S1214;
      int32_t _M0L6_2atmpS3359 = 100 * _M0L6_2atmpS3360;
      int32_t _M0L8vrMod100S1215 = _M0L6_2atmpS3358 - _M0L6_2atmpS3359;
      int32_t _M0L6_2atmpS3357;
      _M0Lm7roundUpS1211 = _M0L8vrMod100S1215 >= 50;
      _M0Lm2vrS1172 = _M0L8vrDiv100S1214;
      _M0Lm2vpS1173 = _M0L8vpDiv100S1212;
      _M0Lm2vmS1174 = _M0L8vmDiv100S1213;
      _M0L6_2atmpS3357 = _M0Lm7removedS1196;
      _M0Lm7removedS1196 = _M0L6_2atmpS3357 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS3370 = _M0Lm2vpS1173;
      uint64_t _M0L7vpDiv10S1216 = _M0L6_2atmpS3370 / 10ull;
      uint64_t _M0L6_2atmpS3369 = _M0Lm2vmS1174;
      uint64_t _M0L7vmDiv10S1217 = _M0L6_2atmpS3369 / 10ull;
      uint64_t _M0L6_2atmpS3368;
      uint64_t _M0L7vrDiv10S1219;
      uint64_t _M0L6_2atmpS3367;
      int32_t _M0L6_2atmpS3364;
      int32_t _M0L6_2atmpS3366;
      int32_t _M0L6_2atmpS3365;
      int32_t _M0L7vrMod10S1220;
      int32_t _M0L6_2atmpS3363;
      if (_M0L7vpDiv10S1216 <= _M0L7vmDiv10S1217) {
        break;
      }
      _M0L6_2atmpS3368 = _M0Lm2vrS1172;
      _M0L7vrDiv10S1219 = _M0L6_2atmpS3368 / 10ull;
      _M0L6_2atmpS3367 = _M0Lm2vrS1172;
      _M0L6_2atmpS3364 = (int32_t)_M0L6_2atmpS3367;
      _M0L6_2atmpS3366 = (int32_t)_M0L7vrDiv10S1219;
      _M0L6_2atmpS3365 = 10 * _M0L6_2atmpS3366;
      _M0L7vrMod10S1220 = _M0L6_2atmpS3364 - _M0L6_2atmpS3365;
      _M0Lm7roundUpS1211 = _M0L7vrMod10S1220 >= 5;
      _M0Lm2vrS1172 = _M0L7vrDiv10S1219;
      _M0Lm2vpS1173 = _M0L7vpDiv10S1216;
      _M0Lm2vmS1174 = _M0L7vmDiv10S1217;
      _M0L6_2atmpS3363 = _M0Lm7removedS1196;
      _M0Lm7removedS1196 = _M0L6_2atmpS3363 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS3371 = _M0Lm2vrS1172;
    _M0L6_2atmpS3374 = _M0Lm2vrS1172;
    _M0L6_2atmpS3375 = _M0Lm2vmS1174;
    _M0L6_2atmpS3373
    = _M0L6_2atmpS3374 == _M0L6_2atmpS3375 || _M0Lm7roundUpS1211;
    #line 522 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0L6_2atmpS3372 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS3373);
    _M0Lm6outputS1198 = _M0L6_2atmpS3371 + _M0L6_2atmpS3372;
  }
  _M0L6_2atmpS3379 = _M0Lm3e10S1175;
  _M0L6_2atmpS3380 = _M0Lm7removedS1196;
  _M0L3expS1221 = _M0L6_2atmpS3379 + _M0L6_2atmpS3380;
  _M0L6_2atmpS3378 = _M0Lm6outputS1198;
  _block_4546
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_4546)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _block_4546->$0 = _M0L6_2atmpS3378;
  _block_4546->$1 = _M0L3expS1221;
  return _block_4546;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS1164) {
  #line 110 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1164) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS1163) {
  #line 58 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1163) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS1162) {
  #line 32 "/home/developer/.moon/lib/core/builtin/bool.mbt"
  if (_M0L4selfS1162) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS1161) {
  #line 280 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L1vS1161 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS1161 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS1161 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS1161 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS1161 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS1161 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS1161 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS1161 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS1161 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS1161 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS1161 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS1161 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS1161 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS1161 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS1161 >= 100ull) {
    return 3;
  }
  if (_M0L1vS1161 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS1144) {
  int32_t _M0L6_2atmpS3279;
  int32_t _M0L6_2atmpS3278;
  int32_t _M0L4baseS1143;
  int32_t _M0L5base2S1145;
  int32_t _M0L6offsetS1146;
  int32_t _M0L6_2atmpS3277;
  uint64_t _M0L4mul0S1147;
  int32_t _M0L6_2atmpS3276;
  int32_t _M0L6_2atmpS3275;
  uint64_t _M0L4mul1S1148;
  uint64_t _M0L1mS1149;
  struct _M0TPB7Umul128 _M0L7_2abindS1150;
  uint64_t _M0L7_2alow1S1151;
  uint64_t _M0L8_2ahigh1S1152;
  struct _M0TPB7Umul128 _M0L7_2abindS1153;
  uint64_t _M0L7_2alow0S1154;
  uint64_t _M0L8_2ahigh0S1155;
  uint64_t _M0L3sumS1156;
  uint64_t _M0Lm5high1S1157;
  int32_t _M0L6_2atmpS3273;
  int32_t _M0L6_2atmpS3274;
  int32_t _M0L5deltaS1158;
  uint64_t _M0L6_2atmpS3272;
  uint64_t _M0L6_2atmpS3264;
  int32_t _M0L6_2atmpS3271;
  uint32_t _M0L6_2atmpS3268;
  int32_t _M0L6_2atmpS3270;
  int32_t _M0L6_2atmpS3269;
  uint32_t _M0L6_2atmpS3267;
  uint32_t _M0L6_2atmpS3266;
  uint64_t _M0L6_2atmpS3265;
  uint64_t _M0L1aS1159;
  uint64_t _M0L6_2atmpS3263;
  uint64_t _M0L1bS1160;
  #line 239 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3279 = _M0L1iS1144 + 26;
  _M0L6_2atmpS3278 = _M0L6_2atmpS3279 - 1;
  _M0L4baseS1143 = _M0L6_2atmpS3278 / 26;
  _M0L5base2S1145 = _M0L4baseS1143 * 26;
  _M0L6offsetS1146 = _M0L5base2S1145 - _M0L1iS1144;
  _M0L6_2atmpS3277 = _M0L4baseS1143 * 2;
  #line 243 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1147
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS3277);
  _M0L6_2atmpS3276 = _M0L4baseS1143 * 2;
  _M0L6_2atmpS3275 = _M0L6_2atmpS3276 + 1;
  #line 244 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1148
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS3275);
  if (_M0L6offsetS1146 == 0) {
    return (struct _M0TPB8Pow5Pair){.$0 = _M0L4mul0S1147,
                                      .$1 = _M0L4mul1S1148};
  }
  #line 248 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1mS1149
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1146);
  #line 249 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1150 = _M0FPB7umul128(_M0L1mS1149, _M0L4mul1S1148);
  _M0L7_2alow1S1151 = _M0L7_2abindS1150.$0;
  _M0L8_2ahigh1S1152 = _M0L7_2abindS1150.$1;
  #line 250 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1153 = _M0FPB7umul128(_M0L1mS1149, _M0L4mul0S1147);
  _M0L7_2alow0S1154 = _M0L7_2abindS1153.$0;
  _M0L8_2ahigh0S1155 = _M0L7_2abindS1153.$1;
  _M0L3sumS1156 = _M0L8_2ahigh0S1155 + _M0L7_2alow1S1151;
  _M0Lm5high1S1157 = _M0L8_2ahigh1S1152;
  if (_M0L3sumS1156 < _M0L8_2ahigh0S1155) {
    uint64_t _M0L6_2atmpS3262 = _M0Lm5high1S1157;
    _M0Lm5high1S1157 = _M0L6_2atmpS3262 + 1ull;
  }
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3273 = _M0FPB8pow5bits(_M0L5base2S1145);
  #line 256 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3274 = _M0FPB8pow5bits(_M0L1iS1144);
  _M0L5deltaS1158 = _M0L6_2atmpS3273 - _M0L6_2atmpS3274;
  #line 257 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3272
  = _M0FPB13shiftright128(_M0L7_2alow0S1154, _M0L3sumS1156, _M0L5deltaS1158);
  _M0L6_2atmpS3264 = _M0L6_2atmpS3272 + 1ull;
  _M0L6_2atmpS3271 = _M0L1iS1144 / 16;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3268
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS3271);
  _M0L6_2atmpS3270 = _M0L1iS1144 % 16;
  _M0L6_2atmpS3269 = _M0L6_2atmpS3270 << 1;
  _M0L6_2atmpS3267 = _M0L6_2atmpS3268 >> (_M0L6_2atmpS3269 & 31);
  _M0L6_2atmpS3266 = _M0L6_2atmpS3267 & 3u;
  #line 259 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3265 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS3266);
  _M0L1aS1159 = _M0L6_2atmpS3264 + _M0L6_2atmpS3265;
  _M0L6_2atmpS3263 = _M0Lm5high1S1157;
  #line 260 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1160
  = _M0FPB13shiftright128(_M0L3sumS1156, _M0L6_2atmpS3263, _M0L5deltaS1158);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1159, .$1 = _M0L1bS1160};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS1126) {
  int32_t _M0L4baseS1125;
  int32_t _M0L5base2S1127;
  int32_t _M0L6offsetS1128;
  int32_t _M0L6_2atmpS3261;
  uint64_t _M0L4mul0S1129;
  int32_t _M0L6_2atmpS3260;
  int32_t _M0L6_2atmpS3259;
  uint64_t _M0L4mul1S1130;
  uint64_t _M0L1mS1131;
  struct _M0TPB7Umul128 _M0L7_2abindS1132;
  uint64_t _M0L7_2alow1S1133;
  uint64_t _M0L8_2ahigh1S1134;
  struct _M0TPB7Umul128 _M0L7_2abindS1135;
  uint64_t _M0L7_2alow0S1136;
  uint64_t _M0L8_2ahigh0S1137;
  uint64_t _M0L3sumS1138;
  uint64_t _M0Lm5high1S1139;
  int32_t _M0L6_2atmpS3257;
  int32_t _M0L6_2atmpS3258;
  int32_t _M0L5deltaS1140;
  uint64_t _M0L6_2atmpS3249;
  int32_t _M0L6_2atmpS3256;
  uint32_t _M0L6_2atmpS3253;
  int32_t _M0L6_2atmpS3255;
  int32_t _M0L6_2atmpS3254;
  uint32_t _M0L6_2atmpS3252;
  uint32_t _M0L6_2atmpS3251;
  uint64_t _M0L6_2atmpS3250;
  uint64_t _M0L1aS1141;
  uint64_t _M0L6_2atmpS3248;
  uint64_t _M0L1bS1142;
  #line 213 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4baseS1125 = _M0L1iS1126 / 26;
  _M0L5base2S1127 = _M0L4baseS1125 * 26;
  _M0L6offsetS1128 = _M0L1iS1126 - _M0L5base2S1127;
  _M0L6_2atmpS3261 = _M0L4baseS1125 * 2;
  #line 217 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul0S1129
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS3261);
  _M0L6_2atmpS3260 = _M0L4baseS1125 * 2;
  _M0L6_2atmpS3259 = _M0L6_2atmpS3260 + 1;
  #line 218 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L4mul1S1130
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS3259);
  if (_M0L6offsetS1128 == 0) {
    return (struct _M0TPB8Pow5Pair){.$0 = _M0L4mul0S1129,
                                      .$1 = _M0L4mul1S1130};
  }
  #line 222 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1mS1131
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1128);
  #line 223 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1132 = _M0FPB7umul128(_M0L1mS1131, _M0L4mul1S1130);
  _M0L7_2alow1S1133 = _M0L7_2abindS1132.$0;
  _M0L8_2ahigh1S1134 = _M0L7_2abindS1132.$1;
  #line 224 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1135 = _M0FPB7umul128(_M0L1mS1131, _M0L4mul0S1129);
  _M0L7_2alow0S1136 = _M0L7_2abindS1135.$0;
  _M0L8_2ahigh0S1137 = _M0L7_2abindS1135.$1;
  _M0L3sumS1138 = _M0L8_2ahigh0S1137 + _M0L7_2alow1S1133;
  _M0Lm5high1S1139 = _M0L8_2ahigh1S1134;
  if (_M0L3sumS1138 < _M0L8_2ahigh0S1137) {
    uint64_t _M0L6_2atmpS3247 = _M0Lm5high1S1139;
    _M0Lm5high1S1139 = _M0L6_2atmpS3247 + 1ull;
  }
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3257 = _M0FPB8pow5bits(_M0L1iS1126);
  #line 230 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3258 = _M0FPB8pow5bits(_M0L5base2S1127);
  _M0L5deltaS1140 = _M0L6_2atmpS3257 - _M0L6_2atmpS3258;
  #line 231 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3249
  = _M0FPB13shiftright128(_M0L7_2alow0S1136, _M0L3sumS1138, _M0L5deltaS1140);
  _M0L6_2atmpS3256 = _M0L1iS1126 / 16;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3253
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS3256);
  _M0L6_2atmpS3255 = _M0L1iS1126 % 16;
  _M0L6_2atmpS3254 = _M0L6_2atmpS3255 << 1;
  _M0L6_2atmpS3252 = _M0L6_2atmpS3253 >> (_M0L6_2atmpS3254 & 31);
  _M0L6_2atmpS3251 = _M0L6_2atmpS3252 & 3u;
  #line 232 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3250 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS3251);
  _M0L1aS1141 = _M0L6_2atmpS3249 + _M0L6_2atmpS3250;
  _M0L6_2atmpS3248 = _M0Lm5high1S1139;
  #line 233 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L1bS1142
  = _M0FPB13shiftright128(_M0L3sumS1138, _M0L6_2atmpS3248, _M0L5deltaS1140);
  return (struct _M0TPB8Pow5Pair){.$0 = _M0L1aS1141, .$1 = _M0L1bS1142};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS1099,
  struct _M0TPB8Pow5Pair _M0L3mulS1096,
  int32_t _M0L1jS1112,
  int32_t _M0L7mmShiftS1114
) {
  uint64_t _M0L7_2amul0S1095;
  uint64_t _M0L7_2amul1S1097;
  uint64_t _M0L1mS1098;
  struct _M0TPB7Umul128 _M0L7_2abindS1100;
  uint64_t _M0L5_2aloS1101;
  uint64_t _M0L6_2atmpS1102;
  struct _M0TPB7Umul128 _M0L7_2abindS1103;
  uint64_t _M0L6_2alo2S1104;
  uint64_t _M0L6_2ahi2S1105;
  uint64_t _M0L3midS1106;
  uint64_t _M0L6_2atmpS3246;
  uint64_t _M0L2hiS1107;
  uint64_t _M0L3lo2S1108;
  uint64_t _M0L6_2atmpS3244;
  uint64_t _M0L6_2atmpS3245;
  uint64_t _M0L4mid2S1109;
  uint64_t _M0L6_2atmpS3243;
  uint64_t _M0L3hi2S1110;
  int32_t _M0L6_2atmpS3242;
  int32_t _M0L6_2atmpS3241;
  uint64_t _M0L2vpS1111;
  uint64_t _M0Lm2vmS1113;
  int32_t _M0L6_2atmpS3240;
  int32_t _M0L6_2atmpS3239;
  uint64_t _M0L2vrS1124;
  uint64_t _M0L6_2atmpS3238;
  #line 129 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2amul0S1095 = _M0L3mulS1096.$0;
  _M0L7_2amul1S1097 = _M0L3mulS1096.$1;
  _M0L1mS1098 = _M0L1mS1099 << 1;
  #line 137 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1100 = _M0FPB7umul128(_M0L1mS1098, _M0L7_2amul0S1095);
  _M0L5_2aloS1101 = _M0L7_2abindS1100.$0;
  _M0L6_2atmpS1102 = _M0L7_2abindS1100.$1;
  #line 138 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L7_2abindS1103 = _M0FPB7umul128(_M0L1mS1098, _M0L7_2amul1S1097);
  _M0L6_2alo2S1104 = _M0L7_2abindS1103.$0;
  _M0L6_2ahi2S1105 = _M0L7_2abindS1103.$1;
  _M0L3midS1106 = _M0L6_2atmpS1102 + _M0L6_2alo2S1104;
  if (_M0L3midS1106 < _M0L6_2atmpS1102) {
    _M0L6_2atmpS3246 = 1ull;
  } else {
    _M0L6_2atmpS3246 = 0ull;
  }
  _M0L2hiS1107 = _M0L6_2ahi2S1105 + _M0L6_2atmpS3246;
  _M0L3lo2S1108 = _M0L5_2aloS1101 + _M0L7_2amul0S1095;
  _M0L6_2atmpS3244 = _M0L3midS1106 + _M0L7_2amul1S1097;
  if (_M0L3lo2S1108 < _M0L5_2aloS1101) {
    _M0L6_2atmpS3245 = 1ull;
  } else {
    _M0L6_2atmpS3245 = 0ull;
  }
  _M0L4mid2S1109 = _M0L6_2atmpS3244 + _M0L6_2atmpS3245;
  if (_M0L4mid2S1109 < _M0L3midS1106) {
    _M0L6_2atmpS3243 = 1ull;
  } else {
    _M0L6_2atmpS3243 = 0ull;
  }
  _M0L3hi2S1110 = _M0L2hiS1107 + _M0L6_2atmpS3243;
  _M0L6_2atmpS3242 = _M0L1jS1112 - 64;
  _M0L6_2atmpS3241 = _M0L6_2atmpS3242 - 1;
  #line 144 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vpS1111
  = _M0FPB13shiftright128(_M0L4mid2S1109, _M0L3hi2S1110, _M0L6_2atmpS3241);
  _M0Lm2vmS1113 = 0ull;
  if (_M0L7mmShiftS1114) {
    uint64_t _M0L3lo3S1115 = _M0L5_2aloS1101 - _M0L7_2amul0S1095;
    uint64_t _M0L6_2atmpS3228 = _M0L3midS1106 - _M0L7_2amul1S1097;
    uint64_t _M0L6_2atmpS3229;
    uint64_t _M0L4mid3S1116;
    uint64_t _M0L6_2atmpS3227;
    uint64_t _M0L3hi3S1117;
    int32_t _M0L6_2atmpS3226;
    int32_t _M0L6_2atmpS3225;
    if (_M0L5_2aloS1101 < _M0L3lo3S1115) {
      _M0L6_2atmpS3229 = 1ull;
    } else {
      _M0L6_2atmpS3229 = 0ull;
    }
    _M0L4mid3S1116 = _M0L6_2atmpS3228 - _M0L6_2atmpS3229;
    if (_M0L3midS1106 < _M0L4mid3S1116) {
      _M0L6_2atmpS3227 = 1ull;
    } else {
      _M0L6_2atmpS3227 = 0ull;
    }
    _M0L3hi3S1117 = _M0L2hiS1107 - _M0L6_2atmpS3227;
    _M0L6_2atmpS3226 = _M0L1jS1112 - 64;
    _M0L6_2atmpS3225 = _M0L6_2atmpS3226 - 1;
    #line 150 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1113
    = _M0FPB13shiftright128(_M0L4mid3S1116, _M0L3hi3S1117, _M0L6_2atmpS3225);
  } else {
    uint64_t _M0L3lo3S1118 = _M0L5_2aloS1101 + _M0L5_2aloS1101;
    uint64_t _M0L6_2atmpS3236 = _M0L3midS1106 + _M0L3midS1106;
    uint64_t _M0L6_2atmpS3237;
    uint64_t _M0L4mid3S1119;
    uint64_t _M0L6_2atmpS3234;
    uint64_t _M0L6_2atmpS3235;
    uint64_t _M0L3hi3S1120;
    uint64_t _M0L3lo4S1121;
    uint64_t _M0L6_2atmpS3232;
    uint64_t _M0L6_2atmpS3233;
    uint64_t _M0L4mid4S1122;
    uint64_t _M0L6_2atmpS3231;
    uint64_t _M0L3hi4S1123;
    int32_t _M0L6_2atmpS3230;
    if (_M0L3lo3S1118 < _M0L5_2aloS1101) {
      _M0L6_2atmpS3237 = 1ull;
    } else {
      _M0L6_2atmpS3237 = 0ull;
    }
    _M0L4mid3S1119 = _M0L6_2atmpS3236 + _M0L6_2atmpS3237;
    _M0L6_2atmpS3234 = _M0L2hiS1107 + _M0L2hiS1107;
    if (_M0L4mid3S1119 < _M0L3midS1106) {
      _M0L6_2atmpS3235 = 1ull;
    } else {
      _M0L6_2atmpS3235 = 0ull;
    }
    _M0L3hi3S1120 = _M0L6_2atmpS3234 + _M0L6_2atmpS3235;
    _M0L3lo4S1121 = _M0L3lo3S1118 - _M0L7_2amul0S1095;
    _M0L6_2atmpS3232 = _M0L4mid3S1119 - _M0L7_2amul1S1097;
    if (_M0L3lo3S1118 < _M0L3lo4S1121) {
      _M0L6_2atmpS3233 = 1ull;
    } else {
      _M0L6_2atmpS3233 = 0ull;
    }
    _M0L4mid4S1122 = _M0L6_2atmpS3232 - _M0L6_2atmpS3233;
    if (_M0L4mid3S1119 < _M0L4mid4S1122) {
      _M0L6_2atmpS3231 = 1ull;
    } else {
      _M0L6_2atmpS3231 = 0ull;
    }
    _M0L3hi4S1123 = _M0L3hi3S1120 - _M0L6_2atmpS3231;
    _M0L6_2atmpS3230 = _M0L1jS1112 - 64;
    #line 158 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _M0Lm2vmS1113
    = _M0FPB13shiftright128(_M0L4mid4S1122, _M0L3hi4S1123, _M0L6_2atmpS3230);
  }
  _M0L6_2atmpS3240 = _M0L1jS1112 - 64;
  _M0L6_2atmpS3239 = _M0L6_2atmpS3240 - 1;
  #line 160 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L2vrS1124
  = _M0FPB13shiftright128(_M0L3midS1106, _M0L2hiS1107, _M0L6_2atmpS3239);
  _M0L6_2atmpS3238 = _M0Lm2vmS1113;
  return (struct _M0TPB19MulShiftAll64Result){.$0 = _M0L2vrS1124,
                                                .$1 = _M0L2vpS1111,
                                                .$2 = _M0L6_2atmpS3238};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS1093,
  int32_t _M0L1pS1094
) {
  uint64_t _M0L6_2atmpS3224;
  uint64_t _M0L6_2atmpS3223;
  uint64_t _M0L6_2atmpS3222;
  #line 124 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3224 = 1ull << (_M0L1pS1094 & 63);
  _M0L6_2atmpS3223 = _M0L6_2atmpS3224 - 1ull;
  _M0L6_2atmpS3222 = _M0L5valueS1093 & _M0L6_2atmpS3223;
  return _M0L6_2atmpS3222 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS1091,
  int32_t _M0L1pS1092
) {
  int32_t _M0L6_2atmpS3221;
  #line 119 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  #line 120 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3221 = _M0FPB10pow5Factor(_M0L5valueS1091);
  return _M0L6_2atmpS3221 >= _M0L1pS1092;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS1086) {
  uint64_t _M0L6_2atmpS3212;
  uint64_t _M0L6_2atmpS3213;
  uint64_t _M0L6_2atmpS3214;
  uint64_t _M0L6_2atmpS3215;
  uint64_t _M0L6_2atmpS3220;
  int32_t _M0L5countS1087;
  uint64_t _M0L1vS1088;
  #line 94 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3212 = _M0L5valueS1086 % 5ull;
  if (_M0L6_2atmpS3212 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3213 = _M0L5valueS1086 % 25ull;
  if (_M0L6_2atmpS3213 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS3214 = _M0L5valueS1086 % 125ull;
  if (_M0L6_2atmpS3214 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS3215 = _M0L5valueS1086 % 625ull;
  if (_M0L6_2atmpS3215 != 0ull) {
    return 3;
  }
  _M0L6_2atmpS3220 = _M0L5valueS1086 / 625ull;
  _M0L5countS1087 = 4;
  _M0L1vS1088 = _M0L6_2atmpS3220;
  while (1) {
    if (_M0L1vS1088 > 0ull) {
      uint64_t _M0L6_2atmpS3216 = _M0L1vS1088 % 5ull;
      int32_t _M0L6_2atmpS3217;
      uint64_t _M0L6_2atmpS3218;
      if (_M0L6_2atmpS3216 != 0ull) {
        return _M0L5countS1087;
      }
      _M0L6_2atmpS3217 = _M0L5countS1087 + 1;
      _M0L6_2atmpS3218 = _M0L1vS1088 / 5ull;
      _M0L5countS1087 = _M0L6_2atmpS3217;
      _M0L1vS1088 = _M0L6_2atmpS3218;
      continue;
    } else {
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1090;
      moonbit_string_t _M0L6_2atmpS3219;
      int32_t _result_4548;
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L18_2astring__builderS1090
      = _M0MPB13StringBuilder21StringBuilder_2einner(25);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1090, (moonbit_string_t)moonbit_string_literal_97.data);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0MPB13StringBuilder13write__objectGmE(_M0L18_2astring__builderS1090, _M0L5valueS1086);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _M0L6_2atmpS3219
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1090);
      moonbit_decref(_M0L18_2astring__builderS1090);
      #line 114 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
      _result_4548 = _M0FPC15abort5abortGiE(_M0L6_2atmpS3219);
      moonbit_decref(_M0L6_2atmpS3219);
      return _result_4548;
    }
    break;
  }
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS1085,
  uint64_t _M0L2hiS1083,
  int32_t _M0L4distS1084
) {
  int32_t _M0L6_2atmpS3211;
  uint64_t _M0L6_2atmpS3209;
  uint64_t _M0L6_2atmpS3210;
  #line 89 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3211 = 64 - _M0L4distS1084;
  _M0L6_2atmpS3209 = _M0L2hiS1083 << (_M0L6_2atmpS3211 & 63);
  _M0L6_2atmpS3210 = _M0L2loS1085 >> (_M0L4distS1084 & 63);
  return _M0L6_2atmpS3209 | _M0L6_2atmpS3210;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS1073,
  uint64_t _M0L1bS1076
) {
  uint64_t _M0L3aLoS1072;
  uint64_t _M0L3aHiS1074;
  uint64_t _M0L3bLoS1075;
  uint64_t _M0L3bHiS1077;
  uint64_t _M0L1xS1078;
  uint64_t _M0L6_2atmpS3207;
  uint64_t _M0L6_2atmpS3208;
  uint64_t _M0L1yS1079;
  uint64_t _M0L6_2atmpS3205;
  uint64_t _M0L6_2atmpS3206;
  uint64_t _M0L1zS1080;
  uint64_t _M0L6_2atmpS3203;
  uint64_t _M0L6_2atmpS3204;
  uint64_t _M0L6_2atmpS3201;
  uint64_t _M0L6_2atmpS3202;
  uint64_t _M0L1wS1081;
  uint64_t _M0L2loS1082;
  #line 74 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3aLoS1072 = _M0L1aS1073 & 4294967295ull;
  _M0L3aHiS1074 = _M0L1aS1073 >> 32;
  _M0L3bLoS1075 = _M0L1bS1076 & 4294967295ull;
  _M0L3bHiS1077 = _M0L1bS1076 >> 32;
  _M0L1xS1078 = _M0L3aLoS1072 * _M0L3bLoS1075;
  _M0L6_2atmpS3207 = _M0L3aHiS1074 * _M0L3bLoS1075;
  _M0L6_2atmpS3208 = _M0L1xS1078 >> 32;
  _M0L1yS1079 = _M0L6_2atmpS3207 + _M0L6_2atmpS3208;
  _M0L6_2atmpS3205 = _M0L3aLoS1072 * _M0L3bHiS1077;
  _M0L6_2atmpS3206 = _M0L1yS1079 & 4294967295ull;
  _M0L1zS1080 = _M0L6_2atmpS3205 + _M0L6_2atmpS3206;
  _M0L6_2atmpS3203 = _M0L3aHiS1074 * _M0L3bHiS1077;
  _M0L6_2atmpS3204 = _M0L1yS1079 >> 32;
  _M0L6_2atmpS3201 = _M0L6_2atmpS3203 + _M0L6_2atmpS3204;
  _M0L6_2atmpS3202 = _M0L1zS1080 >> 32;
  _M0L1wS1081 = _M0L6_2atmpS3201 + _M0L6_2atmpS3202;
  _M0L2loS1082 = _M0L1aS1073 * _M0L1bS1076;
  return (struct _M0TPB7Umul128){.$0 = _M0L2loS1082, .$1 = _M0L1wS1081};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS1070,
  int32_t _M0L4fromS1067,
  int32_t _M0L2toS1066
) {
  int32_t _M0L3lenS1065;
  int32_t _M0L6_2atmpS3200;
  uint16_t* _M0L6bufferS1068;
  int32_t _M0L1iS1069;
  #line 52 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L3lenS1065 = _M0L2toS1066 - _M0L4fromS1067;
  #line 54 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3200 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L6bufferS1068
  = (uint16_t*)moonbit_make_string(_M0L3lenS1065, _M0L6_2atmpS3200);
  _M0L1iS1069 = 0;
  while (1) {
    if (_M0L1iS1069 < _M0L3lenS1065) {
      int32_t _M0L6_2atmpS3198 = _M0L4fromS1067 + _M0L1iS1069;
      int32_t _M0L6_2atmpS3197;
      int32_t _M0L6_2atmpS3196;
      int32_t _M0L6_2atmpS3199;
      if (
        _M0L6_2atmpS3198 < 0
        || _M0L6_2atmpS3198 >= Moonbit_array_length(_M0L5bytesS1070)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3197 = (int32_t)_M0L5bytesS1070[_M0L6_2atmpS3198];
      _M0L6_2atmpS3196 = (uint16_t)_M0L6_2atmpS3197;
      if (
        _M0L1iS1069 < 0
        || _M0L1iS1069 >= Moonbit_array_length(_M0L6bufferS1068)
      ) {
        #line 56 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6bufferS1068[_M0L1iS1069] = _M0L6_2atmpS3196;
      _M0L6_2atmpS3199 = _M0L1iS1069 + 1;
      _M0L1iS1069 = _M0L6_2atmpS3199;
      continue;
    }
    break;
  }
  return _M0L6bufferS1068;
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS1064) {
  int32_t _M0L6_2atmpS3195;
  uint32_t _M0L6_2atmpS3194;
  uint32_t _M0L6_2atmpS3193;
  #line 44 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3195 = _M0L1eS1064 * 78913;
  _M0L6_2atmpS3194 = *(uint32_t*)&_M0L6_2atmpS3195;
  _M0L6_2atmpS3193 = _M0L6_2atmpS3194 >> 18;
  return *(int32_t*)&_M0L6_2atmpS3193;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS1063) {
  int32_t _M0L6_2atmpS3192;
  uint32_t _M0L6_2atmpS3191;
  uint32_t _M0L6_2atmpS3190;
  #line 37 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3192 = _M0L1eS1063 * 732923;
  _M0L6_2atmpS3191 = *(uint32_t*)&_M0L6_2atmpS3192;
  _M0L6_2atmpS3190 = _M0L6_2atmpS3191 >> 20;
  return *(int32_t*)&_M0L6_2atmpS3190;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS1061,
  int32_t _M0L8exponentS1062,
  int32_t _M0L8mantissaS1059
) {
  moonbit_string_t _M0L1sS1060;
  moonbit_string_t _result_4551;
  #line 23 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  if (_M0L8mantissaS1059) {
    return (moonbit_string_t)moonbit_string_literal_98.data;
  }
  if (_M0L4signS1061) {
    _M0L1sS1060 = (moonbit_string_t)moonbit_string_literal_95.data;
  } else {
    _M0L1sS1060 = (moonbit_string_t)moonbit_string_literal_96.data;
  }
  if (_M0L8exponentS1062) {
    moonbit_string_t _result_4550;
    #line 29 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
    _result_4550
    = moonbit_add_string(_M0L1sS1060, (moonbit_string_t)moonbit_string_literal_99.data);
    moonbit_decref(_M0L1sS1060);
    return _result_4550;
  }
  #line 31 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _result_4551
  = moonbit_add_string(_M0L1sS1060, (moonbit_string_t)moonbit_string_literal_100.data);
  moonbit_decref(_M0L1sS1060);
  return _result_4551;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS1058) {
  int32_t _M0L6_2atmpS3189;
  uint32_t _M0L6_2atmpS3188;
  uint32_t _M0L6_2atmpS3187;
  int32_t _M0L6_2atmpS3186;
  #line 18 "/home/developer/.moon/lib/core/builtin/double_ryu_nonjs.mbt"
  _M0L6_2atmpS3189 = _M0L1eS1058 * 1217359;
  _M0L6_2atmpS3188 = *(uint32_t*)&_M0L6_2atmpS3189;
  _M0L6_2atmpS3187 = _M0L6_2atmpS3188 >> 19;
  _M0L6_2atmpS3186 = *(int32_t*)&_M0L6_2atmpS3187;
  return _M0L6_2atmpS3186 + 1;
}

int32_t _M0IPC16string6StringPB4Hash4hash(moonbit_string_t _M0L4selfS1054) {
  int32_t _tmp_4552;
  uint32_t _M0L6_2atmpS3185;
  uint32_t _M0Lm3accS1052;
  int32_t _M0L7_2abindS1053;
  int32_t _M0L1iS1055;
  uint32_t _M0L6_2atmpS3184;
  #line 522 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _tmp_4552 = 0;
  _M0L6_2atmpS3185 = *(uint32_t*)&_tmp_4552;
  _M0Lm3accS1052 = _M0L6_2atmpS3185 + 374761393u;
  _M0L7_2abindS1053 = Moonbit_array_length(_M0L4selfS1054);
  _M0L1iS1055 = 0;
  while (1) {
    if (_M0L1iS1055 < _M0L7_2abindS1053) {
      uint32_t _M0L6_2atmpS3179 = _M0Lm3accS1052;
      int32_t _M0L6_2atmpS3182;
      int32_t _M0L6_2atmpS3181;
      uint32_t _M0L1vS1056;
      uint32_t _M0L6_2atmpS3180;
      int32_t _M0L6_2atmpS3183;
      _M0Lm3accS1052 = _M0L6_2atmpS3179 + 4u;
      _M0L6_2atmpS3182 = _M0L4selfS1054[_M0L1iS1055];
      _M0L6_2atmpS3181 = (int32_t)_M0L6_2atmpS3182;
      _M0L1vS1056 = *(uint32_t*)&_M0L6_2atmpS3181;
      _M0L6_2atmpS3180 = _M0Lm3accS1052;
      #line 527 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
      _M0Lm3accS1052 = _M0FPB13consume4__acc(_M0L6_2atmpS3180, _M0L1vS1056);
      _M0L6_2atmpS3183 = _M0L1iS1055 + 1;
      _M0L1iS1055 = _M0L6_2atmpS3183;
      continue;
    }
    break;
  }
  _M0L6_2atmpS3184 = _M0Lm3accS1052;
  #line 529 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  return _M0FPB13finalize__acc(_M0L6_2atmpS3184);
}

struct _M0TUssE* _M0MPB5Iter24nextGssE(
  struct _M0TPB4IterGUssEE* _M0L4selfS1048
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUssEE(_M0L4selfS1048);
}

struct _M0TUsbE* _M0MPB5Iter24nextGsbE(
  struct _M0TPB4IterGUsbEE* _M0L4selfS1049
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsbEE(_M0L4selfS1049);
}

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB5Iter24nextGsRP19moonbitDB10RedisValueE(
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0L4selfS1050
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsRP19moonbitDB10RedisValueEE(_M0L4selfS1050);
}

struct _M0TUsfE* _M0MPB5Iter24nextGsfE(
  struct _M0TPB4IterGUsfEE* _M0L4selfS1051
) {
  #line 1099 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  #line 1100 "/home/developer/.moon/lib/core/builtin/iterator.mbt"
  return _M0MPB4Iter4nextGUsfEE(_M0L4selfS1051);
}

struct _M0TPB4IterGUssEE* _M0MPB3Map5iter2GssE(
  struct _M0TPB3MapGssE* _M0L4selfS1044
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGssE(_M0L4selfS1044);
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map5iter2GsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1045
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsbE(_M0L4selfS1045);
}

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB3Map5iter2GsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS1046
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsRP19moonbitDB10RedisValueE(_M0L4selfS1046);
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map5iter2GsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1047
) {
  #line 725 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 727 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0MPB3Map4iterGsfE(_M0L4selfS1047);
}

struct _M0TPB4IterGUssEE* _M0MPB3Map4iterGssE(
  struct _M0TPB3MapGssE* _M0L4selfS1001
) {
  struct _M0TPB5EntryGssE* _M0L4headS3148;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS1000;
  int32_t _M0L3lenS1002;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1003;
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3141__l711__* _closure_4554;
  struct _M0TWEOUssE* _M0L6_2atmpS3139;
  int64_t _M0L6_2atmpS3140;
  struct _M0TPB4IterGUssEE* _result_4555;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3148 = _M0L4selfS1001->$5;
  if (_M0L4headS3148) {
    moonbit_incref(_M0L4headS3148);
  }
  _M0L11curr__entryS1000
  = (struct _M0TPB8MutLocalGORPB5EntryGssEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGssEE));
  Moonbit_object_header(_M0L11curr__entryS1000)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 39, 0);
  _M0L11curr__entryS1000->$0 = _M0L4headS3148;
  _M0L3lenS1002 = _M0L4selfS1001->$1;
  _M0L9remainingS1003
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1003)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1003->$0 = _M0L3lenS1002;
  _closure_4554
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3141__l711__*)moonbit_malloc(sizeof(struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3141__l711__));
  Moonbit_object_header(_closure_4554)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 42, 0);
  _closure_4554->code = &_M0MPB3Map4iterGssEC3141l711;
  _closure_4554->$0 = _M0L9remainingS1003;
  _closure_4554->$1 = _M0L11curr__entryS1000;
  _M0L6_2atmpS3139 = (struct _M0TWEOUssE*)_closure_4554;
  _M0L6_2atmpS3140 = (int64_t)_M0L3lenS1002;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4555 = _M0MPB4Iter3newGUssEE(_M0L6_2atmpS3139, _M0L6_2atmpS3140);
  moonbit_decref(_M0L6_2atmpS3139);
  return _result_4555;
}

struct _M0TPB4IterGUsbEE* _M0MPB3Map4iterGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS1012
) {
  struct _M0TPB5EntryGsbE* _M0L4headS3158;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS1011;
  int32_t _M0L3lenS1013;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1014;
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3151__l711__* _closure_4556;
  struct _M0TWEOUsbE* _M0L6_2atmpS3149;
  int64_t _M0L6_2atmpS3150;
  struct _M0TPB4IterGUsbEE* _result_4557;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3158 = _M0L4selfS1012->$5;
  if (_M0L4headS3158) {
    moonbit_incref(_M0L4headS3158);
  }
  _M0L11curr__entryS1011
  = (struct _M0TPB8MutLocalGORPB5EntryGsbEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsbEE));
  Moonbit_object_header(_M0L11curr__entryS1011)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 46, 0);
  _M0L11curr__entryS1011->$0 = _M0L4headS3158;
  _M0L3lenS1013 = _M0L4selfS1012->$1;
  _M0L9remainingS1014
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1014)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1014->$0 = _M0L3lenS1013;
  _closure_4556
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3151__l711__*)moonbit_malloc(sizeof(struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3151__l711__));
  Moonbit_object_header(_closure_4556)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 49, 0);
  _closure_4556->code = &_M0MPB3Map4iterGsbEC3151l711;
  _closure_4556->$0 = _M0L9remainingS1014;
  _closure_4556->$1 = _M0L11curr__entryS1011;
  _M0L6_2atmpS3149 = (struct _M0TWEOUsbE*)_closure_4556;
  _M0L6_2atmpS3150 = (int64_t)_M0L3lenS1013;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4557 = _M0MPB4Iter3newGUsbEE(_M0L6_2atmpS3149, _M0L6_2atmpS3150);
  moonbit_decref(_M0L6_2atmpS3149);
  return _result_4557;
}

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB3Map4iterGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS1023
) {
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4headS3168;
  struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE* _M0L11curr__entryS1022;
  int32_t _M0L3lenS1024;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1025;
  struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3161__l711__* _closure_4558;
  struct _M0TWEOUsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3159;
  int64_t _M0L6_2atmpS3160;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _result_4559;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3168 = _M0L4selfS1023->$5;
  if (_M0L4headS3168) {
    moonbit_incref(_M0L4headS3168);
  }
  _M0L11curr__entryS1022
  = (struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE));
  Moonbit_object_header(_M0L11curr__entryS1022)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 53, 0);
  _M0L11curr__entryS1022->$0 = _M0L4headS3168;
  _M0L3lenS1024 = _M0L4selfS1023->$1;
  _M0L9remainingS1025
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1025)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1025->$0 = _M0L3lenS1024;
  _closure_4558
  = (struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3161__l711__*)moonbit_malloc(sizeof(struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3161__l711__));
  Moonbit_object_header(_closure_4558)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 56, 0);
  _closure_4558->code = &_M0MPB3Map4iterGsRP19moonbitDB10RedisValueEC3161l711;
  _closure_4558->$0 = _M0L9remainingS1025;
  _closure_4558->$1 = _M0L11curr__entryS1022;
  _M0L6_2atmpS3159
  = (struct _M0TWEOUsRP19moonbitDB10RedisValueE*)_closure_4558;
  _M0L6_2atmpS3160 = (int64_t)_M0L3lenS1024;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4559
  = _M0MPB4Iter3newGUsRP19moonbitDB10RedisValueEE(_M0L6_2atmpS3159, _M0L6_2atmpS3160);
  moonbit_decref(_M0L6_2atmpS3159);
  return _result_4559;
}

struct _M0TPB4IterGUsfEE* _M0MPB3Map4iterGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS1034
) {
  struct _M0TPB5EntryGsfE* _M0L4headS3178;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS1033;
  int32_t _M0L3lenS1035;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1036;
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3171__l711__* _closure_4560;
  struct _M0TWEOUsfE* _M0L6_2atmpS3169;
  int64_t _M0L6_2atmpS3170;
  struct _M0TPB4IterGUsfEE* _result_4561;
  #line 705 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4headS3178 = _M0L4selfS1034->$5;
  if (_M0L4headS3178) {
    moonbit_incref(_M0L4headS3178);
  }
  _M0L11curr__entryS1033
  = (struct _M0TPB8MutLocalGORPB5EntryGsfEE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGORPB5EntryGsfEE));
  Moonbit_object_header(_M0L11curr__entryS1033)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 60, 0);
  _M0L11curr__entryS1033->$0 = _M0L4headS3178;
  _M0L3lenS1035 = _M0L4selfS1034->$1;
  _M0L9remainingS1036
  = (struct _M0TPB8MutLocalGiE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGiE));
  Moonbit_object_header(_M0L9remainingS1036)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L9remainingS1036->$0 = _M0L3lenS1035;
  _closure_4560
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3171__l711__*)moonbit_malloc(sizeof(struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3171__l711__));
  Moonbit_object_header(_closure_4560)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 63, 0);
  _closure_4560->code = &_M0MPB3Map4iterGsfEC3171l711;
  _closure_4560->$0 = _M0L9remainingS1036;
  _closure_4560->$1 = _M0L11curr__entryS1033;
  _M0L6_2atmpS3169 = (struct _M0TWEOUsfE*)_closure_4560;
  _M0L6_2atmpS3170 = (int64_t)_M0L3lenS1035;
  #line 710 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _result_4561 = _M0MPB4Iter3newGUsfEE(_M0L6_2atmpS3169, _M0L6_2atmpS3170);
  moonbit_decref(_M0L6_2atmpS3169);
  return _result_4561;
}

struct _M0TUsfE* _M0MPB3Map4iterGsfEC3171l711(
  struct _M0TWEOUsfE* _M0L6_2aenvS3172
) {
  struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3171__l711__* _M0L14_2acasted__envS3173;
  struct _M0TPB8MutLocalGORPB5EntryGsfEE* _M0L11curr__entryS1033;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1036;
  int32_t _M0L3valS3174;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3173
  = (struct _M0R64Map_3a_3aiter_7c_5bString_2c_20Float_5d_7c_2eanon__u3171__l711__*)_M0L6_2aenvS3172;
  _M0L11curr__entryS1033 = _M0L14_2acasted__envS3173->$1;
  _M0L9remainingS1036 = _M0L14_2acasted__envS3173->$0;
  _M0L3valS3174 = _M0L9remainingS1036->$0;
  if (_M0L3valS3174 > 0) {
    struct _M0TPB5EntryGsfE* _M0L7_2abindS1038 = _M0L11curr__entryS1033->$0;
    if (_M0L7_2abindS1038 == 0) {
      goto join_1037;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS1039 = _M0L7_2abindS1038;
      struct _M0TPB5EntryGsfE* _M0L4_2axS1040 = _M0L7_2aSomeS1039;
      moonbit_string_t _M0L6_2akeyS1041 = _M0L4_2axS1040->$4;
      float _M0L8_2avalueS1042 = _M0L4_2axS1040->$5;
      struct _M0TPB5EntryGsfE* _M0L7_2anextS1043 = _M0L4_2axS1040->$1;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4011 = _M0L11curr__entryS1033->$0;
      int32_t _M0L3valS3176;
      int32_t _M0L6_2atmpS3175;
      struct _M0TUsfE* _M0L8_2atupleS3177;
      if (_M0L7_2anextS1043) {
        moonbit_incref(_M0L7_2anextS1043);
      }
      moonbit_incref(_M0L6_2akeyS1041);
      if (_M0L6_2aoldS4011) {
        moonbit_decref(_M0L6_2aoldS4011);
      }
      _M0L11curr__entryS1033->$0 = _M0L7_2anextS1043;
      _M0L3valS3176 = _M0L9remainingS1036->$0;
      _M0L6_2atmpS3175 = _M0L3valS3176 - 1;
      _M0L9remainingS1036->$0 = _M0L6_2atmpS3175;
      _M0L8_2atupleS3177
      = (struct _M0TUsfE*)moonbit_malloc(sizeof(struct _M0TUsfE));
      Moonbit_object_header(_M0L8_2atupleS3177)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 10, 0);
      _M0L8_2atupleS3177->$0 = _M0L6_2akeyS1041;
      _M0L8_2atupleS3177->$1 = _M0L8_2avalueS1042;
      return _M0L8_2atupleS3177;
    }
  } else {
    goto join_1037;
  }
  join_1037:;
  return 0;
}

struct _M0TUsRP19moonbitDB10RedisValueE* _M0MPB3Map4iterGsRP19moonbitDB10RedisValueEC3161l711(
  struct _M0TWEOUsRP19moonbitDB10RedisValueE* _M0L6_2aenvS3162
) {
  struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3161__l711__* _M0L14_2acasted__envS3163;
  struct _M0TPB8MutLocalGORPB5EntryGsRP19moonbitDB10RedisValueEE* _M0L11curr__entryS1022;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1025;
  int32_t _M0L3valS3164;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3163
  = (struct _M0R81Map_3a_3aiter_7c_5bString_2c_20moonbitDB_2fRedisValue_5d_7c_2eanon__u3161__l711__*)_M0L6_2aenvS3162;
  _M0L11curr__entryS1022 = _M0L14_2acasted__envS3163->$1;
  _M0L9remainingS1025 = _M0L14_2acasted__envS3163->$0;
  _M0L3valS3164 = _M0L9remainingS1025->$0;
  if (_M0L3valS3164 > 0) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS1027 =
      _M0L11curr__entryS1022->$0;
    if (_M0L7_2abindS1027 == 0) {
      goto join_1026;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS1028 =
        _M0L7_2abindS1027;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4_2axS1029 =
        _M0L7_2aSomeS1028;
      moonbit_string_t _M0L6_2akeyS1030 = _M0L4_2axS1029->$4;
      void* _M0L8_2avalueS1031 = _M0L4_2axS1029->$5;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2anextS1032 =
        _M0L4_2axS1029->$1;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4015 =
        _M0L11curr__entryS1022->$0;
      int32_t _M0L3valS3166;
      int32_t _M0L6_2atmpS3165;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L8_2atupleS3167;
      if (_M0L7_2anextS1032) {
        moonbit_incref(_M0L7_2anextS1032);
      }
      moonbit_incref(_M0L8_2avalueS1031);
      moonbit_incref(_M0L6_2akeyS1030);
      if (_M0L6_2aoldS4015) {
        moonbit_decref(_M0L6_2aoldS4015);
      }
      _M0L11curr__entryS1022->$0 = _M0L7_2anextS1032;
      _M0L3valS3166 = _M0L9remainingS1025->$0;
      _M0L6_2atmpS3165 = _M0L3valS3166 - 1;
      _M0L9remainingS1025->$0 = _M0L6_2atmpS3165;
      _M0L8_2atupleS3167
      = (struct _M0TUsRP19moonbitDB10RedisValueE*)moonbit_malloc(sizeof(struct _M0TUsRP19moonbitDB10RedisValueE));
      Moonbit_object_header(_M0L8_2atupleS3167)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 67, 0);
      _M0L8_2atupleS3167->$0 = _M0L6_2akeyS1030;
      _M0L8_2atupleS3167->$1 = _M0L8_2avalueS1031;
      return _M0L8_2atupleS3167;
    }
  } else {
    goto join_1026;
  }
  join_1026:;
  return 0;
}

struct _M0TUsbE* _M0MPB3Map4iterGsbEC3151l711(
  struct _M0TWEOUsbE* _M0L6_2aenvS3152
) {
  struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3151__l711__* _M0L14_2acasted__envS3153;
  struct _M0TPB8MutLocalGORPB5EntryGsbEE* _M0L11curr__entryS1011;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1014;
  int32_t _M0L3valS3154;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3153
  = (struct _M0R63Map_3a_3aiter_7c_5bString_2c_20Bool_5d_7c_2eanon__u3151__l711__*)_M0L6_2aenvS3152;
  _M0L11curr__entryS1011 = _M0L14_2acasted__envS3153->$1;
  _M0L9remainingS1014 = _M0L14_2acasted__envS3153->$0;
  _M0L3valS3154 = _M0L9remainingS1014->$0;
  if (_M0L3valS3154 > 0) {
    struct _M0TPB5EntryGsbE* _M0L7_2abindS1016 = _M0L11curr__entryS1011->$0;
    if (_M0L7_2abindS1016 == 0) {
      goto join_1015;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS1017 = _M0L7_2abindS1016;
      struct _M0TPB5EntryGsbE* _M0L4_2axS1018 = _M0L7_2aSomeS1017;
      moonbit_string_t _M0L6_2akeyS1019 = _M0L4_2axS1018->$4;
      int32_t _M0L8_2avalueS1020 = _M0L4_2axS1018->$5;
      struct _M0TPB5EntryGsbE* _M0L7_2anextS1021 = _M0L4_2axS1018->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4020 = _M0L11curr__entryS1011->$0;
      int32_t _M0L3valS3156;
      int32_t _M0L6_2atmpS3155;
      struct _M0TUsbE* _M0L8_2atupleS3157;
      if (_M0L7_2anextS1021) {
        moonbit_incref(_M0L7_2anextS1021);
      }
      moonbit_incref(_M0L6_2akeyS1019);
      if (_M0L6_2aoldS4020) {
        moonbit_decref(_M0L6_2aoldS4020);
      }
      _M0L11curr__entryS1011->$0 = _M0L7_2anextS1021;
      _M0L3valS3156 = _M0L9remainingS1014->$0;
      _M0L6_2atmpS3155 = _M0L3valS3156 - 1;
      _M0L9remainingS1014->$0 = _M0L6_2atmpS3155;
      _M0L8_2atupleS3157
      = (struct _M0TUsbE*)moonbit_malloc(sizeof(struct _M0TUsbE));
      Moonbit_object_header(_M0L8_2atupleS3157)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 71, 0);
      _M0L8_2atupleS3157->$0 = _M0L6_2akeyS1019;
      _M0L8_2atupleS3157->$1 = _M0L8_2avalueS1020;
      return _M0L8_2atupleS3157;
    }
  } else {
    goto join_1015;
  }
  join_1015:;
  return 0;
}

struct _M0TUssE* _M0MPB3Map4iterGssEC3141l711(
  struct _M0TWEOUssE* _M0L6_2aenvS3142
) {
  struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3141__l711__* _M0L14_2acasted__envS3143;
  struct _M0TPB8MutLocalGORPB5EntryGssEE* _M0L11curr__entryS1000;
  struct _M0TPB8MutLocalGiE* _M0L9remainingS1003;
  int32_t _M0L3valS3144;
  #line 711 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14_2acasted__envS3143
  = (struct _M0R65Map_3a_3aiter_7c_5bString_2c_20String_5d_7c_2eanon__u3141__l711__*)_M0L6_2aenvS3142;
  _M0L11curr__entryS1000 = _M0L14_2acasted__envS3143->$1;
  _M0L9remainingS1003 = _M0L14_2acasted__envS3143->$0;
  _M0L3valS3144 = _M0L9remainingS1003->$0;
  if (_M0L3valS3144 > 0) {
    struct _M0TPB5EntryGssE* _M0L7_2abindS1005 = _M0L11curr__entryS1000->$0;
    if (_M0L7_2abindS1005 == 0) {
      goto join_1004;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS1006 = _M0L7_2abindS1005;
      struct _M0TPB5EntryGssE* _M0L4_2axS1007 = _M0L7_2aSomeS1006;
      moonbit_string_t _M0L6_2akeyS1008 = _M0L4_2axS1007->$4;
      moonbit_string_t _M0L8_2avalueS1009 = _M0L4_2axS1007->$5;
      struct _M0TPB5EntryGssE* _M0L7_2anextS1010 = _M0L4_2axS1007->$1;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4024 = _M0L11curr__entryS1000->$0;
      int32_t _M0L3valS3146;
      int32_t _M0L6_2atmpS3145;
      struct _M0TUssE* _M0L8_2atupleS3147;
      if (_M0L7_2anextS1010) {
        moonbit_incref(_M0L7_2anextS1010);
      }
      moonbit_incref(_M0L8_2avalueS1009);
      moonbit_incref(_M0L6_2akeyS1008);
      if (_M0L6_2aoldS4024) {
        moonbit_decref(_M0L6_2aoldS4024);
      }
      _M0L11curr__entryS1000->$0 = _M0L7_2anextS1010;
      _M0L3valS3146 = _M0L9remainingS1003->$0;
      _M0L6_2atmpS3145 = _M0L3valS3146 - 1;
      _M0L9remainingS1003->$0 = _M0L6_2atmpS3145;
      _M0L8_2atupleS3147
      = (struct _M0TUssE*)moonbit_malloc(sizeof(struct _M0TUssE));
      Moonbit_object_header(_M0L8_2atupleS3147)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 74, 0);
      _M0L8_2atupleS3147->$0 = _M0L6_2akeyS1008;
      _M0L8_2atupleS3147->$1 = _M0L8_2avalueS1009;
      return _M0L8_2atupleS3147;
    }
  } else {
    goto join_1004;
  }
  join_1004:;
  return 0;
}

int32_t _M0MPB3Map6lengthGssE(struct _M0TPB3MapGssE* _M0L4selfS997) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS997->$1;
}

int32_t _M0MPB3Map6lengthGsbE(struct _M0TPB3MapGsbE* _M0L4selfS998) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS998->$1;
}

int32_t _M0MPB3Map6lengthGsfE(struct _M0TPB3MapGsfE* _M0L4selfS999) {
  #line 641 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  return _M0L4selfS999->$1;
}

int32_t _M0MPB3Map6removeGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS991,
  moonbit_string_t _M0L3keyS992
) {
  int32_t _M0L6_2atmpS3136;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3136 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS992);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsiE(_M0L4selfS991, _M0L3keyS992, _M0L6_2atmpS3136);
  return 0;
}

int32_t _M0MPB3Map6removeGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS993,
  moonbit_string_t _M0L3keyS994
) {
  int32_t _M0L6_2atmpS3137;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3137 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS994);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsRP19moonbitDB10RedisValueE(_M0L4selfS993, _M0L3keyS994, _M0L6_2atmpS3137);
  return 0;
}

int32_t _M0MPB3Map6removeGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS995,
  moonbit_string_t _M0L3keyS996
) {
  int32_t _M0L6_2atmpS3138;
  #line 490 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS3138 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS996);
  #line 491 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map18remove__with__hashGsbE(_M0L4selfS995, _M0L3keyS996, _M0L6_2atmpS3138);
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS967,
  moonbit_string_t _M0L3keyS971,
  int32_t _M0L4hashS970
) {
  int32_t _M0L14capacity__maskS3111;
  int32_t _M0L6_2atmpS3110;
  int32_t _M0L1iS964;
  int32_t _M0L3idxS965;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3111 = _M0L4selfS967->$3;
  _M0L6_2atmpS3110 = _M0L4hashS970 & _M0L14capacity__maskS3111;
  _M0L1iS964 = 0;
  _M0L3idxS965 = _M0L6_2atmpS3110;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3109 = _M0L4selfS967->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS966;
    if (
      _M0L3idxS965 < 0
      || _M0L3idxS965 >= Moonbit_array_length(_M0L7entriesS3109)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS966
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3109[_M0L3idxS965];
    if (_M0L7_2abindS966 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS968 = _M0L7_2abindS966;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS969 = _M0L7_2aSomeS968;
      int32_t _M0L4hashS3101 = _M0L8_2aentryS969->$3;
      int32_t _if__result_4567;
      int32_t _M0L3pslS3104;
      int32_t _M0L6_2atmpS3105;
      int32_t _M0L6_2atmpS3107;
      int32_t _M0L14capacity__maskS3108;
      int32_t _M0L6_2atmpS3106;
      if (_M0L4hashS3101 == _M0L4hashS970) {
        moonbit_string_t _M0L3keyS3100 = _M0L8_2aentryS969->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4567
        = _M0L3keyS3100 == _M0L3keyS971
          || Moonbit_array_length(_M0L3keyS3100)
             == Moonbit_array_length(_M0L3keyS971)
             && 0
                == memcmp(_M0L3keyS3100, _M0L3keyS971, Moonbit_array_length(_M0L3keyS3100) * 2);
      } else {
        _if__result_4567 = 0;
      }
      if (_if__result_4567) {
        int32_t _M0L4sizeS3103;
        int32_t _M0L6_2atmpS3102;
        moonbit_incref(_M0L8_2aentryS969);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsiE(_M0L4selfS967, _M0L8_2aentryS969);
        moonbit_decref(_M0L8_2aentryS969);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsiE(_M0L4selfS967, _M0L3idxS965);
        _M0L4sizeS3103 = _M0L4selfS967->$1;
        _M0L6_2atmpS3102 = _M0L4sizeS3103 - 1;
        _M0L4selfS967->$1 = _M0L6_2atmpS3102;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS969);
      }
      _M0L3pslS3104 = _M0L8_2aentryS969->$2;
      moonbit_decref(_M0L8_2aentryS969);
      if (_M0L1iS964 > _M0L3pslS3104) {
        break;
      }
      _M0L6_2atmpS3105 = _M0L1iS964 + 1;
      _M0L6_2atmpS3107 = _M0L3idxS965 + 1;
      _M0L14capacity__maskS3108 = _M0L4selfS967->$3;
      _M0L6_2atmpS3106 = _M0L6_2atmpS3107 & _M0L14capacity__maskS3108;
      _M0L1iS964 = _M0L6_2atmpS3105;
      _M0L3idxS965 = _M0L6_2atmpS3106;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS976,
  moonbit_string_t _M0L3keyS980,
  int32_t _M0L4hashS979
) {
  int32_t _M0L14capacity__maskS3123;
  int32_t _M0L6_2atmpS3122;
  int32_t _M0L1iS973;
  int32_t _M0L3idxS974;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3123 = _M0L4selfS976->$3;
  _M0L6_2atmpS3122 = _M0L4hashS979 & _M0L14capacity__maskS3123;
  _M0L1iS973 = 0;
  _M0L3idxS974 = _M0L6_2atmpS3122;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3121 =
      _M0L4selfS976->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS975;
    if (
      _M0L3idxS974 < 0
      || _M0L3idxS974 >= Moonbit_array_length(_M0L7entriesS3121)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS975
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3121[
        _M0L3idxS974
      ];
    if (_M0L7_2abindS975 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS977 =
        _M0L7_2abindS975;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L8_2aentryS978 =
        _M0L7_2aSomeS977;
      int32_t _M0L4hashS3113 = _M0L8_2aentryS978->$3;
      int32_t _if__result_4569;
      int32_t _M0L3pslS3116;
      int32_t _M0L6_2atmpS3117;
      int32_t _M0L6_2atmpS3119;
      int32_t _M0L14capacity__maskS3120;
      int32_t _M0L6_2atmpS3118;
      if (_M0L4hashS3113 == _M0L4hashS979) {
        moonbit_string_t _M0L3keyS3112 = _M0L8_2aentryS978->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4569
        = _M0L3keyS3112 == _M0L3keyS980
          || Moonbit_array_length(_M0L3keyS3112)
             == Moonbit_array_length(_M0L3keyS980)
             && 0
                == memcmp(_M0L3keyS3112, _M0L3keyS980, Moonbit_array_length(_M0L3keyS3112) * 2);
      } else {
        _if__result_4569 = 0;
      }
      if (_if__result_4569) {
        int32_t _M0L4sizeS3115;
        int32_t _M0L6_2atmpS3114;
        moonbit_incref(_M0L8_2aentryS978);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS976, _M0L8_2aentryS978);
        moonbit_decref(_M0L8_2aentryS978);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsRP19moonbitDB10RedisValueE(_M0L4selfS976, _M0L3idxS974);
        _M0L4sizeS3115 = _M0L4selfS976->$1;
        _M0L6_2atmpS3114 = _M0L4sizeS3115 - 1;
        _M0L4selfS976->$1 = _M0L6_2atmpS3114;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS978);
      }
      _M0L3pslS3116 = _M0L8_2aentryS978->$2;
      moonbit_decref(_M0L8_2aentryS978);
      if (_M0L1iS973 > _M0L3pslS3116) {
        break;
      }
      _M0L6_2atmpS3117 = _M0L1iS973 + 1;
      _M0L6_2atmpS3119 = _M0L3idxS974 + 1;
      _M0L14capacity__maskS3120 = _M0L4selfS976->$3;
      _M0L6_2atmpS3118 = _M0L6_2atmpS3119 & _M0L14capacity__maskS3120;
      _M0L1iS973 = _M0L6_2atmpS3117;
      _M0L3idxS974 = _M0L6_2atmpS3118;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map18remove__with__hashGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS985,
  moonbit_string_t _M0L3keyS989,
  int32_t _M0L4hashS988
) {
  int32_t _M0L14capacity__maskS3135;
  int32_t _M0L6_2atmpS3134;
  int32_t _M0L1iS982;
  int32_t _M0L3idxS983;
  #line 495 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS3135 = _M0L4selfS985->$3;
  _M0L6_2atmpS3134 = _M0L4hashS988 & _M0L14capacity__maskS3135;
  _M0L1iS982 = 0;
  _M0L3idxS983 = _M0L6_2atmpS3134;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS3133 = _M0L4selfS985->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS984;
    if (
      _M0L3idxS983 < 0
      || _M0L3idxS983 >= Moonbit_array_length(_M0L7entriesS3133)
    ) {
      #line 501 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS984
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3133[_M0L3idxS983];
    if (_M0L7_2abindS984 == 0) {
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS986 = _M0L7_2abindS984;
      struct _M0TPB5EntryGsbE* _M0L8_2aentryS987 = _M0L7_2aSomeS986;
      int32_t _M0L4hashS3125 = _M0L8_2aentryS987->$3;
      int32_t _if__result_4571;
      int32_t _M0L3pslS3128;
      int32_t _M0L6_2atmpS3129;
      int32_t _M0L6_2atmpS3131;
      int32_t _M0L14capacity__maskS3132;
      int32_t _M0L6_2atmpS3130;
      if (_M0L4hashS3125 == _M0L4hashS988) {
        moonbit_string_t _M0L3keyS3124 = _M0L8_2aentryS987->$4;
        #line 502 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4571
        = _M0L3keyS3124 == _M0L3keyS989
          || Moonbit_array_length(_M0L3keyS3124)
             == Moonbit_array_length(_M0L3keyS989)
             && 0
                == memcmp(_M0L3keyS3124, _M0L3keyS989, Moonbit_array_length(_M0L3keyS3124) * 2);
      } else {
        _if__result_4571 = 0;
      }
      if (_if__result_4571) {
        int32_t _M0L4sizeS3127;
        int32_t _M0L6_2atmpS3126;
        moonbit_incref(_M0L8_2aentryS987);
        #line 503 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map13remove__entryGsbE(_M0L4selfS985, _M0L8_2aentryS987);
        moonbit_decref(_M0L8_2aentryS987);
        #line 504 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map11shift__backGsbE(_M0L4selfS985, _M0L3idxS983);
        _M0L4sizeS3127 = _M0L4selfS985->$1;
        _M0L6_2atmpS3126 = _M0L4sizeS3127 - 1;
        _M0L4selfS985->$1 = _M0L6_2atmpS3126;
        break;
      } else {
        moonbit_incref(_M0L8_2aentryS987);
      }
      _M0L3pslS3128 = _M0L8_2aentryS987->$2;
      moonbit_decref(_M0L8_2aentryS987);
      if (_M0L1iS982 > _M0L3pslS3128) {
        break;
      }
      _M0L6_2atmpS3129 = _M0L1iS982 + 1;
      _M0L6_2atmpS3131 = _M0L3idxS983 + 1;
      _M0L14capacity__maskS3132 = _M0L4selfS985->$3;
      _M0L6_2atmpS3130 = _M0L6_2atmpS3131 & _M0L14capacity__maskS3132;
      _M0L1iS982 = _M0L6_2atmpS3129;
      _M0L3idxS983 = _M0L6_2atmpS3130;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS936,
  int32_t _M0L3idxS943
) {
  int32_t _M0L3curS934;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS934 = _M0L3idxS943;
  _2afor_938:;
  while (1) {
    int32_t _M0L6_2atmpS3084 = _M0L3curS934 + 1;
    int32_t _M0L14capacity__maskS3085 = _M0L4selfS936->$3;
    int32_t _M0L4nextS935 = _M0L6_2atmpS3084 & _M0L14capacity__maskS3085;
    struct _M0TPB5EntryGsiE** _M0L7entriesS3083 = _M0L4selfS936->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS939;
    struct _M0TPB5EntryGsiE** _M0L7entriesS3079;
    struct _M0TPB5EntryGsiE* _M0L6_2atmpS3080;
    struct _M0TPB5EntryGsiE* _M0L6_2aoldS4038;
    int32_t _tmp_4574;
    if (
      _M0L4nextS935 < 0
      || _M0L4nextS935 >= Moonbit_array_length(_M0L7entriesS3083)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS939
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3083[_M0L4nextS935];
    if (_M0L7_2abindS939 == 0) {
      goto join_937;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS940 = _M0L7_2abindS939;
      struct _M0TPB5EntryGsiE* _M0L4_2axS941 = _M0L7_2aSomeS940;
      int32_t _M0L4_2axS942 = _M0L4_2axS941->$2;
      switch (_M0L4_2axS942) {
        case 0: {
          goto join_937;
          break;
        }
        default: {
          int32_t _M0L3pslS3082 = _M0L4_2axS941->$2;
          int32_t _M0L6_2atmpS3081 = _M0L3pslS3082 - 1;
          _M0L4_2axS941->$2 = _M0L6_2atmpS3081;
          moonbit_incref(_M0L4_2axS941);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsiE(_M0L4selfS936, _M0L4_2axS941, _M0L3curS934);
          moonbit_decref(_M0L4_2axS941);
          _M0L3curS934 = _M0L4nextS935;
          goto _2afor_938;
          break;
        }
      }
    }
    goto joinlet_4573;
    join_937:;
    _M0L7entriesS3079 = _M0L4selfS936->$0;
    _M0L6_2atmpS3080 = 0;
    if (
      _M0L3curS934 < 0
      || _M0L3curS934 >= Moonbit_array_length(_M0L7entriesS3079)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4038
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3079[_M0L3curS934];
    if (_M0L6_2aoldS4038) {
      moonbit_decref(_M0L6_2aoldS4038);
    }
    _M0L7entriesS3079[_M0L3curS934] = _M0L6_2atmpS3080;
    break;
    joinlet_4573:;
    _tmp_4574 = _M0L3curS934;
    _M0L3curS934 = _tmp_4574;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS946,
  int32_t _M0L3idxS953
) {
  int32_t _M0L3curS944;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS944 = _M0L3idxS953;
  _2afor_948:;
  while (1) {
    int32_t _M0L6_2atmpS3091 = _M0L3curS944 + 1;
    int32_t _M0L14capacity__maskS3092 = _M0L4selfS946->$3;
    int32_t _M0L4nextS945 = _M0L6_2atmpS3091 & _M0L14capacity__maskS3092;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3090 =
      _M0L4selfS946->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS949;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3086;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3087;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4042;
    int32_t _tmp_4577;
    if (
      _M0L4nextS945 < 0
      || _M0L4nextS945 >= Moonbit_array_length(_M0L7entriesS3090)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS949
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3090[
        _M0L4nextS945
      ];
    if (_M0L7_2abindS949 == 0) {
      goto join_947;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS950 =
        _M0L7_2abindS949;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4_2axS951 =
        _M0L7_2aSomeS950;
      int32_t _M0L4_2axS952 = _M0L4_2axS951->$2;
      switch (_M0L4_2axS952) {
        case 0: {
          goto join_947;
          break;
        }
        default: {
          int32_t _M0L3pslS3089 = _M0L4_2axS951->$2;
          int32_t _M0L6_2atmpS3088 = _M0L3pslS3089 - 1;
          _M0L4_2axS951->$2 = _M0L6_2atmpS3088;
          moonbit_incref(_M0L4_2axS951);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS946, _M0L4_2axS951, _M0L3curS944);
          moonbit_decref(_M0L4_2axS951);
          _M0L3curS944 = _M0L4nextS945;
          goto _2afor_948;
          break;
        }
      }
    }
    goto joinlet_4576;
    join_947:;
    _M0L7entriesS3086 = _M0L4selfS946->$0;
    _M0L6_2atmpS3087 = 0;
    if (
      _M0L3curS944 < 0
      || _M0L3curS944 >= Moonbit_array_length(_M0L7entriesS3086)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4042
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3086[
        _M0L3curS944
      ];
    if (_M0L6_2aoldS4042) {
      moonbit_decref(_M0L6_2aoldS4042);
    }
    _M0L7entriesS3086[_M0L3curS944] = _M0L6_2atmpS3087;
    break;
    joinlet_4576:;
    _tmp_4577 = _M0L3curS944;
    _M0L3curS944 = _tmp_4577;
    continue;
    break;
  }
  return 0;
}

int32_t _M0MPB3Map11shift__backGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS956,
  int32_t _M0L3idxS963
) {
  int32_t _M0L3curS954;
  #line 543 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3curS954 = _M0L3idxS963;
  _2afor_958:;
  while (1) {
    int32_t _M0L6_2atmpS3098 = _M0L3curS954 + 1;
    int32_t _M0L14capacity__maskS3099 = _M0L4selfS956->$3;
    int32_t _M0L4nextS955 = _M0L6_2atmpS3098 & _M0L14capacity__maskS3099;
    struct _M0TPB5EntryGsbE** _M0L7entriesS3097 = _M0L4selfS956->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS959;
    struct _M0TPB5EntryGsbE** _M0L7entriesS3093;
    struct _M0TPB5EntryGsbE* _M0L6_2atmpS3094;
    struct _M0TPB5EntryGsbE* _M0L6_2aoldS4046;
    int32_t _tmp_4580;
    if (
      _M0L4nextS955 < 0
      || _M0L4nextS955 >= Moonbit_array_length(_M0L7entriesS3097)
    ) {
      #line 546 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS959
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3097[_M0L4nextS955];
    if (_M0L7_2abindS959 == 0) {
      goto join_957;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS960 = _M0L7_2abindS959;
      struct _M0TPB5EntryGsbE* _M0L4_2axS961 = _M0L7_2aSomeS960;
      int32_t _M0L4_2axS962 = _M0L4_2axS961->$2;
      switch (_M0L4_2axS962) {
        case 0: {
          goto join_957;
          break;
        }
        default: {
          int32_t _M0L3pslS3096 = _M0L4_2axS961->$2;
          int32_t _M0L6_2atmpS3095 = _M0L3pslS3096 - 1;
          _M0L4_2axS961->$2 = _M0L6_2atmpS3095;
          moonbit_incref(_M0L4_2axS961);
          #line 553 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map10set__entryGsbE(_M0L4selfS956, _M0L4_2axS961, _M0L3curS954);
          moonbit_decref(_M0L4_2axS961);
          _M0L3curS954 = _M0L4nextS955;
          goto _2afor_958;
          break;
        }
      }
    }
    goto joinlet_4579;
    join_957:;
    _M0L7entriesS3093 = _M0L4selfS956->$0;
    _M0L6_2atmpS3094 = 0;
    if (
      _M0L3curS954 < 0
      || _M0L3curS954 >= Moonbit_array_length(_M0L7entriesS3093)
    ) {
      #line 548 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4046
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3093[_M0L3curS954];
    if (_M0L6_2aoldS4046) {
      moonbit_decref(_M0L6_2aoldS4046);
    }
    _M0L7entriesS3093[_M0L3curS954] = _M0L6_2atmpS3094;
    break;
    joinlet_4579:;
    _tmp_4580 = _M0L3curS954;
    _M0L3curS954 = _tmp_4580;
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
      struct _M0TPB5EntryGsiE* _M0L4nextS3058 = _M0L5entryS917->$1;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4051 = _M0L4selfS918->$5;
      if (_M0L4nextS3058) {
        moonbit_incref(_M0L4nextS3058);
      }
      if (_M0L6_2aoldS4051) {
        moonbit_decref(_M0L6_2aoldS4051);
      }
      _M0L4selfS918->$5 = _M0L4nextS3058;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS3062 = _M0L4selfS918->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS3061;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS3059;
      struct _M0TPB5EntryGsiE* _M0L4nextS3060;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4053;
      if (
        _M0L7_2abindS916 < 0
        || _M0L7_2abindS916 >= Moonbit_array_length(_M0L7entriesS3062)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3061
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3062[_M0L7_2abindS916];
      if (_M0L6_2atmpS3061) {
        moonbit_incref(_M0L6_2atmpS3061);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3059
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS3061);
      if (_M0L6_2atmpS3061) {
        moonbit_decref(_M0L6_2atmpS3061);
      }
      _M0L4nextS3060 = _M0L5entryS917->$1;
      _M0L6_2aoldS4053 = _M0L6_2atmpS3059->$1;
      if (_M0L4nextS3060) {
        moonbit_incref(_M0L4nextS3060);
      }
      if (_M0L6_2aoldS4053) {
        moonbit_decref(_M0L6_2aoldS4053);
      }
      _M0L6_2atmpS3059->$1 = _M0L4nextS3060;
      moonbit_decref(_M0L6_2atmpS3059);
      break;
    }
  }
  _M0L7_2abindS919 = _M0L5entryS917->$1;
  if (_M0L7_2abindS919 == 0) {
    int32_t _M0L4prevS3063 = _M0L5entryS917->$0;
    _M0L4selfS918->$6 = _M0L4prevS3063;
  } else {
    struct _M0TPB5EntryGsiE* _M0L7_2aSomeS920 = _M0L7_2abindS919;
    struct _M0TPB5EntryGsiE* _M0L7_2anextS921 = _M0L7_2aSomeS920;
    int32_t _M0L4prevS3064 = _M0L5entryS917->$0;
    _M0L7_2anextS921->$0 = _M0L4prevS3064;
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
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4nextS3065 =
        _M0L5entryS923->$1;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4058 =
        _M0L4selfS924->$5;
      if (_M0L4nextS3065) {
        moonbit_incref(_M0L4nextS3065);
      }
      if (_M0L6_2aoldS4058) {
        moonbit_decref(_M0L6_2aoldS4058);
      }
      _M0L4selfS924->$5 = _M0L4nextS3065;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3069 =
        _M0L4selfS924->$0;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3068;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS3066;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L4nextS3067;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4060;
      if (
        _M0L7_2abindS922 < 0
        || _M0L7_2abindS922 >= Moonbit_array_length(_M0L7entriesS3069)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3068
      = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3069[
          _M0L7_2abindS922
        ];
      if (_M0L6_2atmpS3068) {
        moonbit_incref(_M0L6_2atmpS3068);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3066
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP19moonbitDB10RedisValueEE(_M0L6_2atmpS3068);
      if (_M0L6_2atmpS3068) {
        moonbit_decref(_M0L6_2atmpS3068);
      }
      _M0L4nextS3067 = _M0L5entryS923->$1;
      _M0L6_2aoldS4060 = _M0L6_2atmpS3066->$1;
      if (_M0L4nextS3067) {
        moonbit_incref(_M0L4nextS3067);
      }
      if (_M0L6_2aoldS4060) {
        moonbit_decref(_M0L6_2aoldS4060);
      }
      _M0L6_2atmpS3066->$1 = _M0L4nextS3067;
      moonbit_decref(_M0L6_2atmpS3066);
      break;
    }
  }
  _M0L7_2abindS925 = _M0L5entryS923->$1;
  if (_M0L7_2abindS925 == 0) {
    int32_t _M0L4prevS3070 = _M0L5entryS923->$0;
    _M0L4selfS924->$6 = _M0L4prevS3070;
  } else {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS926 =
      _M0L7_2abindS925;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2anextS927 =
      _M0L7_2aSomeS926;
    int32_t _M0L4prevS3071 = _M0L5entryS923->$0;
    _M0L7_2anextS927->$0 = _M0L4prevS3071;
  }
  return 0;
}

int32_t _M0MPB3Map13remove__entryGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS930,
  struct _M0TPB5EntryGsbE* _M0L5entryS929
) {
  int32_t _M0L7_2abindS928;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS931;
  #line 531 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS928 = _M0L5entryS929->$0;
  switch (_M0L7_2abindS928) {
    case -1: {
      struct _M0TPB5EntryGsbE* _M0L4nextS3072 = _M0L5entryS929->$1;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4065 = _M0L4selfS930->$5;
      if (_M0L4nextS3072) {
        moonbit_incref(_M0L4nextS3072);
      }
      if (_M0L6_2aoldS4065) {
        moonbit_decref(_M0L6_2aoldS4065);
      }
      _M0L4selfS930->$5 = _M0L4nextS3072;
      break;
    }
    default: {
      struct _M0TPB5EntryGsbE** _M0L7entriesS3076 = _M0L4selfS930->$0;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS3075;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS3073;
      struct _M0TPB5EntryGsbE* _M0L4nextS3074;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4067;
      if (
        _M0L7_2abindS928 < 0
        || _M0L7_2abindS928 >= Moonbit_array_length(_M0L7entriesS3076)
      ) {
        #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3075
      = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3076[_M0L7_2abindS928];
      if (_M0L6_2atmpS3075) {
        moonbit_incref(_M0L6_2atmpS3075);
      }
      #line 534 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS3073
      = _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(_M0L6_2atmpS3075);
      if (_M0L6_2atmpS3075) {
        moonbit_decref(_M0L6_2atmpS3075);
      }
      _M0L4nextS3074 = _M0L5entryS929->$1;
      _M0L6_2aoldS4067 = _M0L6_2atmpS3073->$1;
      if (_M0L4nextS3074) {
        moonbit_incref(_M0L4nextS3074);
      }
      if (_M0L6_2aoldS4067) {
        moonbit_decref(_M0L6_2aoldS4067);
      }
      _M0L6_2atmpS3073->$1 = _M0L4nextS3074;
      moonbit_decref(_M0L6_2atmpS3073);
      break;
    }
  }
  _M0L7_2abindS931 = _M0L5entryS929->$1;
  if (_M0L7_2abindS931 == 0) {
    int32_t _M0L4prevS3077 = _M0L5entryS929->$0;
    _M0L4selfS930->$6 = _M0L4prevS3077;
  } else {
    struct _M0TPB5EntryGsbE* _M0L7_2aSomeS932 = _M0L7_2abindS931;
    struct _M0TPB5EntryGsbE* _M0L7_2anextS933 = _M0L7_2aSomeS932;
    int32_t _M0L4prevS3078 = _M0L5entryS929->$0;
    _M0L7_2anextS933->$0 = _M0L4prevS3078;
  }
  return 0;
}

int32_t _M0MPB3Map8containsGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS885,
  moonbit_string_t _M0L3keyS881
) {
  int32_t _M0L4hashS880;
  int32_t _M0L14capacity__maskS3027;
  int32_t _M0L6_2atmpS3026;
  int32_t _M0L1iS882;
  int32_t _M0L3idxS883;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS880 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS881);
  _M0L14capacity__maskS3027 = _M0L4selfS885->$3;
  _M0L6_2atmpS3026 = _M0L4hashS880 & _M0L14capacity__maskS3027;
  _M0L1iS882 = 0;
  _M0L3idxS883 = _M0L6_2atmpS3026;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS3025 = _M0L4selfS885->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS884;
    if (
      _M0L3idxS883 < 0
      || _M0L3idxS883 >= Moonbit_array_length(_M0L7entriesS3025)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS884
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS3025[_M0L3idxS883];
    if (_M0L7_2abindS884 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS886 = _M0L7_2abindS884;
      struct _M0TPB5EntryGsbE* _M0L8_2aentryS887 = _M0L7_2aSomeS886;
      int32_t _M0L4hashS3019 = _M0L8_2aentryS887->$3;
      int32_t _if__result_4582;
      int32_t _M0L3pslS3020;
      int32_t _M0L6_2atmpS3021;
      int32_t _M0L6_2atmpS3023;
      int32_t _M0L14capacity__maskS3024;
      int32_t _M0L6_2atmpS3022;
      if (_M0L4hashS3019 == _M0L4hashS880) {
        moonbit_string_t _M0L3keyS3018 = _M0L8_2aentryS887->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4582
        = _M0L3keyS3018 == _M0L3keyS881
          || Moonbit_array_length(_M0L3keyS3018)
             == Moonbit_array_length(_M0L3keyS881)
             && 0
                == memcmp(_M0L3keyS3018, _M0L3keyS881, Moonbit_array_length(_M0L3keyS3018) * 2);
      } else {
        _if__result_4582 = 0;
      }
      if (_if__result_4582) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS887);
      }
      _M0L3pslS3020 = _M0L8_2aentryS887->$2;
      moonbit_decref(_M0L8_2aentryS887);
      if (_M0L1iS882 > _M0L3pslS3020) {
        return 0;
      }
      _M0L6_2atmpS3021 = _M0L1iS882 + 1;
      _M0L6_2atmpS3023 = _M0L3idxS883 + 1;
      _M0L14capacity__maskS3024 = _M0L4selfS885->$3;
      _M0L6_2atmpS3022 = _M0L6_2atmpS3023 & _M0L14capacity__maskS3024;
      _M0L1iS882 = _M0L6_2atmpS3021;
      _M0L3idxS883 = _M0L6_2atmpS3022;
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
  int32_t _M0L14capacity__maskS3037;
  int32_t _M0L6_2atmpS3036;
  int32_t _M0L1iS891;
  int32_t _M0L3idxS892;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS889 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS890);
  _M0L14capacity__maskS3037 = _M0L4selfS894->$3;
  _M0L6_2atmpS3036 = _M0L4hashS889 & _M0L14capacity__maskS3037;
  _M0L1iS891 = 0;
  _M0L3idxS892 = _M0L6_2atmpS3036;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS3035 = _M0L4selfS894->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS893;
    if (
      _M0L3idxS892 < 0
      || _M0L3idxS892 >= Moonbit_array_length(_M0L7entriesS3035)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS893
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3035[_M0L3idxS892];
    if (_M0L7_2abindS893 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS895 = _M0L7_2abindS893;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS896 = _M0L7_2aSomeS895;
      int32_t _M0L4hashS3029 = _M0L8_2aentryS896->$3;
      int32_t _if__result_4584;
      int32_t _M0L3pslS3030;
      int32_t _M0L6_2atmpS3031;
      int32_t _M0L6_2atmpS3033;
      int32_t _M0L14capacity__maskS3034;
      int32_t _M0L6_2atmpS3032;
      if (_M0L4hashS3029 == _M0L4hashS889) {
        moonbit_string_t _M0L3keyS3028 = _M0L8_2aentryS896->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4584
        = _M0L3keyS3028 == _M0L3keyS890
          || Moonbit_array_length(_M0L3keyS3028)
             == Moonbit_array_length(_M0L3keyS890)
             && 0
                == memcmp(_M0L3keyS3028, _M0L3keyS890, Moonbit_array_length(_M0L3keyS3028) * 2);
      } else {
        _if__result_4584 = 0;
      }
      if (_if__result_4584) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS896);
      }
      _M0L3pslS3030 = _M0L8_2aentryS896->$2;
      moonbit_decref(_M0L8_2aentryS896);
      if (_M0L1iS891 > _M0L3pslS3030) {
        return 0;
      }
      _M0L6_2atmpS3031 = _M0L1iS891 + 1;
      _M0L6_2atmpS3033 = _M0L3idxS892 + 1;
      _M0L14capacity__maskS3034 = _M0L4selfS894->$3;
      _M0L6_2atmpS3032 = _M0L6_2atmpS3033 & _M0L14capacity__maskS3034;
      _M0L1iS891 = _M0L6_2atmpS3031;
      _M0L3idxS892 = _M0L6_2atmpS3032;
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
  int32_t _M0L14capacity__maskS3047;
  int32_t _M0L6_2atmpS3046;
  int32_t _M0L1iS900;
  int32_t _M0L3idxS901;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS898 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS899);
  _M0L14capacity__maskS3047 = _M0L4selfS903->$3;
  _M0L6_2atmpS3046 = _M0L4hashS898 & _M0L14capacity__maskS3047;
  _M0L1iS900 = 0;
  _M0L3idxS901 = _M0L6_2atmpS3046;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS3045 =
      _M0L4selfS903->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS902;
    if (
      _M0L3idxS901 < 0
      || _M0L3idxS901 >= Moonbit_array_length(_M0L7entriesS3045)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS902
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS3045[
        _M0L3idxS901
      ];
    if (_M0L7_2abindS902 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS904 =
        _M0L7_2abindS902;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L8_2aentryS905 =
        _M0L7_2aSomeS904;
      int32_t _M0L4hashS3039 = _M0L8_2aentryS905->$3;
      int32_t _if__result_4586;
      int32_t _M0L3pslS3040;
      int32_t _M0L6_2atmpS3041;
      int32_t _M0L6_2atmpS3043;
      int32_t _M0L14capacity__maskS3044;
      int32_t _M0L6_2atmpS3042;
      if (_M0L4hashS3039 == _M0L4hashS898) {
        moonbit_string_t _M0L3keyS3038 = _M0L8_2aentryS905->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4586
        = _M0L3keyS3038 == _M0L3keyS899
          || Moonbit_array_length(_M0L3keyS3038)
             == Moonbit_array_length(_M0L3keyS899)
             && 0
                == memcmp(_M0L3keyS3038, _M0L3keyS899, Moonbit_array_length(_M0L3keyS3038) * 2);
      } else {
        _if__result_4586 = 0;
      }
      if (_if__result_4586) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS905);
      }
      _M0L3pslS3040 = _M0L8_2aentryS905->$2;
      moonbit_decref(_M0L8_2aentryS905);
      if (_M0L1iS900 > _M0L3pslS3040) {
        return 0;
      }
      _M0L6_2atmpS3041 = _M0L1iS900 + 1;
      _M0L6_2atmpS3043 = _M0L3idxS901 + 1;
      _M0L14capacity__maskS3044 = _M0L4selfS903->$3;
      _M0L6_2atmpS3042 = _M0L6_2atmpS3043 & _M0L14capacity__maskS3044;
      _M0L1iS900 = _M0L6_2atmpS3041;
      _M0L3idxS901 = _M0L6_2atmpS3042;
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
  int32_t _M0L14capacity__maskS3057;
  int32_t _M0L6_2atmpS3056;
  int32_t _M0L1iS909;
  int32_t _M0L3idxS910;
  #line 413 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 415 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS907 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS908);
  _M0L14capacity__maskS3057 = _M0L4selfS912->$3;
  _M0L6_2atmpS3056 = _M0L4hashS907 & _M0L14capacity__maskS3057;
  _M0L1iS909 = 0;
  _M0L3idxS910 = _M0L6_2atmpS3056;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3055 = _M0L4selfS912->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS911;
    if (
      _M0L3idxS910 < 0
      || _M0L3idxS910 >= Moonbit_array_length(_M0L7entriesS3055)
    ) {
      #line 417 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS911
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3055[_M0L3idxS910];
    if (_M0L7_2abindS911 == 0) {
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS913 = _M0L7_2abindS911;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS914 = _M0L7_2aSomeS913;
      int32_t _M0L4hashS3049 = _M0L8_2aentryS914->$3;
      int32_t _if__result_4588;
      int32_t _M0L3pslS3050;
      int32_t _M0L6_2atmpS3051;
      int32_t _M0L6_2atmpS3053;
      int32_t _M0L14capacity__maskS3054;
      int32_t _M0L6_2atmpS3052;
      if (_M0L4hashS3049 == _M0L4hashS907) {
        moonbit_string_t _M0L3keyS3048 = _M0L8_2aentryS914->$4;
        #line 418 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4588
        = _M0L3keyS3048 == _M0L3keyS908
          || Moonbit_array_length(_M0L3keyS3048)
             == Moonbit_array_length(_M0L3keyS908)
             && 0
                == memcmp(_M0L3keyS3048, _M0L3keyS908, Moonbit_array_length(_M0L3keyS3048) * 2);
      } else {
        _if__result_4588 = 0;
      }
      if (_if__result_4588) {
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS914);
      }
      _M0L3pslS3050 = _M0L8_2aentryS914->$2;
      moonbit_decref(_M0L8_2aentryS914);
      if (_M0L1iS909 > _M0L3pslS3050) {
        return 0;
      }
      _M0L6_2atmpS3051 = _M0L1iS909 + 1;
      _M0L6_2atmpS3053 = _M0L3idxS910 + 1;
      _M0L14capacity__maskS3054 = _M0L4selfS912->$3;
      _M0L6_2atmpS3052 = _M0L6_2atmpS3053 & _M0L14capacity__maskS3054;
      _M0L1iS909 = _M0L6_2atmpS3051;
      _M0L3idxS910 = _M0L6_2atmpS3052;
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
  int32_t _M0L14capacity__maskS2977;
  int32_t _M0L6_2atmpS2976;
  int32_t _M0L1iS846;
  int32_t _M0L3idxS847;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS844 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS845);
  _M0L14capacity__maskS2977 = _M0L4selfS849->$3;
  _M0L6_2atmpS2976 = _M0L4hashS844 & _M0L14capacity__maskS2977;
  _M0L1iS846 = 0;
  _M0L3idxS847 = _M0L6_2atmpS2976;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2975 =
      _M0L4selfS849->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS848;
    if (
      _M0L3idxS847 < 0
      || _M0L3idxS847 >= Moonbit_array_length(_M0L7entriesS2975)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS848
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2975[
        _M0L3idxS847
      ];
    if (_M0L7_2abindS848 == 0) {
      void* _M0L6_2atmpS2964 = 0;
      return _M0L6_2atmpS2964;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS850 =
        _M0L7_2abindS848;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L8_2aentryS851 =
        _M0L7_2aSomeS850;
      int32_t _M0L4hashS2966 = _M0L8_2aentryS851->$3;
      int32_t _if__result_4590;
      int32_t _M0L3pslS2969;
      int32_t _M0L6_2atmpS2971;
      int32_t _M0L6_2atmpS2973;
      int32_t _M0L14capacity__maskS2974;
      int32_t _M0L6_2atmpS2972;
      if (_M0L4hashS2966 == _M0L4hashS844) {
        moonbit_string_t _M0L3keyS2965 = _M0L8_2aentryS851->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4590
        = _M0L3keyS2965 == _M0L3keyS845
          || Moonbit_array_length(_M0L3keyS2965)
             == Moonbit_array_length(_M0L3keyS845)
             && 0
                == memcmp(_M0L3keyS2965, _M0L3keyS845, Moonbit_array_length(_M0L3keyS2965) * 2);
      } else {
        _if__result_4590 = 0;
      }
      if (_if__result_4590) {
        void* _M0L5valueS2968 = _M0L8_2aentryS851->$5;
        void* _M0L6_2atmpS2967;
        moonbit_incref(_M0L5valueS2968);
        _M0L6_2atmpS2967 = _M0L5valueS2968;
        return _M0L6_2atmpS2967;
      } else {
        moonbit_incref(_M0L8_2aentryS851);
      }
      _M0L3pslS2969 = _M0L8_2aentryS851->$2;
      moonbit_decref(_M0L8_2aentryS851);
      if (_M0L1iS846 > _M0L3pslS2969) {
        void* _M0L6_2atmpS2970 = 0;
        return _M0L6_2atmpS2970;
      }
      _M0L6_2atmpS2971 = _M0L1iS846 + 1;
      _M0L6_2atmpS2973 = _M0L3idxS847 + 1;
      _M0L14capacity__maskS2974 = _M0L4selfS849->$3;
      _M0L6_2atmpS2972 = _M0L6_2atmpS2973 & _M0L14capacity__maskS2974;
      _M0L1iS846 = _M0L6_2atmpS2971;
      _M0L3idxS847 = _M0L6_2atmpS2972;
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
  int32_t _M0L14capacity__maskS2991;
  int32_t _M0L6_2atmpS2990;
  int32_t _M0L1iS855;
  int32_t _M0L3idxS856;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS853 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS854);
  _M0L14capacity__maskS2991 = _M0L4selfS858->$3;
  _M0L6_2atmpS2990 = _M0L4hashS853 & _M0L14capacity__maskS2991;
  _M0L1iS855 = 0;
  _M0L3idxS856 = _M0L6_2atmpS2990;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2989 = _M0L4selfS858->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS857;
    if (
      _M0L3idxS856 < 0
      || _M0L3idxS856 >= Moonbit_array_length(_M0L7entriesS2989)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS857
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2989[_M0L3idxS856];
    if (_M0L7_2abindS857 == 0) {
      moonbit_string_t _M0L6_2atmpS2978 = 0;
      return _M0L6_2atmpS2978;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS859 = _M0L7_2abindS857;
      struct _M0TPB5EntryGssE* _M0L8_2aentryS860 = _M0L7_2aSomeS859;
      int32_t _M0L4hashS2980 = _M0L8_2aentryS860->$3;
      int32_t _if__result_4592;
      int32_t _M0L3pslS2983;
      int32_t _M0L6_2atmpS2985;
      int32_t _M0L6_2atmpS2987;
      int32_t _M0L14capacity__maskS2988;
      int32_t _M0L6_2atmpS2986;
      if (_M0L4hashS2980 == _M0L4hashS853) {
        moonbit_string_t _M0L3keyS2979 = _M0L8_2aentryS860->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4592
        = _M0L3keyS2979 == _M0L3keyS854
          || Moonbit_array_length(_M0L3keyS2979)
             == Moonbit_array_length(_M0L3keyS854)
             && 0
                == memcmp(_M0L3keyS2979, _M0L3keyS854, Moonbit_array_length(_M0L3keyS2979) * 2);
      } else {
        _if__result_4592 = 0;
      }
      if (_if__result_4592) {
        moonbit_string_t _M0L5valueS2982 = _M0L8_2aentryS860->$5;
        moonbit_string_t _M0L6_2atmpS2981;
        moonbit_incref(_M0L5valueS2982);
        _M0L6_2atmpS2981 = _M0L5valueS2982;
        return _M0L6_2atmpS2981;
      } else {
        moonbit_incref(_M0L8_2aentryS860);
      }
      _M0L3pslS2983 = _M0L8_2aentryS860->$2;
      moonbit_decref(_M0L8_2aentryS860);
      if (_M0L1iS855 > _M0L3pslS2983) {
        moonbit_string_t _M0L6_2atmpS2984 = 0;
        return _M0L6_2atmpS2984;
      }
      _M0L6_2atmpS2985 = _M0L1iS855 + 1;
      _M0L6_2atmpS2987 = _M0L3idxS856 + 1;
      _M0L14capacity__maskS2988 = _M0L4selfS858->$3;
      _M0L6_2atmpS2986 = _M0L6_2atmpS2987 & _M0L14capacity__maskS2988;
      _M0L1iS855 = _M0L6_2atmpS2985;
      _M0L3idxS856 = _M0L6_2atmpS2986;
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
  int32_t _M0L14capacity__maskS3005;
  int32_t _M0L6_2atmpS3004;
  int32_t _M0L1iS864;
  int32_t _M0L3idxS865;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS862 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS863);
  _M0L14capacity__maskS3005 = _M0L4selfS867->$3;
  _M0L6_2atmpS3004 = _M0L4hashS862 & _M0L14capacity__maskS3005;
  _M0L1iS864 = 0;
  _M0L3idxS865 = _M0L6_2atmpS3004;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS3003 = _M0L4selfS867->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS866;
    if (
      _M0L3idxS865 < 0
      || _M0L3idxS865 >= Moonbit_array_length(_M0L7entriesS3003)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS866
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS3003[_M0L3idxS865];
    if (_M0L7_2abindS866 == 0) {
      void* _M0L4NoneS2992 =
        (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      return _M0L4NoneS2992;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS868 = _M0L7_2abindS866;
      struct _M0TPB5EntryGsfE* _M0L8_2aentryS869 = _M0L7_2aSomeS868;
      int32_t _M0L4hashS2994 = _M0L8_2aentryS869->$3;
      int32_t _if__result_4594;
      int32_t _M0L3pslS2997;
      int32_t _M0L6_2atmpS2999;
      int32_t _M0L6_2atmpS3001;
      int32_t _M0L14capacity__maskS3002;
      int32_t _M0L6_2atmpS3000;
      if (_M0L4hashS2994 == _M0L4hashS862) {
        moonbit_string_t _M0L3keyS2993 = _M0L8_2aentryS869->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4594
        = _M0L3keyS2993 == _M0L3keyS863
          || Moonbit_array_length(_M0L3keyS2993)
             == Moonbit_array_length(_M0L3keyS863)
             && 0
                == memcmp(_M0L3keyS2993, _M0L3keyS863, Moonbit_array_length(_M0L3keyS2993) * 2);
      } else {
        _if__result_4594 = 0;
      }
      if (_if__result_4594) {
        float _M0L5valueS2996 = _M0L8_2aentryS869->$5;
        void* _M0L4SomeS2995 =
          (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGfE4Some));
        Moonbit_object_header(_M0L4SomeS2995)->meta
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 1);
        ((struct _M0DTPC16option6OptionGfE4Some*)_M0L4SomeS2995)->$0
        = _M0L5valueS2996;
        return _M0L4SomeS2995;
      } else {
        moonbit_incref(_M0L8_2aentryS869);
      }
      _M0L3pslS2997 = _M0L8_2aentryS869->$2;
      moonbit_decref(_M0L8_2aentryS869);
      if (_M0L1iS864 > _M0L3pslS2997) {
        void* _M0L4NoneS2998 =
          (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
        return _M0L4NoneS2998;
      }
      _M0L6_2atmpS2999 = _M0L1iS864 + 1;
      _M0L6_2atmpS3001 = _M0L3idxS865 + 1;
      _M0L14capacity__maskS3002 = _M0L4selfS867->$3;
      _M0L6_2atmpS3000 = _M0L6_2atmpS3001 & _M0L14capacity__maskS3002;
      _M0L1iS864 = _M0L6_2atmpS2999;
      _M0L3idxS865 = _M0L6_2atmpS3000;
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
  int32_t _M0L14capacity__maskS3017;
  int32_t _M0L6_2atmpS3016;
  int32_t _M0L1iS873;
  int32_t _M0L3idxS874;
  #line 236 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 237 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS871 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS872);
  _M0L14capacity__maskS3017 = _M0L4selfS876->$3;
  _M0L6_2atmpS3016 = _M0L4hashS871 & _M0L14capacity__maskS3017;
  _M0L1iS873 = 0;
  _M0L3idxS874 = _M0L6_2atmpS3016;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS3015 = _M0L4selfS876->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS875;
    if (
      _M0L3idxS874 < 0
      || _M0L3idxS874 >= Moonbit_array_length(_M0L7entriesS3015)
    ) {
      #line 239 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS875
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS3015[_M0L3idxS874];
    if (_M0L7_2abindS875 == 0) {
      return 4294967296ll;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS877 = _M0L7_2abindS875;
      struct _M0TPB5EntryGsiE* _M0L8_2aentryS878 = _M0L7_2aSomeS877;
      int32_t _M0L4hashS3007 = _M0L8_2aentryS878->$3;
      int32_t _if__result_4596;
      int32_t _M0L3pslS3010;
      int32_t _M0L6_2atmpS3011;
      int32_t _M0L6_2atmpS3013;
      int32_t _M0L14capacity__maskS3014;
      int32_t _M0L6_2atmpS3012;
      if (_M0L4hashS3007 == _M0L4hashS871) {
        moonbit_string_t _M0L3keyS3006 = _M0L8_2aentryS878->$4;
        #line 240 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4596
        = _M0L3keyS3006 == _M0L3keyS872
          || Moonbit_array_length(_M0L3keyS3006)
             == Moonbit_array_length(_M0L3keyS872)
             && 0
                == memcmp(_M0L3keyS3006, _M0L3keyS872, Moonbit_array_length(_M0L3keyS3006) * 2);
      } else {
        _if__result_4596 = 0;
      }
      if (_if__result_4596) {
        int32_t _M0L5valueS3009 = _M0L8_2aentryS878->$5;
        int64_t _M0L6_2atmpS3008 = (int64_t)_M0L5valueS3009;
        return _M0L6_2atmpS3008;
      } else {
        moonbit_incref(_M0L8_2aentryS878);
      }
      _M0L3pslS3010 = _M0L8_2aentryS878->$2;
      moonbit_decref(_M0L8_2aentryS878);
      if (_M0L1iS873 > _M0L3pslS3010) {
        return 4294967296ll;
      }
      _M0L6_2atmpS3011 = _M0L1iS873 + 1;
      _M0L6_2atmpS3013 = _M0L3idxS874 + 1;
      _M0L14capacity__maskS3014 = _M0L4selfS876->$3;
      _M0L6_2atmpS3012 = _M0L6_2atmpS3013 & _M0L14capacity__maskS3014;
      _M0L1iS873 = _M0L6_2atmpS3011;
      _M0L3idxS874 = _M0L6_2atmpS3012;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0MPB3Map3MapGsRP19moonbitDB10RedisValueE(
  struct _M0TPB9ArrayViewGUsRP19moonbitDB10RedisValueEE _M0L3arrS790,
  int64_t _M0L8capacityS792
) {
  int32_t _M0L3endS2918;
  int32_t _M0L5startS2919;
  int32_t _M0L6lengthS789;
  int32_t _M0L8capacityS791;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L1mS795;
  int32_t _M0L3endS2915;
  int32_t _M0L5startS2916;
  int32_t _M0L7_2abindS796;
  int32_t _M0L2__S797;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2918 = _M0L3arrS790.$2;
  _M0L5startS2919 = _M0L3arrS790.$1;
  _M0L6lengthS789 = _M0L3endS2918 - _M0L5startS2919;
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
    int32_t _M0L6_2atmpS2917;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2917 = _M0FPB21capacity__for__length(_M0L6lengthS789);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS791
    = _M0MPC13int3Int3max(_M0L11_2acapacityS794, _M0L6_2atmpS2917);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS795 = _M0FPB8new__mapGsRP19moonbitDB10RedisValueE(_M0L8capacityS791);
  _M0L3endS2915 = _M0L3arrS790.$2;
  _M0L5startS2916 = _M0L3arrS790.$1;
  _M0L7_2abindS796 = _M0L3endS2915 - _M0L5startS2916;
  _M0L2__S797 = 0;
  while (1) {
    if (_M0L2__S797 < _M0L7_2abindS796) {
      struct _M0TUsRP19moonbitDB10RedisValueE** _M0L3bufS2912 =
        _M0L3arrS790.$0;
      int32_t _M0L5startS2914 = _M0L3arrS790.$1;
      int32_t _M0L6_2atmpS2913 = _M0L5startS2914 + _M0L2__S797;
      struct _M0TUsRP19moonbitDB10RedisValueE* _M0L1eS798 =
        (struct _M0TUsRP19moonbitDB10RedisValueE*)_M0L3bufS2912[
          _M0L6_2atmpS2913
        ];
      moonbit_string_t _M0L6_2atmpS2909 = _M0L1eS798->$0;
      void* _M0L6_2atmpS2910 = _M0L1eS798->$1;
      int32_t _M0L6_2atmpS2911;
      moonbit_incref(_M0L6_2atmpS2910);
      moonbit_incref(_M0L6_2atmpS2909);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsRP19moonbitDB10RedisValueE(_M0L1mS795, _M0L6_2atmpS2909, _M0L6_2atmpS2910);
      moonbit_decref(_M0L6_2atmpS2909);
      moonbit_decref(_M0L6_2atmpS2910);
      _M0L6_2atmpS2911 = _M0L2__S797 + 1;
      _M0L2__S797 = _M0L6_2atmpS2911;
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
  int32_t _M0L3endS2929;
  int32_t _M0L5startS2930;
  int32_t _M0L6lengthS800;
  int32_t _M0L8capacityS802;
  struct _M0TPB3MapGsiE* _M0L1mS806;
  int32_t _M0L3endS2926;
  int32_t _M0L5startS2927;
  int32_t _M0L7_2abindS807;
  int32_t _M0L2__S808;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2929 = _M0L3arrS801.$2;
  _M0L5startS2930 = _M0L3arrS801.$1;
  _M0L6lengthS800 = _M0L3endS2929 - _M0L5startS2930;
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
    int32_t _M0L6_2atmpS2928;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2928 = _M0FPB21capacity__for__length(_M0L6lengthS800);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS802
    = _M0MPC13int3Int3max(_M0L11_2acapacityS805, _M0L6_2atmpS2928);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS806 = _M0FPB8new__mapGsiE(_M0L8capacityS802);
  _M0L3endS2926 = _M0L3arrS801.$2;
  _M0L5startS2927 = _M0L3arrS801.$1;
  _M0L7_2abindS807 = _M0L3endS2926 - _M0L5startS2927;
  _M0L2__S808 = 0;
  while (1) {
    if (_M0L2__S808 < _M0L7_2abindS807) {
      struct _M0TUsiE** _M0L3bufS2923 = _M0L3arrS801.$0;
      int32_t _M0L5startS2925 = _M0L3arrS801.$1;
      int32_t _M0L6_2atmpS2924 = _M0L5startS2925 + _M0L2__S808;
      struct _M0TUsiE* _M0L1eS809 =
        (struct _M0TUsiE*)_M0L3bufS2923[_M0L6_2atmpS2924];
      moonbit_string_t _M0L6_2atmpS2920 = _M0L1eS809->$0;
      int32_t _M0L6_2atmpS2921 = _M0L1eS809->$1;
      int32_t _M0L6_2atmpS2922;
      moonbit_incref(_M0L6_2atmpS2920);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsiE(_M0L1mS806, _M0L6_2atmpS2920, _M0L6_2atmpS2921);
      moonbit_decref(_M0L6_2atmpS2920);
      _M0L6_2atmpS2922 = _M0L2__S808 + 1;
      _M0L2__S808 = _M0L6_2atmpS2922;
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
  int32_t _M0L3endS2940;
  int32_t _M0L5startS2941;
  int32_t _M0L6lengthS811;
  int32_t _M0L8capacityS813;
  struct _M0TPB3MapGssE* _M0L1mS817;
  int32_t _M0L3endS2937;
  int32_t _M0L5startS2938;
  int32_t _M0L7_2abindS818;
  int32_t _M0L2__S819;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2940 = _M0L3arrS812.$2;
  _M0L5startS2941 = _M0L3arrS812.$1;
  _M0L6lengthS811 = _M0L3endS2940 - _M0L5startS2941;
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
    int32_t _M0L6_2atmpS2939;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2939 = _M0FPB21capacity__for__length(_M0L6lengthS811);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS813
    = _M0MPC13int3Int3max(_M0L11_2acapacityS816, _M0L6_2atmpS2939);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS817 = _M0FPB8new__mapGssE(_M0L8capacityS813);
  _M0L3endS2937 = _M0L3arrS812.$2;
  _M0L5startS2938 = _M0L3arrS812.$1;
  _M0L7_2abindS818 = _M0L3endS2937 - _M0L5startS2938;
  _M0L2__S819 = 0;
  while (1) {
    if (_M0L2__S819 < _M0L7_2abindS818) {
      struct _M0TUssE** _M0L3bufS2934 = _M0L3arrS812.$0;
      int32_t _M0L5startS2936 = _M0L3arrS812.$1;
      int32_t _M0L6_2atmpS2935 = _M0L5startS2936 + _M0L2__S819;
      struct _M0TUssE* _M0L1eS820 =
        (struct _M0TUssE*)_M0L3bufS2934[_M0L6_2atmpS2935];
      moonbit_string_t _M0L6_2atmpS2931 = _M0L1eS820->$0;
      moonbit_string_t _M0L6_2atmpS2932 = _M0L1eS820->$1;
      int32_t _M0L6_2atmpS2933;
      moonbit_incref(_M0L6_2atmpS2932);
      moonbit_incref(_M0L6_2atmpS2931);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGssE(_M0L1mS817, _M0L6_2atmpS2931, _M0L6_2atmpS2932);
      moonbit_decref(_M0L6_2atmpS2931);
      moonbit_decref(_M0L6_2atmpS2932);
      _M0L6_2atmpS2933 = _M0L2__S819 + 1;
      _M0L2__S819 = _M0L6_2atmpS2933;
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
  int32_t _M0L3endS2951;
  int32_t _M0L5startS2952;
  int32_t _M0L6lengthS822;
  int32_t _M0L8capacityS824;
  struct _M0TPB3MapGsbE* _M0L1mS828;
  int32_t _M0L3endS2948;
  int32_t _M0L5startS2949;
  int32_t _M0L7_2abindS829;
  int32_t _M0L2__S830;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2951 = _M0L3arrS823.$2;
  _M0L5startS2952 = _M0L3arrS823.$1;
  _M0L6lengthS822 = _M0L3endS2951 - _M0L5startS2952;
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
    int32_t _M0L6_2atmpS2950;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2950 = _M0FPB21capacity__for__length(_M0L6lengthS822);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS824
    = _M0MPC13int3Int3max(_M0L11_2acapacityS827, _M0L6_2atmpS2950);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS828 = _M0FPB8new__mapGsbE(_M0L8capacityS824);
  _M0L3endS2948 = _M0L3arrS823.$2;
  _M0L5startS2949 = _M0L3arrS823.$1;
  _M0L7_2abindS829 = _M0L3endS2948 - _M0L5startS2949;
  _M0L2__S830 = 0;
  while (1) {
    if (_M0L2__S830 < _M0L7_2abindS829) {
      struct _M0TUsbE** _M0L3bufS2945 = _M0L3arrS823.$0;
      int32_t _M0L5startS2947 = _M0L3arrS823.$1;
      int32_t _M0L6_2atmpS2946 = _M0L5startS2947 + _M0L2__S830;
      struct _M0TUsbE* _M0L1eS831 =
        (struct _M0TUsbE*)_M0L3bufS2945[_M0L6_2atmpS2946];
      moonbit_string_t _M0L6_2atmpS2942 = _M0L1eS831->$0;
      int32_t _M0L6_2atmpS2943 = _M0L1eS831->$1;
      int32_t _M0L6_2atmpS2944;
      moonbit_incref(_M0L6_2atmpS2942);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsbE(_M0L1mS828, _M0L6_2atmpS2942, _M0L6_2atmpS2943);
      moonbit_decref(_M0L6_2atmpS2942);
      _M0L6_2atmpS2944 = _M0L2__S830 + 1;
      _M0L2__S830 = _M0L6_2atmpS2944;
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
  int32_t _M0L3endS2962;
  int32_t _M0L5startS2963;
  int32_t _M0L6lengthS833;
  int32_t _M0L8capacityS835;
  struct _M0TPB3MapGsfE* _M0L1mS839;
  int32_t _M0L3endS2959;
  int32_t _M0L5startS2960;
  int32_t _M0L7_2abindS840;
  int32_t _M0L2__S841;
  #line 83 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3endS2962 = _M0L3arrS834.$2;
  _M0L5startS2963 = _M0L3arrS834.$1;
  _M0L6lengthS833 = _M0L3endS2962 - _M0L5startS2963;
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
    int32_t _M0L6_2atmpS2961;
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L6_2atmpS2961 = _M0FPB21capacity__for__length(_M0L6lengthS833);
    #line 90 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    _M0L8capacityS835
    = _M0MPC13int3Int3max(_M0L11_2acapacityS838, _M0L6_2atmpS2961);
  }
  #line 98 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L1mS839 = _M0FPB8new__mapGsfE(_M0L8capacityS835);
  _M0L3endS2959 = _M0L3arrS834.$2;
  _M0L5startS2960 = _M0L3arrS834.$1;
  _M0L7_2abindS840 = _M0L3endS2959 - _M0L5startS2960;
  _M0L2__S841 = 0;
  while (1) {
    if (_M0L2__S841 < _M0L7_2abindS840) {
      struct _M0TUsfE** _M0L3bufS2956 = _M0L3arrS834.$0;
      int32_t _M0L5startS2958 = _M0L3arrS834.$1;
      int32_t _M0L6_2atmpS2957 = _M0L5startS2958 + _M0L2__S841;
      struct _M0TUsfE* _M0L1eS842 =
        (struct _M0TUsfE*)_M0L3bufS2956[_M0L6_2atmpS2957];
      moonbit_string_t _M0L6_2atmpS2953 = _M0L1eS842->$0;
      float _M0L6_2atmpS2954 = _M0L1eS842->$1;
      int32_t _M0L6_2atmpS2955;
      moonbit_incref(_M0L6_2atmpS2953);
      #line 100 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map3setGsfE(_M0L1mS839, _M0L6_2atmpS2953, _M0L6_2atmpS2954);
      moonbit_decref(_M0L6_2atmpS2953);
      _M0L6_2atmpS2955 = _M0L2__S841 + 1;
      _M0L2__S841 = _M0L6_2atmpS2955;
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
  int32_t _M0L6_2atmpS2904;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2904 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS775);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRP19moonbitDB10RedisValueE(_M0L4selfS774, _M0L3keyS775, _M0L5valueS776, _M0L6_2atmpS2904);
  return 0;
}

int32_t _M0MPB3Map3setGssE(
  struct _M0TPB3MapGssE* _M0L4selfS777,
  moonbit_string_t _M0L3keyS778,
  moonbit_string_t _M0L5valueS779
) {
  int32_t _M0L6_2atmpS2905;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2905 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS778);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGssE(_M0L4selfS777, _M0L3keyS778, _M0L5valueS779, _M0L6_2atmpS2905);
  return 0;
}

int32_t _M0MPB3Map3setGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS780,
  moonbit_string_t _M0L3keyS781,
  int32_t _M0L5valueS782
) {
  int32_t _M0L6_2atmpS2906;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2906 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS781);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsbE(_M0L4selfS780, _M0L3keyS781, _M0L5valueS782, _M0L6_2atmpS2906);
  return 0;
}

int32_t _M0MPB3Map3setGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS783,
  moonbit_string_t _M0L3keyS784,
  float _M0L5valueS785
) {
  int32_t _M0L6_2atmpS2907;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2907 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS784);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsfE(_M0L4selfS783, _M0L3keyS784, _M0L5valueS785, _M0L6_2atmpS2907);
  return 0;
}

int32_t _M0MPB3Map3setGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS786,
  moonbit_string_t _M0L3keyS787,
  int32_t _M0L5valueS788
) {
  int32_t _M0L6_2atmpS2908;
  #line 127 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2908 = _M0IPC16string6StringPB4Hash4hash(_M0L3keyS787);
  #line 129 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsiE(_M0L4selfS786, _M0L3keyS787, _M0L5valueS788, _M0L6_2atmpS2908);
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRP19moonbitDB10RedisValueE(
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0L4selfS697,
  moonbit_string_t _M0L3keyS703,
  void* _M0L5valueS704,
  int32_t _M0L4hashS699
) {
  int32_t _M0L14capacity__maskS2831;
  int32_t _M0L6_2atmpS2830;
  int32_t _M0L3pslS694;
  int32_t _M0L3idxS695;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2831 = _M0L4selfS697->$3;
  _M0L6_2atmpS2830 = _M0L4hashS699 & _M0L14capacity__maskS2831;
  _M0L3pslS694 = 0;
  _M0L3idxS695 = _M0L6_2atmpS2830;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2829 =
      _M0L4selfS697->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS696;
    if (
      _M0L3idxS695 < 0
      || _M0L3idxS695 >= Moonbit_array_length(_M0L7entriesS2829)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS696
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2829[
        _M0L3idxS695
      ];
    if (_M0L7_2abindS696 == 0) {
      int32_t _M0L4sizeS2814 = _M0L4selfS697->$1;
      int32_t _M0L8grow__atS2815 = _M0L4selfS697->$4;
      int32_t _M0L7_2abindS700;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS701;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS702;
      if (_M0L4sizeS2814 >= _M0L8grow__atS2815) {
        int32_t _M0L14capacity__maskS2817;
        int32_t _M0L6_2atmpS2816;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsRP19moonbitDB10RedisValueE(_M0L4selfS697);
        _M0L14capacity__maskS2817 = _M0L4selfS697->$3;
        _M0L6_2atmpS2816 = _M0L4hashS699 & _M0L14capacity__maskS2817;
        _M0L3pslS694 = 0;
        _M0L3idxS695 = _M0L6_2atmpS2816;
        continue;
      }
      _M0L7_2abindS700 = _M0L4selfS697->$6;
      _M0L7_2abindS701 = 0;
      moonbit_incref(_M0L3keyS703);
      moonbit_incref(_M0L5valueS704);
      _M0L5entryS702
      = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE));
      Moonbit_object_header(_M0L5entryS702)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 78, 0);
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
      int32_t _M0L4hashS2819 = _M0L14_2acurr__entryS706->$3;
      int32_t _if__result_4603;
      int32_t _M0L3pslS2820;
      int32_t _M0L6_2atmpS2825;
      int32_t _M0L6_2atmpS2827;
      int32_t _M0L14capacity__maskS2828;
      int32_t _M0L6_2atmpS2826;
      if (_M0L4hashS2819 == _M0L4hashS699) {
        moonbit_string_t _M0L3keyS2818 = _M0L14_2acurr__entryS706->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4603
        = _M0L3keyS2818 == _M0L3keyS703
          || Moonbit_array_length(_M0L3keyS2818)
             == Moonbit_array_length(_M0L3keyS703)
             && 0
                == memcmp(_M0L3keyS2818, _M0L3keyS703, Moonbit_array_length(_M0L3keyS2818) * 2);
      } else {
        _if__result_4603 = 0;
      }
      if (_if__result_4603) {
        void* _M0L6_2aoldS4114 = _M0L14_2acurr__entryS706->$5;
        moonbit_incref(_M0L5valueS704);
        moonbit_decref(_M0L6_2aoldS4114);
        _M0L14_2acurr__entryS706->$5 = _M0L5valueS704;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS706);
      }
      _M0L3pslS2820 = _M0L14_2acurr__entryS706->$2;
      if (_M0L3pslS694 > _M0L3pslS2820) {
        int32_t _M0L4sizeS2821 = _M0L4selfS697->$1;
        int32_t _M0L8grow__atS2822 = _M0L4selfS697->$4;
        int32_t _M0L7_2abindS707;
        struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS708;
        struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS709;
        if (_M0L4sizeS2821 >= _M0L8grow__atS2822) {
          int32_t _M0L14capacity__maskS2824;
          int32_t _M0L6_2atmpS2823;
          moonbit_decref(_M0L14_2acurr__entryS706);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsRP19moonbitDB10RedisValueE(_M0L4selfS697);
          _M0L14capacity__maskS2824 = _M0L4selfS697->$3;
          _M0L6_2atmpS2823 = _M0L4hashS699 & _M0L14capacity__maskS2824;
          _M0L3pslS694 = 0;
          _M0L3idxS695 = _M0L6_2atmpS2823;
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
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 78, 0);
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
      _M0L6_2atmpS2825 = _M0L3pslS694 + 1;
      _M0L6_2atmpS2827 = _M0L3idxS695 + 1;
      _M0L14capacity__maskS2828 = _M0L4selfS697->$3;
      _M0L6_2atmpS2826 = _M0L6_2atmpS2827 & _M0L14capacity__maskS2828;
      _M0L3pslS694 = _M0L6_2atmpS2825;
      _M0L3idxS695 = _M0L6_2atmpS2826;
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
  int32_t _M0L14capacity__maskS2849;
  int32_t _M0L6_2atmpS2848;
  int32_t _M0L3pslS710;
  int32_t _M0L3idxS711;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2849 = _M0L4selfS713->$3;
  _M0L6_2atmpS2848 = _M0L4hashS715 & _M0L14capacity__maskS2849;
  _M0L3pslS710 = 0;
  _M0L3idxS711 = _M0L6_2atmpS2848;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2847 = _M0L4selfS713->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS712;
    if (
      _M0L3idxS711 < 0
      || _M0L3idxS711 >= Moonbit_array_length(_M0L7entriesS2847)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS712
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2847[_M0L3idxS711];
    if (_M0L7_2abindS712 == 0) {
      int32_t _M0L4sizeS2832 = _M0L4selfS713->$1;
      int32_t _M0L8grow__atS2833 = _M0L4selfS713->$4;
      int32_t _M0L7_2abindS716;
      struct _M0TPB5EntryGssE* _M0L7_2abindS717;
      struct _M0TPB5EntryGssE* _M0L5entryS718;
      if (_M0L4sizeS2832 >= _M0L8grow__atS2833) {
        int32_t _M0L14capacity__maskS2835;
        int32_t _M0L6_2atmpS2834;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGssE(_M0L4selfS713);
        _M0L14capacity__maskS2835 = _M0L4selfS713->$3;
        _M0L6_2atmpS2834 = _M0L4hashS715 & _M0L14capacity__maskS2835;
        _M0L3pslS710 = 0;
        _M0L3idxS711 = _M0L6_2atmpS2834;
        continue;
      }
      _M0L7_2abindS716 = _M0L4selfS713->$6;
      _M0L7_2abindS717 = 0;
      moonbit_incref(_M0L3keyS719);
      moonbit_incref(_M0L5valueS720);
      _M0L5entryS718
      = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
      Moonbit_object_header(_M0L5entryS718)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 83, 0);
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
      int32_t _M0L4hashS2837 = _M0L14_2acurr__entryS722->$3;
      int32_t _if__result_4605;
      int32_t _M0L3pslS2838;
      int32_t _M0L6_2atmpS2843;
      int32_t _M0L6_2atmpS2845;
      int32_t _M0L14capacity__maskS2846;
      int32_t _M0L6_2atmpS2844;
      if (_M0L4hashS2837 == _M0L4hashS715) {
        moonbit_string_t _M0L3keyS2836 = _M0L14_2acurr__entryS722->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4605
        = _M0L3keyS2836 == _M0L3keyS719
          || Moonbit_array_length(_M0L3keyS2836)
             == Moonbit_array_length(_M0L3keyS719)
             && 0
                == memcmp(_M0L3keyS2836, _M0L3keyS719, Moonbit_array_length(_M0L3keyS2836) * 2);
      } else {
        _if__result_4605 = 0;
      }
      if (_if__result_4605) {
        moonbit_string_t _M0L6_2aoldS4118 = _M0L14_2acurr__entryS722->$5;
        moonbit_incref(_M0L5valueS720);
        moonbit_decref(_M0L6_2aoldS4118);
        _M0L14_2acurr__entryS722->$5 = _M0L5valueS720;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS722);
      }
      _M0L3pslS2838 = _M0L14_2acurr__entryS722->$2;
      if (_M0L3pslS710 > _M0L3pslS2838) {
        int32_t _M0L4sizeS2839 = _M0L4selfS713->$1;
        int32_t _M0L8grow__atS2840 = _M0L4selfS713->$4;
        int32_t _M0L7_2abindS723;
        struct _M0TPB5EntryGssE* _M0L7_2abindS724;
        struct _M0TPB5EntryGssE* _M0L5entryS725;
        if (_M0L4sizeS2839 >= _M0L8grow__atS2840) {
          int32_t _M0L14capacity__maskS2842;
          int32_t _M0L6_2atmpS2841;
          moonbit_decref(_M0L14_2acurr__entryS722);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGssE(_M0L4selfS713);
          _M0L14capacity__maskS2842 = _M0L4selfS713->$3;
          _M0L6_2atmpS2841 = _M0L4hashS715 & _M0L14capacity__maskS2842;
          _M0L3pslS710 = 0;
          _M0L3idxS711 = _M0L6_2atmpS2841;
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
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 83, 0);
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
      _M0L6_2atmpS2843 = _M0L3pslS710 + 1;
      _M0L6_2atmpS2845 = _M0L3idxS711 + 1;
      _M0L14capacity__maskS2846 = _M0L4selfS713->$3;
      _M0L6_2atmpS2844 = _M0L6_2atmpS2845 & _M0L14capacity__maskS2846;
      _M0L3pslS710 = _M0L6_2atmpS2843;
      _M0L3idxS711 = _M0L6_2atmpS2844;
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
  int32_t _M0L14capacity__maskS2867;
  int32_t _M0L6_2atmpS2866;
  int32_t _M0L3pslS726;
  int32_t _M0L3idxS727;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2867 = _M0L4selfS729->$3;
  _M0L6_2atmpS2866 = _M0L4hashS731 & _M0L14capacity__maskS2867;
  _M0L3pslS726 = 0;
  _M0L3idxS727 = _M0L6_2atmpS2866;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2865 = _M0L4selfS729->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS728;
    if (
      _M0L3idxS727 < 0
      || _M0L3idxS727 >= Moonbit_array_length(_M0L7entriesS2865)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS728
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2865[_M0L3idxS727];
    if (_M0L7_2abindS728 == 0) {
      int32_t _M0L4sizeS2850 = _M0L4selfS729->$1;
      int32_t _M0L8grow__atS2851 = _M0L4selfS729->$4;
      int32_t _M0L7_2abindS732;
      struct _M0TPB5EntryGsbE* _M0L7_2abindS733;
      struct _M0TPB5EntryGsbE* _M0L5entryS734;
      if (_M0L4sizeS2850 >= _M0L8grow__atS2851) {
        int32_t _M0L14capacity__maskS2853;
        int32_t _M0L6_2atmpS2852;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsbE(_M0L4selfS729);
        _M0L14capacity__maskS2853 = _M0L4selfS729->$3;
        _M0L6_2atmpS2852 = _M0L4hashS731 & _M0L14capacity__maskS2853;
        _M0L3pslS726 = 0;
        _M0L3idxS727 = _M0L6_2atmpS2852;
        continue;
      }
      _M0L7_2abindS732 = _M0L4selfS729->$6;
      _M0L7_2abindS733 = 0;
      moonbit_incref(_M0L3keyS735);
      _M0L5entryS734
      = (struct _M0TPB5EntryGsbE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsbE));
      Moonbit_object_header(_M0L5entryS734)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 88, 0);
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
      int32_t _M0L4hashS2855 = _M0L14_2acurr__entryS738->$3;
      int32_t _if__result_4607;
      int32_t _M0L3pslS2856;
      int32_t _M0L6_2atmpS2861;
      int32_t _M0L6_2atmpS2863;
      int32_t _M0L14capacity__maskS2864;
      int32_t _M0L6_2atmpS2862;
      if (_M0L4hashS2855 == _M0L4hashS731) {
        moonbit_string_t _M0L3keyS2854 = _M0L14_2acurr__entryS738->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4607
        = _M0L3keyS2854 == _M0L3keyS735
          || Moonbit_array_length(_M0L3keyS2854)
             == Moonbit_array_length(_M0L3keyS735)
             && 0
                == memcmp(_M0L3keyS2854, _M0L3keyS735, Moonbit_array_length(_M0L3keyS2854) * 2);
      } else {
        _if__result_4607 = 0;
      }
      if (_if__result_4607) {
        _M0L14_2acurr__entryS738->$5 = _M0L5valueS736;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS738);
      }
      _M0L3pslS2856 = _M0L14_2acurr__entryS738->$2;
      if (_M0L3pslS726 > _M0L3pslS2856) {
        int32_t _M0L4sizeS2857 = _M0L4selfS729->$1;
        int32_t _M0L8grow__atS2858 = _M0L4selfS729->$4;
        int32_t _M0L7_2abindS739;
        struct _M0TPB5EntryGsbE* _M0L7_2abindS740;
        struct _M0TPB5EntryGsbE* _M0L5entryS741;
        if (_M0L4sizeS2857 >= _M0L8grow__atS2858) {
          int32_t _M0L14capacity__maskS2860;
          int32_t _M0L6_2atmpS2859;
          moonbit_decref(_M0L14_2acurr__entryS738);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsbE(_M0L4selfS729);
          _M0L14capacity__maskS2860 = _M0L4selfS729->$3;
          _M0L6_2atmpS2859 = _M0L4hashS731 & _M0L14capacity__maskS2860;
          _M0L3pslS726 = 0;
          _M0L3idxS727 = _M0L6_2atmpS2859;
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
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 88, 0);
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
      _M0L6_2atmpS2861 = _M0L3pslS726 + 1;
      _M0L6_2atmpS2863 = _M0L3idxS727 + 1;
      _M0L14capacity__maskS2864 = _M0L4selfS729->$3;
      _M0L6_2atmpS2862 = _M0L6_2atmpS2863 & _M0L14capacity__maskS2864;
      _M0L3pslS726 = _M0L6_2atmpS2861;
      _M0L3idxS727 = _M0L6_2atmpS2862;
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
  int32_t _M0L14capacity__maskS2885;
  int32_t _M0L6_2atmpS2884;
  int32_t _M0L3pslS742;
  int32_t _M0L3idxS743;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2885 = _M0L4selfS745->$3;
  _M0L6_2atmpS2884 = _M0L4hashS747 & _M0L14capacity__maskS2885;
  _M0L3pslS742 = 0;
  _M0L3idxS743 = _M0L6_2atmpS2884;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2883 = _M0L4selfS745->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS744;
    if (
      _M0L3idxS743 < 0
      || _M0L3idxS743 >= Moonbit_array_length(_M0L7entriesS2883)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS744
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2883[_M0L3idxS743];
    if (_M0L7_2abindS744 == 0) {
      int32_t _M0L4sizeS2868 = _M0L4selfS745->$1;
      int32_t _M0L8grow__atS2869 = _M0L4selfS745->$4;
      int32_t _M0L7_2abindS748;
      struct _M0TPB5EntryGsfE* _M0L7_2abindS749;
      struct _M0TPB5EntryGsfE* _M0L5entryS750;
      if (_M0L4sizeS2868 >= _M0L8grow__atS2869) {
        int32_t _M0L14capacity__maskS2871;
        int32_t _M0L6_2atmpS2870;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsfE(_M0L4selfS745);
        _M0L14capacity__maskS2871 = _M0L4selfS745->$3;
        _M0L6_2atmpS2870 = _M0L4hashS747 & _M0L14capacity__maskS2871;
        _M0L3pslS742 = 0;
        _M0L3idxS743 = _M0L6_2atmpS2870;
        continue;
      }
      _M0L7_2abindS748 = _M0L4selfS745->$6;
      _M0L7_2abindS749 = 0;
      moonbit_incref(_M0L3keyS751);
      _M0L5entryS750
      = (struct _M0TPB5EntryGsfE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsfE));
      Moonbit_object_header(_M0L5entryS750)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 92, 0);
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
      int32_t _M0L4hashS2873 = _M0L14_2acurr__entryS754->$3;
      int32_t _if__result_4609;
      int32_t _M0L3pslS2874;
      int32_t _M0L6_2atmpS2879;
      int32_t _M0L6_2atmpS2881;
      int32_t _M0L14capacity__maskS2882;
      int32_t _M0L6_2atmpS2880;
      if (_M0L4hashS2873 == _M0L4hashS747) {
        moonbit_string_t _M0L3keyS2872 = _M0L14_2acurr__entryS754->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4609
        = _M0L3keyS2872 == _M0L3keyS751
          || Moonbit_array_length(_M0L3keyS2872)
             == Moonbit_array_length(_M0L3keyS751)
             && 0
                == memcmp(_M0L3keyS2872, _M0L3keyS751, Moonbit_array_length(_M0L3keyS2872) * 2);
      } else {
        _if__result_4609 = 0;
      }
      if (_if__result_4609) {
        _M0L14_2acurr__entryS754->$5 = _M0L5valueS752;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS754);
      }
      _M0L3pslS2874 = _M0L14_2acurr__entryS754->$2;
      if (_M0L3pslS742 > _M0L3pslS2874) {
        int32_t _M0L4sizeS2875 = _M0L4selfS745->$1;
        int32_t _M0L8grow__atS2876 = _M0L4selfS745->$4;
        int32_t _M0L7_2abindS755;
        struct _M0TPB5EntryGsfE* _M0L7_2abindS756;
        struct _M0TPB5EntryGsfE* _M0L5entryS757;
        if (_M0L4sizeS2875 >= _M0L8grow__atS2876) {
          int32_t _M0L14capacity__maskS2878;
          int32_t _M0L6_2atmpS2877;
          moonbit_decref(_M0L14_2acurr__entryS754);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsfE(_M0L4selfS745);
          _M0L14capacity__maskS2878 = _M0L4selfS745->$3;
          _M0L6_2atmpS2877 = _M0L4hashS747 & _M0L14capacity__maskS2878;
          _M0L3pslS742 = 0;
          _M0L3idxS743 = _M0L6_2atmpS2877;
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
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 92, 0);
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
      _M0L6_2atmpS2879 = _M0L3pslS742 + 1;
      _M0L6_2atmpS2881 = _M0L3idxS743 + 1;
      _M0L14capacity__maskS2882 = _M0L4selfS745->$3;
      _M0L6_2atmpS2880 = _M0L6_2atmpS2881 & _M0L14capacity__maskS2882;
      _M0L3pslS742 = _M0L6_2atmpS2879;
      _M0L3idxS743 = _M0L6_2atmpS2880;
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
  int32_t _M0L14capacity__maskS2903;
  int32_t _M0L6_2atmpS2902;
  int32_t _M0L3pslS758;
  int32_t _M0L3idxS759;
  #line 133 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L14capacity__maskS2903 = _M0L4selfS761->$3;
  _M0L6_2atmpS2902 = _M0L4hashS763 & _M0L14capacity__maskS2903;
  _M0L3pslS758 = 0;
  _M0L3idxS759 = _M0L6_2atmpS2902;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2901 = _M0L4selfS761->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS760;
    if (
      _M0L3idxS759 < 0
      || _M0L3idxS759 >= Moonbit_array_length(_M0L7entriesS2901)
    ) {
      #line 141 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS760
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2901[_M0L3idxS759];
    if (_M0L7_2abindS760 == 0) {
      int32_t _M0L4sizeS2886 = _M0L4selfS761->$1;
      int32_t _M0L8grow__atS2887 = _M0L4selfS761->$4;
      int32_t _M0L7_2abindS764;
      struct _M0TPB5EntryGsiE* _M0L7_2abindS765;
      struct _M0TPB5EntryGsiE* _M0L5entryS766;
      if (_M0L4sizeS2886 >= _M0L8grow__atS2887) {
        int32_t _M0L14capacity__maskS2889;
        int32_t _M0L6_2atmpS2888;
        #line 145 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map4growGsiE(_M0L4selfS761);
        _M0L14capacity__maskS2889 = _M0L4selfS761->$3;
        _M0L6_2atmpS2888 = _M0L4hashS763 & _M0L14capacity__maskS2889;
        _M0L3pslS758 = 0;
        _M0L3idxS759 = _M0L6_2atmpS2888;
        continue;
      }
      _M0L7_2abindS764 = _M0L4selfS761->$6;
      _M0L7_2abindS765 = 0;
      moonbit_incref(_M0L3keyS767);
      _M0L5entryS766
      = (struct _M0TPB5EntryGsiE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsiE));
      Moonbit_object_header(_M0L5entryS766)->meta
      = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 96, 0);
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
      int32_t _M0L4hashS2891 = _M0L14_2acurr__entryS770->$3;
      int32_t _if__result_4611;
      int32_t _M0L3pslS2892;
      int32_t _M0L6_2atmpS2897;
      int32_t _M0L6_2atmpS2899;
      int32_t _M0L14capacity__maskS2900;
      int32_t _M0L6_2atmpS2898;
      if (_M0L4hashS2891 == _M0L4hashS763) {
        moonbit_string_t _M0L3keyS2890 = _M0L14_2acurr__entryS770->$4;
        #line 154 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _if__result_4611
        = _M0L3keyS2890 == _M0L3keyS767
          || Moonbit_array_length(_M0L3keyS2890)
             == Moonbit_array_length(_M0L3keyS767)
             && 0
                == memcmp(_M0L3keyS2890, _M0L3keyS767, Moonbit_array_length(_M0L3keyS2890) * 2);
      } else {
        _if__result_4611 = 0;
      }
      if (_if__result_4611) {
        _M0L14_2acurr__entryS770->$5 = _M0L5valueS768;
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS770);
      }
      _M0L3pslS2892 = _M0L14_2acurr__entryS770->$2;
      if (_M0L3pslS758 > _M0L3pslS2892) {
        int32_t _M0L4sizeS2893 = _M0L4selfS761->$1;
        int32_t _M0L8grow__atS2894 = _M0L4selfS761->$4;
        int32_t _M0L7_2abindS771;
        struct _M0TPB5EntryGsiE* _M0L7_2abindS772;
        struct _M0TPB5EntryGsiE* _M0L5entryS773;
        if (_M0L4sizeS2893 >= _M0L8grow__atS2894) {
          int32_t _M0L14capacity__maskS2896;
          int32_t _M0L6_2atmpS2895;
          moonbit_decref(_M0L14_2acurr__entryS770);
          #line 162 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
          _M0MPB3Map4growGsiE(_M0L4selfS761);
          _M0L14capacity__maskS2896 = _M0L4selfS761->$3;
          _M0L6_2atmpS2895 = _M0L4hashS763 & _M0L14capacity__maskS2896;
          _M0L3pslS758 = 0;
          _M0L3idxS759 = _M0L6_2atmpS2895;
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
        = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 96, 0);
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
      _M0L6_2atmpS2897 = _M0L3pslS758 + 1;
      _M0L6_2atmpS2899 = _M0L3idxS759 + 1;
      _M0L14capacity__maskS2900 = _M0L4selfS761->$3;
      _M0L6_2atmpS2898 = _M0L6_2atmpS2899 & _M0L14capacity__maskS2900;
      _M0L3pslS758 = _M0L6_2atmpS2897;
      _M0L3idxS759 = _M0L6_2atmpS2898;
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
  int32_t _M0L8capacityS2781;
  int32_t _M0L13new__capacityS656;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2775;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L6_2atmpS2774;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L6_2aoldS4134;
  int32_t _M0L6_2atmpS2776;
  int32_t _M0L8capacityS2778;
  int32_t _M0L6_2atmpS2777;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2779;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4133;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L1xS657;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS654 = _M0L4selfS655->$5;
  _M0L8capacityS2781 = _M0L4selfS655->$2;
  _M0L13new__capacityS656 = _M0L8capacityS2781 << 1;
  _M0L6_2atmpS2775 = 0;
  _M0L6_2atmpS2774
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE**)moonbit_make_ref_array(_M0L13new__capacityS656, _M0L6_2atmpS2775);
  _M0L6_2aoldS4134 = _M0L4selfS655->$0;
  if (_M0L9old__headS654) {
    moonbit_incref(_M0L9old__headS654);
  }
  moonbit_decref(_M0L6_2aoldS4134);
  _M0L4selfS655->$0 = _M0L6_2atmpS2774;
  _M0L4selfS655->$2 = _M0L13new__capacityS656;
  _M0L6_2atmpS2776 = _M0L13new__capacityS656 - 1;
  _M0L4selfS655->$3 = _M0L6_2atmpS2776;
  _M0L8capacityS2778 = _M0L4selfS655->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2777 = _M0FPB21calc__grow__threshold(_M0L8capacityS2778);
  _M0L4selfS655->$4 = _M0L6_2atmpS2777;
  _M0L4selfS655->$1 = 0;
  _M0L6_2atmpS2779 = 0;
  _M0L6_2aoldS4133 = _M0L4selfS655->$5;
  if (_M0L6_2aoldS4133) {
    moonbit_decref(_M0L6_2aoldS4133);
  }
  _M0L4selfS655->$5 = _M0L6_2atmpS2779;
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
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2780 = 0;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4131 =
        _M0L4_2aeS660->$1;
      if (_M0L15next__in__chainS661) {
        moonbit_incref(_M0L15next__in__chainS661);
      }
      if (_M0L6_2aoldS4131) {
        moonbit_decref(_M0L6_2aoldS4131);
      }
      _M0L4_2aeS660->$1 = _M0L6_2atmpS2780;
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
  int32_t _M0L8capacityS2789;
  int32_t _M0L13new__capacityS664;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2783;
  struct _M0TPB5EntryGssE** _M0L6_2atmpS2782;
  struct _M0TPB5EntryGssE** _M0L6_2aoldS4139;
  int32_t _M0L6_2atmpS2784;
  int32_t _M0L8capacityS2786;
  int32_t _M0L6_2atmpS2785;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2787;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS4138;
  struct _M0TPB5EntryGssE* _M0L1xS665;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS662 = _M0L4selfS663->$5;
  _M0L8capacityS2789 = _M0L4selfS663->$2;
  _M0L13new__capacityS664 = _M0L8capacityS2789 << 1;
  _M0L6_2atmpS2783 = 0;
  _M0L6_2atmpS2782
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L13new__capacityS664, _M0L6_2atmpS2783);
  _M0L6_2aoldS4139 = _M0L4selfS663->$0;
  if (_M0L9old__headS662) {
    moonbit_incref(_M0L9old__headS662);
  }
  moonbit_decref(_M0L6_2aoldS4139);
  _M0L4selfS663->$0 = _M0L6_2atmpS2782;
  _M0L4selfS663->$2 = _M0L13new__capacityS664;
  _M0L6_2atmpS2784 = _M0L13new__capacityS664 - 1;
  _M0L4selfS663->$3 = _M0L6_2atmpS2784;
  _M0L8capacityS2786 = _M0L4selfS663->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2785 = _M0FPB21calc__grow__threshold(_M0L8capacityS2786);
  _M0L4selfS663->$4 = _M0L6_2atmpS2785;
  _M0L4selfS663->$1 = 0;
  _M0L6_2atmpS2787 = 0;
  _M0L6_2aoldS4138 = _M0L4selfS663->$5;
  if (_M0L6_2aoldS4138) {
    moonbit_decref(_M0L6_2aoldS4138);
  }
  _M0L4selfS663->$5 = _M0L6_2atmpS2787;
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
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2788 = 0;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4136 = _M0L4_2aeS668->$1;
      if (_M0L15next__in__chainS669) {
        moonbit_incref(_M0L15next__in__chainS669);
      }
      if (_M0L6_2aoldS4136) {
        moonbit_decref(_M0L6_2aoldS4136);
      }
      _M0L4_2aeS668->$1 = _M0L6_2atmpS2788;
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
  int32_t _M0L8capacityS2797;
  int32_t _M0L13new__capacityS672;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2791;
  struct _M0TPB5EntryGsbE** _M0L6_2atmpS2790;
  struct _M0TPB5EntryGsbE** _M0L6_2aoldS4144;
  int32_t _M0L6_2atmpS2792;
  int32_t _M0L8capacityS2794;
  int32_t _M0L6_2atmpS2793;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2795;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS4143;
  struct _M0TPB5EntryGsbE* _M0L1xS673;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS670 = _M0L4selfS671->$5;
  _M0L8capacityS2797 = _M0L4selfS671->$2;
  _M0L13new__capacityS672 = _M0L8capacityS2797 << 1;
  _M0L6_2atmpS2791 = 0;
  _M0L6_2atmpS2790
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L13new__capacityS672, _M0L6_2atmpS2791);
  _M0L6_2aoldS4144 = _M0L4selfS671->$0;
  if (_M0L9old__headS670) {
    moonbit_incref(_M0L9old__headS670);
  }
  moonbit_decref(_M0L6_2aoldS4144);
  _M0L4selfS671->$0 = _M0L6_2atmpS2790;
  _M0L4selfS671->$2 = _M0L13new__capacityS672;
  _M0L6_2atmpS2792 = _M0L13new__capacityS672 - 1;
  _M0L4selfS671->$3 = _M0L6_2atmpS2792;
  _M0L8capacityS2794 = _M0L4selfS671->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2793 = _M0FPB21calc__grow__threshold(_M0L8capacityS2794);
  _M0L4selfS671->$4 = _M0L6_2atmpS2793;
  _M0L4selfS671->$1 = 0;
  _M0L6_2atmpS2795 = 0;
  _M0L6_2aoldS4143 = _M0L4selfS671->$5;
  if (_M0L6_2aoldS4143) {
    moonbit_decref(_M0L6_2aoldS4143);
  }
  _M0L4selfS671->$5 = _M0L6_2atmpS2795;
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
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2796 = 0;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4141 = _M0L4_2aeS676->$1;
      if (_M0L15next__in__chainS677) {
        moonbit_incref(_M0L15next__in__chainS677);
      }
      if (_M0L6_2aoldS4141) {
        moonbit_decref(_M0L6_2aoldS4141);
      }
      _M0L4_2aeS676->$1 = _M0L6_2atmpS2796;
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
  int32_t _M0L8capacityS2805;
  int32_t _M0L13new__capacityS680;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2799;
  struct _M0TPB5EntryGsfE** _M0L6_2atmpS2798;
  struct _M0TPB5EntryGsfE** _M0L6_2aoldS4149;
  int32_t _M0L6_2atmpS2800;
  int32_t _M0L8capacityS2802;
  int32_t _M0L6_2atmpS2801;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2803;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS4148;
  struct _M0TPB5EntryGsfE* _M0L1xS681;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS678 = _M0L4selfS679->$5;
  _M0L8capacityS2805 = _M0L4selfS679->$2;
  _M0L13new__capacityS680 = _M0L8capacityS2805 << 1;
  _M0L6_2atmpS2799 = 0;
  _M0L6_2atmpS2798
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L13new__capacityS680, _M0L6_2atmpS2799);
  _M0L6_2aoldS4149 = _M0L4selfS679->$0;
  if (_M0L9old__headS678) {
    moonbit_incref(_M0L9old__headS678);
  }
  moonbit_decref(_M0L6_2aoldS4149);
  _M0L4selfS679->$0 = _M0L6_2atmpS2798;
  _M0L4selfS679->$2 = _M0L13new__capacityS680;
  _M0L6_2atmpS2800 = _M0L13new__capacityS680 - 1;
  _M0L4selfS679->$3 = _M0L6_2atmpS2800;
  _M0L8capacityS2802 = _M0L4selfS679->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2801 = _M0FPB21calc__grow__threshold(_M0L8capacityS2802);
  _M0L4selfS679->$4 = _M0L6_2atmpS2801;
  _M0L4selfS679->$1 = 0;
  _M0L6_2atmpS2803 = 0;
  _M0L6_2aoldS4148 = _M0L4selfS679->$5;
  if (_M0L6_2aoldS4148) {
    moonbit_decref(_M0L6_2aoldS4148);
  }
  _M0L4selfS679->$5 = _M0L6_2atmpS2803;
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
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2804 = 0;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4146 = _M0L4_2aeS684->$1;
      if (_M0L15next__in__chainS685) {
        moonbit_incref(_M0L15next__in__chainS685);
      }
      if (_M0L6_2aoldS4146) {
        moonbit_decref(_M0L6_2aoldS4146);
      }
      _M0L4_2aeS684->$1 = _M0L6_2atmpS2804;
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
  int32_t _M0L8capacityS2813;
  int32_t _M0L13new__capacityS688;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2807;
  struct _M0TPB5EntryGsiE** _M0L6_2atmpS2806;
  struct _M0TPB5EntryGsiE** _M0L6_2aoldS4154;
  int32_t _M0L6_2atmpS2808;
  int32_t _M0L8capacityS2810;
  int32_t _M0L6_2atmpS2809;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2811;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS4153;
  struct _M0TPB5EntryGsiE* _M0L1xS689;
  #line 561 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L9old__headS686 = _M0L4selfS687->$5;
  _M0L8capacityS2813 = _M0L4selfS687->$2;
  _M0L13new__capacityS688 = _M0L8capacityS2813 << 1;
  _M0L6_2atmpS2807 = 0;
  _M0L6_2atmpS2806
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L13new__capacityS688, _M0L6_2atmpS2807);
  _M0L6_2aoldS4154 = _M0L4selfS687->$0;
  if (_M0L9old__headS686) {
    moonbit_incref(_M0L9old__headS686);
  }
  moonbit_decref(_M0L6_2aoldS4154);
  _M0L4selfS687->$0 = _M0L6_2atmpS2806;
  _M0L4selfS687->$2 = _M0L13new__capacityS688;
  _M0L6_2atmpS2808 = _M0L13new__capacityS688 - 1;
  _M0L4selfS687->$3 = _M0L6_2atmpS2808;
  _M0L8capacityS2810 = _M0L4selfS687->$2;
  #line 567 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2809 = _M0FPB21calc__grow__threshold(_M0L8capacityS2810);
  _M0L4selfS687->$4 = _M0L6_2atmpS2809;
  _M0L4selfS687->$1 = 0;
  _M0L6_2atmpS2811 = 0;
  _M0L6_2aoldS4153 = _M0L4selfS687->$5;
  if (_M0L6_2aoldS4153) {
    moonbit_decref(_M0L6_2aoldS4153);
  }
  _M0L4selfS687->$5 = _M0L6_2atmpS2811;
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
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2812 = 0;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4151 = _M0L4_2aeS692->$1;
      if (_M0L15next__in__chainS693) {
        moonbit_incref(_M0L15next__in__chainS693);
      }
      if (_M0L6_2aoldS4151) {
        moonbit_decref(_M0L6_2aoldS4151);
      }
      _M0L4_2aeS692->$1 = _M0L6_2atmpS2812;
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
  int32_t _M0L14capacity__maskS2733;
  int32_t _M0L6_2atmpS2732;
  int32_t _M0L3pslS611;
  int32_t _M0L3idxS612;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS609 = _M0L5outerS610->$3;
  _M0L14capacity__maskS2733 = _M0L4selfS614->$3;
  _M0L6_2atmpS2732 = _M0L4hashS609 & _M0L14capacity__maskS2733;
  _M0L3pslS611 = 0;
  _M0L3idxS612 = _M0L6_2atmpS2732;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2731 =
      _M0L4selfS614->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS613;
    if (
      _M0L3idxS612 < 0
      || _M0L3idxS612 >= Moonbit_array_length(_M0L7entriesS2731)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS613
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2731[
        _M0L3idxS612
      ];
    if (_M0L7_2abindS613 == 0) {
      int32_t _M0L4tailS2724;
      _M0L5outerS610->$2 = _M0L3pslS611;
      _M0L4tailS2724 = _M0L4selfS614->$6;
      _M0L5outerS610->$0 = _M0L4tailS2724;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(_M0L4selfS614, _M0L3idxS612, _M0L5outerS610);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2aSomeS615 =
        _M0L7_2abindS613;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2acurrS616 =
        _M0L7_2aSomeS615;
      int32_t _M0L3pslS2725 = _M0L7_2acurrS616->$2;
      if (_M0L3pslS611 > _M0L3pslS2725) {
        int32_t _M0L4tailS2726;
        moonbit_incref(_M0L7_2acurrS616);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP19moonbitDB10RedisValueE(_M0L4selfS614, _M0L3idxS612, _M0L7_2acurrS616);
        moonbit_decref(_M0L7_2acurrS616);
        _M0L5outerS610->$2 = _M0L3pslS611;
        _M0L4tailS2726 = _M0L4selfS614->$6;
        _M0L5outerS610->$0 = _M0L4tailS2726;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP19moonbitDB10RedisValueE(_M0L4selfS614, _M0L3idxS612, _M0L5outerS610);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2727 = _M0L3pslS611 + 1;
        int32_t _M0L6_2atmpS2729 = _M0L3idxS612 + 1;
        int32_t _M0L14capacity__maskS2730 = _M0L4selfS614->$3;
        int32_t _M0L6_2atmpS2728 =
          _M0L6_2atmpS2729 & _M0L14capacity__maskS2730;
        _M0L3pslS611 = _M0L6_2atmpS2727;
        _M0L3idxS612 = _M0L6_2atmpS2728;
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
  int32_t _M0L14capacity__maskS2743;
  int32_t _M0L6_2atmpS2742;
  int32_t _M0L3pslS620;
  int32_t _M0L3idxS621;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS618 = _M0L5outerS619->$3;
  _M0L14capacity__maskS2743 = _M0L4selfS623->$3;
  _M0L6_2atmpS2742 = _M0L4hashS618 & _M0L14capacity__maskS2743;
  _M0L3pslS620 = 0;
  _M0L3idxS621 = _M0L6_2atmpS2742;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2741 = _M0L4selfS623->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS622;
    if (
      _M0L3idxS621 < 0
      || _M0L3idxS621 >= Moonbit_array_length(_M0L7entriesS2741)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS622
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2741[_M0L3idxS621];
    if (_M0L7_2abindS622 == 0) {
      int32_t _M0L4tailS2734;
      _M0L5outerS619->$2 = _M0L3pslS620;
      _M0L4tailS2734 = _M0L4selfS623->$6;
      _M0L5outerS619->$0 = _M0L4tailS2734;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS623, _M0L3idxS621, _M0L5outerS619);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS624 = _M0L7_2abindS622;
      struct _M0TPB5EntryGssE* _M0L7_2acurrS625 = _M0L7_2aSomeS624;
      int32_t _M0L3pslS2735 = _M0L7_2acurrS625->$2;
      if (_M0L3pslS620 > _M0L3pslS2735) {
        int32_t _M0L4tailS2736;
        moonbit_incref(_M0L7_2acurrS625);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS623, _M0L3idxS621, _M0L7_2acurrS625);
        moonbit_decref(_M0L7_2acurrS625);
        _M0L5outerS619->$2 = _M0L3pslS620;
        _M0L4tailS2736 = _M0L4selfS623->$6;
        _M0L5outerS619->$0 = _M0L4tailS2736;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS623, _M0L3idxS621, _M0L5outerS619);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2737 = _M0L3pslS620 + 1;
        int32_t _M0L6_2atmpS2739 = _M0L3idxS621 + 1;
        int32_t _M0L14capacity__maskS2740 = _M0L4selfS623->$3;
        int32_t _M0L6_2atmpS2738 =
          _M0L6_2atmpS2739 & _M0L14capacity__maskS2740;
        _M0L3pslS620 = _M0L6_2atmpS2737;
        _M0L3idxS621 = _M0L6_2atmpS2738;
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
  int32_t _M0L14capacity__maskS2753;
  int32_t _M0L6_2atmpS2752;
  int32_t _M0L3pslS629;
  int32_t _M0L3idxS630;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS627 = _M0L5outerS628->$3;
  _M0L14capacity__maskS2753 = _M0L4selfS632->$3;
  _M0L6_2atmpS2752 = _M0L4hashS627 & _M0L14capacity__maskS2753;
  _M0L3pslS629 = 0;
  _M0L3idxS630 = _M0L6_2atmpS2752;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2751 = _M0L4selfS632->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS631;
    if (
      _M0L3idxS630 < 0
      || _M0L3idxS630 >= Moonbit_array_length(_M0L7entriesS2751)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS631
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2751[_M0L3idxS630];
    if (_M0L7_2abindS631 == 0) {
      int32_t _M0L4tailS2744;
      _M0L5outerS628->$2 = _M0L3pslS629;
      _M0L4tailS2744 = _M0L4selfS632->$6;
      _M0L5outerS628->$0 = _M0L4tailS2744;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS632, _M0L3idxS630, _M0L5outerS628);
      return 0;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS633 = _M0L7_2abindS631;
      struct _M0TPB5EntryGsbE* _M0L7_2acurrS634 = _M0L7_2aSomeS633;
      int32_t _M0L3pslS2745 = _M0L7_2acurrS634->$2;
      if (_M0L3pslS629 > _M0L3pslS2745) {
        int32_t _M0L4tailS2746;
        moonbit_incref(_M0L7_2acurrS634);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsbE(_M0L4selfS632, _M0L3idxS630, _M0L7_2acurrS634);
        moonbit_decref(_M0L7_2acurrS634);
        _M0L5outerS628->$2 = _M0L3pslS629;
        _M0L4tailS2746 = _M0L4selfS632->$6;
        _M0L5outerS628->$0 = _M0L4tailS2746;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsbE(_M0L4selfS632, _M0L3idxS630, _M0L5outerS628);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2747 = _M0L3pslS629 + 1;
        int32_t _M0L6_2atmpS2749 = _M0L3idxS630 + 1;
        int32_t _M0L14capacity__maskS2750 = _M0L4selfS632->$3;
        int32_t _M0L6_2atmpS2748 =
          _M0L6_2atmpS2749 & _M0L14capacity__maskS2750;
        _M0L3pslS629 = _M0L6_2atmpS2747;
        _M0L3idxS630 = _M0L6_2atmpS2748;
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
  int32_t _M0L14capacity__maskS2763;
  int32_t _M0L6_2atmpS2762;
  int32_t _M0L3pslS638;
  int32_t _M0L3idxS639;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS636 = _M0L5outerS637->$3;
  _M0L14capacity__maskS2763 = _M0L4selfS641->$3;
  _M0L6_2atmpS2762 = _M0L4hashS636 & _M0L14capacity__maskS2763;
  _M0L3pslS638 = 0;
  _M0L3idxS639 = _M0L6_2atmpS2762;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2761 = _M0L4selfS641->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS640;
    if (
      _M0L3idxS639 < 0
      || _M0L3idxS639 >= Moonbit_array_length(_M0L7entriesS2761)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS640
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2761[_M0L3idxS639];
    if (_M0L7_2abindS640 == 0) {
      int32_t _M0L4tailS2754;
      _M0L5outerS637->$2 = _M0L3pslS638;
      _M0L4tailS2754 = _M0L4selfS641->$6;
      _M0L5outerS637->$0 = _M0L4tailS2754;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS641, _M0L3idxS639, _M0L5outerS637);
      return 0;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS642 = _M0L7_2abindS640;
      struct _M0TPB5EntryGsfE* _M0L7_2acurrS643 = _M0L7_2aSomeS642;
      int32_t _M0L3pslS2755 = _M0L7_2acurrS643->$2;
      if (_M0L3pslS638 > _M0L3pslS2755) {
        int32_t _M0L4tailS2756;
        moonbit_incref(_M0L7_2acurrS643);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsfE(_M0L4selfS641, _M0L3idxS639, _M0L7_2acurrS643);
        moonbit_decref(_M0L7_2acurrS643);
        _M0L5outerS637->$2 = _M0L3pslS638;
        _M0L4tailS2756 = _M0L4selfS641->$6;
        _M0L5outerS637->$0 = _M0L4tailS2756;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsfE(_M0L4selfS641, _M0L3idxS639, _M0L5outerS637);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2757 = _M0L3pslS638 + 1;
        int32_t _M0L6_2atmpS2759 = _M0L3idxS639 + 1;
        int32_t _M0L14capacity__maskS2760 = _M0L4selfS641->$3;
        int32_t _M0L6_2atmpS2758 =
          _M0L6_2atmpS2759 & _M0L14capacity__maskS2760;
        _M0L3pslS638 = _M0L6_2atmpS2757;
        _M0L3idxS639 = _M0L6_2atmpS2758;
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
  int32_t _M0L14capacity__maskS2773;
  int32_t _M0L6_2atmpS2772;
  int32_t _M0L3pslS647;
  int32_t _M0L3idxS648;
  #line 585 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L4hashS645 = _M0L5outerS646->$3;
  _M0L14capacity__maskS2773 = _M0L4selfS650->$3;
  _M0L6_2atmpS2772 = _M0L4hashS645 & _M0L14capacity__maskS2773;
  _M0L3pslS647 = 0;
  _M0L3idxS648 = _M0L6_2atmpS2772;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2771 = _M0L4selfS650->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS649;
    if (
      _M0L3idxS648 < 0
      || _M0L3idxS648 >= Moonbit_array_length(_M0L7entriesS2771)
    ) {
      #line 588 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS649
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2771[_M0L3idxS648];
    if (_M0L7_2abindS649 == 0) {
      int32_t _M0L4tailS2764;
      _M0L5outerS646->$2 = _M0L3pslS647;
      _M0L4tailS2764 = _M0L4selfS650->$6;
      _M0L5outerS646->$0 = _M0L4tailS2764;
      #line 592 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS650, _M0L3idxS648, _M0L5outerS646);
      return 0;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS651 = _M0L7_2abindS649;
      struct _M0TPB5EntryGsiE* _M0L7_2acurrS652 = _M0L7_2aSomeS651;
      int32_t _M0L3pslS2765 = _M0L7_2acurrS652->$2;
      if (_M0L3pslS647 > _M0L3pslS2765) {
        int32_t _M0L4tailS2766;
        moonbit_incref(_M0L7_2acurrS652);
        #line 597 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsiE(_M0L4selfS650, _M0L3idxS648, _M0L7_2acurrS652);
        moonbit_decref(_M0L7_2acurrS652);
        _M0L5outerS646->$2 = _M0L3pslS647;
        _M0L4tailS2766 = _M0L4selfS650->$6;
        _M0L5outerS646->$0 = _M0L4tailS2766;
        #line 600 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsiE(_M0L4selfS650, _M0L3idxS648, _M0L5outerS646);
        return 0;
      } else {
        int32_t _M0L6_2atmpS2767 = _M0L3pslS647 + 1;
        int32_t _M0L6_2atmpS2769 = _M0L3idxS648 + 1;
        int32_t _M0L14capacity__maskS2770 = _M0L4selfS650->$3;
        int32_t _M0L6_2atmpS2768 =
          _M0L6_2atmpS2769 & _M0L14capacity__maskS2770;
        _M0L3pslS647 = _M0L6_2atmpS2767;
        _M0L3idxS648 = _M0L6_2atmpS2768;
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
  int32_t _M0L3pslS2659;
  int32_t _M0L6_2atmpS2655;
  int32_t _M0L6_2atmpS2657;
  int32_t _M0L14capacity__maskS2658;
  int32_t _M0L6_2atmpS2656;
  int32_t _M0L3pslS559;
  int32_t _M0L3idxS560;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L5entryS561;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2659 = _M0L5entryS567->$2;
  _M0L6_2atmpS2655 = _M0L3pslS2659 + 1;
  _M0L6_2atmpS2657 = _M0L3idxS568 + 1;
  _M0L14capacity__maskS2658 = _M0L4selfS563->$3;
  _M0L6_2atmpS2656 = _M0L6_2atmpS2657 & _M0L14capacity__maskS2658;
  moonbit_incref(_M0L5entryS567);
  _M0L3pslS559 = _M0L6_2atmpS2655;
  _M0L3idxS560 = _M0L6_2atmpS2656;
  _M0L5entryS561 = _M0L5entryS567;
  while (1) {
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2654 =
      _M0L4selfS563->$0;
    struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS562;
    if (
      _M0L3idxS560 < 0
      || _M0L3idxS560 >= Moonbit_array_length(_M0L7entriesS2654)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS562
    = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2654[
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
      int32_t _M0L3pslS2644 = _M0L14_2acurr__entryS566->$2;
      if (_M0L3pslS559 > _M0L3pslS2644) {
        int32_t _M0L3pslS2649;
        int32_t _M0L6_2atmpS2645;
        int32_t _M0L6_2atmpS2647;
        int32_t _M0L14capacity__maskS2648;
        int32_t _M0L6_2atmpS2646;
        _M0L5entryS561->$2 = _M0L3pslS559;
        moonbit_incref(_M0L14_2acurr__entryS566);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRP19moonbitDB10RedisValueE(_M0L4selfS563, _M0L5entryS561, _M0L3idxS560);
        moonbit_decref(_M0L5entryS561);
        _M0L3pslS2649 = _M0L14_2acurr__entryS566->$2;
        _M0L6_2atmpS2645 = _M0L3pslS2649 + 1;
        _M0L6_2atmpS2647 = _M0L3idxS560 + 1;
        _M0L14capacity__maskS2648 = _M0L4selfS563->$3;
        _M0L6_2atmpS2646 = _M0L6_2atmpS2647 & _M0L14capacity__maskS2648;
        _M0L3pslS559 = _M0L6_2atmpS2645;
        _M0L3idxS560 = _M0L6_2atmpS2646;
        _M0L5entryS561 = _M0L14_2acurr__entryS566;
        continue;
      } else {
        int32_t _M0L6_2atmpS2650 = _M0L3pslS559 + 1;
        int32_t _M0L6_2atmpS2652 = _M0L3idxS560 + 1;
        int32_t _M0L14capacity__maskS2653 = _M0L4selfS563->$3;
        int32_t _M0L6_2atmpS2651 =
          _M0L6_2atmpS2652 & _M0L14capacity__maskS2653;
        struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _tmp_4623 =
          _M0L5entryS561;
        _M0L3pslS559 = _M0L6_2atmpS2650;
        _M0L3idxS560 = _M0L6_2atmpS2651;
        _M0L5entryS561 = _tmp_4623;
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
  int32_t _M0L3pslS2675;
  int32_t _M0L6_2atmpS2671;
  int32_t _M0L6_2atmpS2673;
  int32_t _M0L14capacity__maskS2674;
  int32_t _M0L6_2atmpS2672;
  int32_t _M0L3pslS569;
  int32_t _M0L3idxS570;
  struct _M0TPB5EntryGssE* _M0L5entryS571;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2675 = _M0L5entryS577->$2;
  _M0L6_2atmpS2671 = _M0L3pslS2675 + 1;
  _M0L6_2atmpS2673 = _M0L3idxS578 + 1;
  _M0L14capacity__maskS2674 = _M0L4selfS573->$3;
  _M0L6_2atmpS2672 = _M0L6_2atmpS2673 & _M0L14capacity__maskS2674;
  moonbit_incref(_M0L5entryS577);
  _M0L3pslS569 = _M0L6_2atmpS2671;
  _M0L3idxS570 = _M0L6_2atmpS2672;
  _M0L5entryS571 = _M0L5entryS577;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L7entriesS2670 = _M0L4selfS573->$0;
    struct _M0TPB5EntryGssE* _M0L7_2abindS572;
    if (
      _M0L3idxS570 < 0
      || _M0L3idxS570 >= Moonbit_array_length(_M0L7entriesS2670)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS572
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2670[_M0L3idxS570];
    if (_M0L7_2abindS572 == 0) {
      _M0L5entryS571->$2 = _M0L3pslS569;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGssE(_M0L4selfS573, _M0L5entryS571, _M0L3idxS570);
      moonbit_decref(_M0L5entryS571);
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS575 = _M0L7_2abindS572;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS576 = _M0L7_2aSomeS575;
      int32_t _M0L3pslS2660 = _M0L14_2acurr__entryS576->$2;
      if (_M0L3pslS569 > _M0L3pslS2660) {
        int32_t _M0L3pslS2665;
        int32_t _M0L6_2atmpS2661;
        int32_t _M0L6_2atmpS2663;
        int32_t _M0L14capacity__maskS2664;
        int32_t _M0L6_2atmpS2662;
        _M0L5entryS571->$2 = _M0L3pslS569;
        moonbit_incref(_M0L14_2acurr__entryS576);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGssE(_M0L4selfS573, _M0L5entryS571, _M0L3idxS570);
        moonbit_decref(_M0L5entryS571);
        _M0L3pslS2665 = _M0L14_2acurr__entryS576->$2;
        _M0L6_2atmpS2661 = _M0L3pslS2665 + 1;
        _M0L6_2atmpS2663 = _M0L3idxS570 + 1;
        _M0L14capacity__maskS2664 = _M0L4selfS573->$3;
        _M0L6_2atmpS2662 = _M0L6_2atmpS2663 & _M0L14capacity__maskS2664;
        _M0L3pslS569 = _M0L6_2atmpS2661;
        _M0L3idxS570 = _M0L6_2atmpS2662;
        _M0L5entryS571 = _M0L14_2acurr__entryS576;
        continue;
      } else {
        int32_t _M0L6_2atmpS2666 = _M0L3pslS569 + 1;
        int32_t _M0L6_2atmpS2668 = _M0L3idxS570 + 1;
        int32_t _M0L14capacity__maskS2669 = _M0L4selfS573->$3;
        int32_t _M0L6_2atmpS2667 =
          _M0L6_2atmpS2668 & _M0L14capacity__maskS2669;
        struct _M0TPB5EntryGssE* _tmp_4625 = _M0L5entryS571;
        _M0L3pslS569 = _M0L6_2atmpS2666;
        _M0L3idxS570 = _M0L6_2atmpS2667;
        _M0L5entryS571 = _tmp_4625;
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
  int32_t _M0L3pslS2691;
  int32_t _M0L6_2atmpS2687;
  int32_t _M0L6_2atmpS2689;
  int32_t _M0L14capacity__maskS2690;
  int32_t _M0L6_2atmpS2688;
  int32_t _M0L3pslS579;
  int32_t _M0L3idxS580;
  struct _M0TPB5EntryGsbE* _M0L5entryS581;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2691 = _M0L5entryS587->$2;
  _M0L6_2atmpS2687 = _M0L3pslS2691 + 1;
  _M0L6_2atmpS2689 = _M0L3idxS588 + 1;
  _M0L14capacity__maskS2690 = _M0L4selfS583->$3;
  _M0L6_2atmpS2688 = _M0L6_2atmpS2689 & _M0L14capacity__maskS2690;
  moonbit_incref(_M0L5entryS587);
  _M0L3pslS579 = _M0L6_2atmpS2687;
  _M0L3idxS580 = _M0L6_2atmpS2688;
  _M0L5entryS581 = _M0L5entryS587;
  while (1) {
    struct _M0TPB5EntryGsbE** _M0L7entriesS2686 = _M0L4selfS583->$0;
    struct _M0TPB5EntryGsbE* _M0L7_2abindS582;
    if (
      _M0L3idxS580 < 0
      || _M0L3idxS580 >= Moonbit_array_length(_M0L7entriesS2686)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS582
    = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2686[_M0L3idxS580];
    if (_M0L7_2abindS582 == 0) {
      _M0L5entryS581->$2 = _M0L3pslS579;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsbE(_M0L4selfS583, _M0L5entryS581, _M0L3idxS580);
      moonbit_decref(_M0L5entryS581);
      break;
    } else {
      struct _M0TPB5EntryGsbE* _M0L7_2aSomeS585 = _M0L7_2abindS582;
      struct _M0TPB5EntryGsbE* _M0L14_2acurr__entryS586 = _M0L7_2aSomeS585;
      int32_t _M0L3pslS2676 = _M0L14_2acurr__entryS586->$2;
      if (_M0L3pslS579 > _M0L3pslS2676) {
        int32_t _M0L3pslS2681;
        int32_t _M0L6_2atmpS2677;
        int32_t _M0L6_2atmpS2679;
        int32_t _M0L14capacity__maskS2680;
        int32_t _M0L6_2atmpS2678;
        _M0L5entryS581->$2 = _M0L3pslS579;
        moonbit_incref(_M0L14_2acurr__entryS586);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsbE(_M0L4selfS583, _M0L5entryS581, _M0L3idxS580);
        moonbit_decref(_M0L5entryS581);
        _M0L3pslS2681 = _M0L14_2acurr__entryS586->$2;
        _M0L6_2atmpS2677 = _M0L3pslS2681 + 1;
        _M0L6_2atmpS2679 = _M0L3idxS580 + 1;
        _M0L14capacity__maskS2680 = _M0L4selfS583->$3;
        _M0L6_2atmpS2678 = _M0L6_2atmpS2679 & _M0L14capacity__maskS2680;
        _M0L3pslS579 = _M0L6_2atmpS2677;
        _M0L3idxS580 = _M0L6_2atmpS2678;
        _M0L5entryS581 = _M0L14_2acurr__entryS586;
        continue;
      } else {
        int32_t _M0L6_2atmpS2682 = _M0L3pslS579 + 1;
        int32_t _M0L6_2atmpS2684 = _M0L3idxS580 + 1;
        int32_t _M0L14capacity__maskS2685 = _M0L4selfS583->$3;
        int32_t _M0L6_2atmpS2683 =
          _M0L6_2atmpS2684 & _M0L14capacity__maskS2685;
        struct _M0TPB5EntryGsbE* _tmp_4627 = _M0L5entryS581;
        _M0L3pslS579 = _M0L6_2atmpS2682;
        _M0L3idxS580 = _M0L6_2atmpS2683;
        _M0L5entryS581 = _tmp_4627;
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
  int32_t _M0L3pslS2707;
  int32_t _M0L6_2atmpS2703;
  int32_t _M0L6_2atmpS2705;
  int32_t _M0L14capacity__maskS2706;
  int32_t _M0L6_2atmpS2704;
  int32_t _M0L3pslS589;
  int32_t _M0L3idxS590;
  struct _M0TPB5EntryGsfE* _M0L5entryS591;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2707 = _M0L5entryS597->$2;
  _M0L6_2atmpS2703 = _M0L3pslS2707 + 1;
  _M0L6_2atmpS2705 = _M0L3idxS598 + 1;
  _M0L14capacity__maskS2706 = _M0L4selfS593->$3;
  _M0L6_2atmpS2704 = _M0L6_2atmpS2705 & _M0L14capacity__maskS2706;
  moonbit_incref(_M0L5entryS597);
  _M0L3pslS589 = _M0L6_2atmpS2703;
  _M0L3idxS590 = _M0L6_2atmpS2704;
  _M0L5entryS591 = _M0L5entryS597;
  while (1) {
    struct _M0TPB5EntryGsfE** _M0L7entriesS2702 = _M0L4selfS593->$0;
    struct _M0TPB5EntryGsfE* _M0L7_2abindS592;
    if (
      _M0L3idxS590 < 0
      || _M0L3idxS590 >= Moonbit_array_length(_M0L7entriesS2702)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS592
    = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2702[_M0L3idxS590];
    if (_M0L7_2abindS592 == 0) {
      _M0L5entryS591->$2 = _M0L3pslS589;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsfE(_M0L4selfS593, _M0L5entryS591, _M0L3idxS590);
      moonbit_decref(_M0L5entryS591);
      break;
    } else {
      struct _M0TPB5EntryGsfE* _M0L7_2aSomeS595 = _M0L7_2abindS592;
      struct _M0TPB5EntryGsfE* _M0L14_2acurr__entryS596 = _M0L7_2aSomeS595;
      int32_t _M0L3pslS2692 = _M0L14_2acurr__entryS596->$2;
      if (_M0L3pslS589 > _M0L3pslS2692) {
        int32_t _M0L3pslS2697;
        int32_t _M0L6_2atmpS2693;
        int32_t _M0L6_2atmpS2695;
        int32_t _M0L14capacity__maskS2696;
        int32_t _M0L6_2atmpS2694;
        _M0L5entryS591->$2 = _M0L3pslS589;
        moonbit_incref(_M0L14_2acurr__entryS596);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsfE(_M0L4selfS593, _M0L5entryS591, _M0L3idxS590);
        moonbit_decref(_M0L5entryS591);
        _M0L3pslS2697 = _M0L14_2acurr__entryS596->$2;
        _M0L6_2atmpS2693 = _M0L3pslS2697 + 1;
        _M0L6_2atmpS2695 = _M0L3idxS590 + 1;
        _M0L14capacity__maskS2696 = _M0L4selfS593->$3;
        _M0L6_2atmpS2694 = _M0L6_2atmpS2695 & _M0L14capacity__maskS2696;
        _M0L3pslS589 = _M0L6_2atmpS2693;
        _M0L3idxS590 = _M0L6_2atmpS2694;
        _M0L5entryS591 = _M0L14_2acurr__entryS596;
        continue;
      } else {
        int32_t _M0L6_2atmpS2698 = _M0L3pslS589 + 1;
        int32_t _M0L6_2atmpS2700 = _M0L3idxS590 + 1;
        int32_t _M0L14capacity__maskS2701 = _M0L4selfS593->$3;
        int32_t _M0L6_2atmpS2699 =
          _M0L6_2atmpS2700 & _M0L14capacity__maskS2701;
        struct _M0TPB5EntryGsfE* _tmp_4629 = _M0L5entryS591;
        _M0L3pslS589 = _M0L6_2atmpS2698;
        _M0L3idxS590 = _M0L6_2atmpS2699;
        _M0L5entryS591 = _tmp_4629;
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
  int32_t _M0L3pslS2723;
  int32_t _M0L6_2atmpS2719;
  int32_t _M0L6_2atmpS2721;
  int32_t _M0L14capacity__maskS2722;
  int32_t _M0L6_2atmpS2720;
  int32_t _M0L3pslS599;
  int32_t _M0L3idxS600;
  struct _M0TPB5EntryGsiE* _M0L5entryS601;
  #line 178 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L3pslS2723 = _M0L5entryS607->$2;
  _M0L6_2atmpS2719 = _M0L3pslS2723 + 1;
  _M0L6_2atmpS2721 = _M0L3idxS608 + 1;
  _M0L14capacity__maskS2722 = _M0L4selfS603->$3;
  _M0L6_2atmpS2720 = _M0L6_2atmpS2721 & _M0L14capacity__maskS2722;
  moonbit_incref(_M0L5entryS607);
  _M0L3pslS599 = _M0L6_2atmpS2719;
  _M0L3idxS600 = _M0L6_2atmpS2720;
  _M0L5entryS601 = _M0L5entryS607;
  while (1) {
    struct _M0TPB5EntryGsiE** _M0L7entriesS2718 = _M0L4selfS603->$0;
    struct _M0TPB5EntryGsiE* _M0L7_2abindS602;
    if (
      _M0L3idxS600 < 0
      || _M0L3idxS600 >= Moonbit_array_length(_M0L7entriesS2718)
    ) {
      #line 184 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L7_2abindS602
    = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2718[_M0L3idxS600];
    if (_M0L7_2abindS602 == 0) {
      _M0L5entryS601->$2 = _M0L3pslS599;
      #line 187 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsiE(_M0L4selfS603, _M0L5entryS601, _M0L3idxS600);
      moonbit_decref(_M0L5entryS601);
      break;
    } else {
      struct _M0TPB5EntryGsiE* _M0L7_2aSomeS605 = _M0L7_2abindS602;
      struct _M0TPB5EntryGsiE* _M0L14_2acurr__entryS606 = _M0L7_2aSomeS605;
      int32_t _M0L3pslS2708 = _M0L14_2acurr__entryS606->$2;
      if (_M0L3pslS599 > _M0L3pslS2708) {
        int32_t _M0L3pslS2713;
        int32_t _M0L6_2atmpS2709;
        int32_t _M0L6_2atmpS2711;
        int32_t _M0L14capacity__maskS2712;
        int32_t _M0L6_2atmpS2710;
        _M0L5entryS601->$2 = _M0L3pslS599;
        moonbit_incref(_M0L14_2acurr__entryS606);
        #line 193 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsiE(_M0L4selfS603, _M0L5entryS601, _M0L3idxS600);
        moonbit_decref(_M0L5entryS601);
        _M0L3pslS2713 = _M0L14_2acurr__entryS606->$2;
        _M0L6_2atmpS2709 = _M0L3pslS2713 + 1;
        _M0L6_2atmpS2711 = _M0L3idxS600 + 1;
        _M0L14capacity__maskS2712 = _M0L4selfS603->$3;
        _M0L6_2atmpS2710 = _M0L6_2atmpS2711 & _M0L14capacity__maskS2712;
        _M0L3pslS599 = _M0L6_2atmpS2709;
        _M0L3idxS600 = _M0L6_2atmpS2710;
        _M0L5entryS601 = _M0L14_2acurr__entryS606;
        continue;
      } else {
        int32_t _M0L6_2atmpS2714 = _M0L3pslS599 + 1;
        int32_t _M0L6_2atmpS2716 = _M0L3idxS600 + 1;
        int32_t _M0L14capacity__maskS2717 = _M0L4selfS603->$3;
        int32_t _M0L6_2atmpS2715 =
          _M0L6_2atmpS2716 & _M0L14capacity__maskS2717;
        struct _M0TPB5EntryGsiE* _tmp_4631 = _M0L5entryS601;
        _M0L3pslS599 = _M0L6_2atmpS2714;
        _M0L3idxS600 = _M0L6_2atmpS2715;
        _M0L5entryS601 = _tmp_4631;
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
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2634;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2635;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4177;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS532;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2634 = _M0L4selfS529->$0;
  _M0L6_2atmpS2635 = _M0L5entryS531;
  if (
    _M0L8new__idxS530 < 0
    || _M0L8new__idxS530 >= Moonbit_array_length(_M0L7entriesS2634)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4177
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2634[
      _M0L8new__idxS530
    ];
  if (_M0L6_2atmpS2635) {
    moonbit_incref(_M0L6_2atmpS2635);
  }
  if (_M0L6_2aoldS4177) {
    moonbit_decref(_M0L6_2aoldS4177);
  }
  _M0L7entriesS2634[_M0L8new__idxS530] = _M0L6_2atmpS2635;
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
  struct _M0TPB5EntryGsiE** _M0L7entriesS2636;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2637;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS4180;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS538;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2636 = _M0L4selfS535->$0;
  _M0L6_2atmpS2637 = _M0L5entryS537;
  if (
    _M0L8new__idxS536 < 0
    || _M0L8new__idxS536 >= Moonbit_array_length(_M0L7entriesS2636)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4180
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2636[_M0L8new__idxS536];
  if (_M0L6_2atmpS2637) {
    moonbit_incref(_M0L6_2atmpS2637);
  }
  if (_M0L6_2aoldS4180) {
    moonbit_decref(_M0L6_2aoldS4180);
  }
  _M0L7entriesS2636[_M0L8new__idxS536] = _M0L6_2atmpS2637;
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
  struct _M0TPB5EntryGssE** _M0L7entriesS2638;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2639;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS4183;
  struct _M0TPB5EntryGssE* _M0L7_2abindS544;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2638 = _M0L4selfS541->$0;
  _M0L6_2atmpS2639 = _M0L5entryS543;
  if (
    _M0L8new__idxS542 < 0
    || _M0L8new__idxS542 >= Moonbit_array_length(_M0L7entriesS2638)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4183
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2638[_M0L8new__idxS542];
  if (_M0L6_2atmpS2639) {
    moonbit_incref(_M0L6_2atmpS2639);
  }
  if (_M0L6_2aoldS4183) {
    moonbit_decref(_M0L6_2aoldS4183);
  }
  _M0L7entriesS2638[_M0L8new__idxS542] = _M0L6_2atmpS2639;
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
  struct _M0TPB5EntryGsbE** _M0L7entriesS2640;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2641;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS4186;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS550;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2640 = _M0L4selfS547->$0;
  _M0L6_2atmpS2641 = _M0L5entryS549;
  if (
    _M0L8new__idxS548 < 0
    || _M0L8new__idxS548 >= Moonbit_array_length(_M0L7entriesS2640)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4186
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2640[_M0L8new__idxS548];
  if (_M0L6_2atmpS2641) {
    moonbit_incref(_M0L6_2atmpS2641);
  }
  if (_M0L6_2aoldS4186) {
    moonbit_decref(_M0L6_2aoldS4186);
  }
  _M0L7entriesS2640[_M0L8new__idxS548] = _M0L6_2atmpS2641;
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
  struct _M0TPB5EntryGsfE** _M0L7entriesS2642;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2643;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS4189;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS556;
  #line 205 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7entriesS2642 = _M0L4selfS553->$0;
  _M0L6_2atmpS2643 = _M0L5entryS555;
  if (
    _M0L8new__idxS554 < 0
    || _M0L8new__idxS554 >= Moonbit_array_length(_M0L7entriesS2642)
  ) {
    #line 210 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4189
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2642[_M0L8new__idxS554];
  if (_M0L6_2atmpS2643) {
    moonbit_incref(_M0L6_2atmpS2643);
  }
  if (_M0L6_2aoldS4189) {
    moonbit_decref(_M0L6_2aoldS4189);
  }
  _M0L7entriesS2642[_M0L8new__idxS554] = _M0L6_2atmpS2643;
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
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2594;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2595;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4191;
  int32_t _M0L4sizeS2597;
  int32_t _M0L6_2atmpS2596;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS509 = _M0L4selfS510->$6;
  switch (_M0L7_2abindS509) {
    case -1: {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2589 =
        _M0L5entryS511;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4193 =
        _M0L4selfS510->$5;
      if (_M0L6_2atmpS2589) {
        moonbit_incref(_M0L6_2atmpS2589);
      }
      if (_M0L6_2aoldS4193) {
        moonbit_decref(_M0L6_2aoldS4193);
      }
      _M0L4selfS510->$5 = _M0L6_2atmpS2589;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7entriesS2593 =
        _M0L4selfS510->$0;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2592;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2590;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2591;
      struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2aoldS4194;
      if (
        _M0L7_2abindS509 < 0
        || _M0L7_2abindS509 >= Moonbit_array_length(_M0L7entriesS2593)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2592
      = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2593[
          _M0L7_2abindS509
        ];
      if (_M0L6_2atmpS2592) {
        moonbit_incref(_M0L6_2atmpS2592);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2590
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP19moonbitDB10RedisValueEE(_M0L6_2atmpS2592);
      if (_M0L6_2atmpS2592) {
        moonbit_decref(_M0L6_2atmpS2592);
      }
      _M0L6_2atmpS2591 = _M0L5entryS511;
      _M0L6_2aoldS4194 = _M0L6_2atmpS2590->$1;
      if (_M0L6_2atmpS2591) {
        moonbit_incref(_M0L6_2atmpS2591);
      }
      if (_M0L6_2aoldS4194) {
        moonbit_decref(_M0L6_2aoldS4194);
      }
      _M0L6_2atmpS2590->$1 = _M0L6_2atmpS2591;
      moonbit_decref(_M0L6_2atmpS2590);
      break;
    }
  }
  _M0L4selfS510->$6 = _M0L3idxS512;
  _M0L7entriesS2594 = _M0L4selfS510->$0;
  _M0L6_2atmpS2595 = _M0L5entryS511;
  if (
    _M0L3idxS512 < 0
    || _M0L3idxS512 >= Moonbit_array_length(_M0L7entriesS2594)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4191
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE*)_M0L7entriesS2594[
      _M0L3idxS512
    ];
  if (_M0L6_2atmpS2595) {
    moonbit_incref(_M0L6_2atmpS2595);
  }
  if (_M0L6_2aoldS4191) {
    moonbit_decref(_M0L6_2aoldS4191);
  }
  _M0L7entriesS2594[_M0L3idxS512] = _M0L6_2atmpS2595;
  _M0L4sizeS2597 = _M0L4selfS510->$1;
  _M0L6_2atmpS2596 = _M0L4sizeS2597 + 1;
  _M0L4selfS510->$1 = _M0L6_2atmpS2596;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGssE(
  struct _M0TPB3MapGssE* _M0L4selfS514,
  int32_t _M0L3idxS516,
  struct _M0TPB5EntryGssE* _M0L5entryS515
) {
  int32_t _M0L7_2abindS513;
  struct _M0TPB5EntryGssE** _M0L7entriesS2603;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2604;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS4197;
  int32_t _M0L4sizeS2606;
  int32_t _M0L6_2atmpS2605;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS513 = _M0L4selfS514->$6;
  switch (_M0L7_2abindS513) {
    case -1: {
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2598 = _M0L5entryS515;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4199 = _M0L4selfS514->$5;
      if (_M0L6_2atmpS2598) {
        moonbit_incref(_M0L6_2atmpS2598);
      }
      if (_M0L6_2aoldS4199) {
        moonbit_decref(_M0L6_2aoldS4199);
      }
      _M0L4selfS514->$5 = _M0L6_2atmpS2598;
      break;
    }
    default: {
      struct _M0TPB5EntryGssE** _M0L7entriesS2602 = _M0L4selfS514->$0;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2601;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2599;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2600;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS4200;
      if (
        _M0L7_2abindS513 < 0
        || _M0L7_2abindS513 >= Moonbit_array_length(_M0L7entriesS2602)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2601
      = (struct _M0TPB5EntryGssE*)_M0L7entriesS2602[_M0L7_2abindS513];
      if (_M0L6_2atmpS2601) {
        moonbit_incref(_M0L6_2atmpS2601);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2599
      = _M0MPC16option6Option6unwrapGRPB5EntryGssEE(_M0L6_2atmpS2601);
      if (_M0L6_2atmpS2601) {
        moonbit_decref(_M0L6_2atmpS2601);
      }
      _M0L6_2atmpS2600 = _M0L5entryS515;
      _M0L6_2aoldS4200 = _M0L6_2atmpS2599->$1;
      if (_M0L6_2atmpS2600) {
        moonbit_incref(_M0L6_2atmpS2600);
      }
      if (_M0L6_2aoldS4200) {
        moonbit_decref(_M0L6_2aoldS4200);
      }
      _M0L6_2atmpS2599->$1 = _M0L6_2atmpS2600;
      moonbit_decref(_M0L6_2atmpS2599);
      break;
    }
  }
  _M0L4selfS514->$6 = _M0L3idxS516;
  _M0L7entriesS2603 = _M0L4selfS514->$0;
  _M0L6_2atmpS2604 = _M0L5entryS515;
  if (
    _M0L3idxS516 < 0
    || _M0L3idxS516 >= Moonbit_array_length(_M0L7entriesS2603)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4197
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS2603[_M0L3idxS516];
  if (_M0L6_2atmpS2604) {
    moonbit_incref(_M0L6_2atmpS2604);
  }
  if (_M0L6_2aoldS4197) {
    moonbit_decref(_M0L6_2aoldS4197);
  }
  _M0L7entriesS2603[_M0L3idxS516] = _M0L6_2atmpS2604;
  _M0L4sizeS2606 = _M0L4selfS514->$1;
  _M0L6_2atmpS2605 = _M0L4sizeS2606 + 1;
  _M0L4selfS514->$1 = _M0L6_2atmpS2605;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsbE(
  struct _M0TPB3MapGsbE* _M0L4selfS518,
  int32_t _M0L3idxS520,
  struct _M0TPB5EntryGsbE* _M0L5entryS519
) {
  int32_t _M0L7_2abindS517;
  struct _M0TPB5EntryGsbE** _M0L7entriesS2612;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2613;
  struct _M0TPB5EntryGsbE* _M0L6_2aoldS4203;
  int32_t _M0L4sizeS2615;
  int32_t _M0L6_2atmpS2614;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS517 = _M0L4selfS518->$6;
  switch (_M0L7_2abindS517) {
    case -1: {
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2607 = _M0L5entryS519;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4205 = _M0L4selfS518->$5;
      if (_M0L6_2atmpS2607) {
        moonbit_incref(_M0L6_2atmpS2607);
      }
      if (_M0L6_2aoldS4205) {
        moonbit_decref(_M0L6_2aoldS4205);
      }
      _M0L4selfS518->$5 = _M0L6_2atmpS2607;
      break;
    }
    default: {
      struct _M0TPB5EntryGsbE** _M0L7entriesS2611 = _M0L4selfS518->$0;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2610;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2608;
      struct _M0TPB5EntryGsbE* _M0L6_2atmpS2609;
      struct _M0TPB5EntryGsbE* _M0L6_2aoldS4206;
      if (
        _M0L7_2abindS517 < 0
        || _M0L7_2abindS517 >= Moonbit_array_length(_M0L7entriesS2611)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2610
      = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2611[_M0L7_2abindS517];
      if (_M0L6_2atmpS2610) {
        moonbit_incref(_M0L6_2atmpS2610);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2608
      = _M0MPC16option6Option6unwrapGRPB5EntryGsbEE(_M0L6_2atmpS2610);
      if (_M0L6_2atmpS2610) {
        moonbit_decref(_M0L6_2atmpS2610);
      }
      _M0L6_2atmpS2609 = _M0L5entryS519;
      _M0L6_2aoldS4206 = _M0L6_2atmpS2608->$1;
      if (_M0L6_2atmpS2609) {
        moonbit_incref(_M0L6_2atmpS2609);
      }
      if (_M0L6_2aoldS4206) {
        moonbit_decref(_M0L6_2aoldS4206);
      }
      _M0L6_2atmpS2608->$1 = _M0L6_2atmpS2609;
      moonbit_decref(_M0L6_2atmpS2608);
      break;
    }
  }
  _M0L4selfS518->$6 = _M0L3idxS520;
  _M0L7entriesS2612 = _M0L4selfS518->$0;
  _M0L6_2atmpS2613 = _M0L5entryS519;
  if (
    _M0L3idxS520 < 0
    || _M0L3idxS520 >= Moonbit_array_length(_M0L7entriesS2612)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4203
  = (struct _M0TPB5EntryGsbE*)_M0L7entriesS2612[_M0L3idxS520];
  if (_M0L6_2atmpS2613) {
    moonbit_incref(_M0L6_2atmpS2613);
  }
  if (_M0L6_2aoldS4203) {
    moonbit_decref(_M0L6_2aoldS4203);
  }
  _M0L7entriesS2612[_M0L3idxS520] = _M0L6_2atmpS2613;
  _M0L4sizeS2615 = _M0L4selfS518->$1;
  _M0L6_2atmpS2614 = _M0L4sizeS2615 + 1;
  _M0L4selfS518->$1 = _M0L6_2atmpS2614;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsfE(
  struct _M0TPB3MapGsfE* _M0L4selfS522,
  int32_t _M0L3idxS524,
  struct _M0TPB5EntryGsfE* _M0L5entryS523
) {
  int32_t _M0L7_2abindS521;
  struct _M0TPB5EntryGsfE** _M0L7entriesS2621;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2622;
  struct _M0TPB5EntryGsfE* _M0L6_2aoldS4209;
  int32_t _M0L4sizeS2624;
  int32_t _M0L6_2atmpS2623;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS521 = _M0L4selfS522->$6;
  switch (_M0L7_2abindS521) {
    case -1: {
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2616 = _M0L5entryS523;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4211 = _M0L4selfS522->$5;
      if (_M0L6_2atmpS2616) {
        moonbit_incref(_M0L6_2atmpS2616);
      }
      if (_M0L6_2aoldS4211) {
        moonbit_decref(_M0L6_2aoldS4211);
      }
      _M0L4selfS522->$5 = _M0L6_2atmpS2616;
      break;
    }
    default: {
      struct _M0TPB5EntryGsfE** _M0L7entriesS2620 = _M0L4selfS522->$0;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2619;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2617;
      struct _M0TPB5EntryGsfE* _M0L6_2atmpS2618;
      struct _M0TPB5EntryGsfE* _M0L6_2aoldS4212;
      if (
        _M0L7_2abindS521 < 0
        || _M0L7_2abindS521 >= Moonbit_array_length(_M0L7entriesS2620)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2619
      = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2620[_M0L7_2abindS521];
      if (_M0L6_2atmpS2619) {
        moonbit_incref(_M0L6_2atmpS2619);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2617
      = _M0MPC16option6Option6unwrapGRPB5EntryGsfEE(_M0L6_2atmpS2619);
      if (_M0L6_2atmpS2619) {
        moonbit_decref(_M0L6_2atmpS2619);
      }
      _M0L6_2atmpS2618 = _M0L5entryS523;
      _M0L6_2aoldS4212 = _M0L6_2atmpS2617->$1;
      if (_M0L6_2atmpS2618) {
        moonbit_incref(_M0L6_2atmpS2618);
      }
      if (_M0L6_2aoldS4212) {
        moonbit_decref(_M0L6_2aoldS4212);
      }
      _M0L6_2atmpS2617->$1 = _M0L6_2atmpS2618;
      moonbit_decref(_M0L6_2atmpS2617);
      break;
    }
  }
  _M0L4selfS522->$6 = _M0L3idxS524;
  _M0L7entriesS2621 = _M0L4selfS522->$0;
  _M0L6_2atmpS2622 = _M0L5entryS523;
  if (
    _M0L3idxS524 < 0
    || _M0L3idxS524 >= Moonbit_array_length(_M0L7entriesS2621)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4209
  = (struct _M0TPB5EntryGsfE*)_M0L7entriesS2621[_M0L3idxS524];
  if (_M0L6_2atmpS2622) {
    moonbit_incref(_M0L6_2atmpS2622);
  }
  if (_M0L6_2aoldS4209) {
    moonbit_decref(_M0L6_2aoldS4209);
  }
  _M0L7entriesS2621[_M0L3idxS524] = _M0L6_2atmpS2622;
  _M0L4sizeS2624 = _M0L4selfS522->$1;
  _M0L6_2atmpS2623 = _M0L4sizeS2624 + 1;
  _M0L4selfS522->$1 = _M0L6_2atmpS2623;
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsiE(
  struct _M0TPB3MapGsiE* _M0L4selfS526,
  int32_t _M0L3idxS528,
  struct _M0TPB5EntryGsiE* _M0L5entryS527
) {
  int32_t _M0L7_2abindS525;
  struct _M0TPB5EntryGsiE** _M0L7entriesS2630;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2631;
  struct _M0TPB5EntryGsiE* _M0L6_2aoldS4215;
  int32_t _M0L4sizeS2633;
  int32_t _M0L6_2atmpS2632;
  #line 516 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS525 = _M0L4selfS526->$6;
  switch (_M0L7_2abindS525) {
    case -1: {
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2625 = _M0L5entryS527;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4217 = _M0L4selfS526->$5;
      if (_M0L6_2atmpS2625) {
        moonbit_incref(_M0L6_2atmpS2625);
      }
      if (_M0L6_2aoldS4217) {
        moonbit_decref(_M0L6_2aoldS4217);
      }
      _M0L4selfS526->$5 = _M0L6_2atmpS2625;
      break;
    }
    default: {
      struct _M0TPB5EntryGsiE** _M0L7entriesS2629 = _M0L4selfS526->$0;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2628;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2626;
      struct _M0TPB5EntryGsiE* _M0L6_2atmpS2627;
      struct _M0TPB5EntryGsiE* _M0L6_2aoldS4218;
      if (
        _M0L7_2abindS525 < 0
        || _M0L7_2abindS525 >= Moonbit_array_length(_M0L7entriesS2629)
      ) {
        #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2628
      = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2629[_M0L7_2abindS525];
      if (_M0L6_2atmpS2628) {
        moonbit_incref(_M0L6_2atmpS2628);
      }
      #line 523 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
      _M0L6_2atmpS2626
      = _M0MPC16option6Option6unwrapGRPB5EntryGsiEE(_M0L6_2atmpS2628);
      if (_M0L6_2atmpS2628) {
        moonbit_decref(_M0L6_2atmpS2628);
      }
      _M0L6_2atmpS2627 = _M0L5entryS527;
      _M0L6_2aoldS4218 = _M0L6_2atmpS2626->$1;
      if (_M0L6_2atmpS2627) {
        moonbit_incref(_M0L6_2atmpS2627);
      }
      if (_M0L6_2aoldS4218) {
        moonbit_decref(_M0L6_2aoldS4218);
      }
      _M0L6_2atmpS2626->$1 = _M0L6_2atmpS2627;
      moonbit_decref(_M0L6_2atmpS2626);
      break;
    }
  }
  _M0L4selfS526->$6 = _M0L3idxS528;
  _M0L7entriesS2630 = _M0L4selfS526->$0;
  _M0L6_2atmpS2631 = _M0L5entryS527;
  if (
    _M0L3idxS528 < 0
    || _M0L3idxS528 >= Moonbit_array_length(_M0L7entriesS2630)
  ) {
    #line 526 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4215
  = (struct _M0TPB5EntryGsiE*)_M0L7entriesS2630[_M0L3idxS528];
  if (_M0L6_2atmpS2631) {
    moonbit_incref(_M0L6_2atmpS2631);
  }
  if (_M0L6_2aoldS4215) {
    moonbit_decref(_M0L6_2aoldS4215);
  }
  _M0L7entriesS2630[_M0L3idxS528] = _M0L6_2atmpS2631;
  _M0L4sizeS2633 = _M0L4selfS526->$1;
  _M0L6_2atmpS2632 = _M0L4sizeS2633 + 1;
  _M0L4selfS526->$1 = _M0L6_2atmpS2632;
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
  int32_t _M0L6_2atmpS2587;
  int32_t _M0L6_2atmpS2586;
  #line 71 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 72 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0Lm8capacityS505 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS506);
  _M0L6_2atmpS2587 = _M0Lm8capacityS505;
  #line 73 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2586 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2587);
  if (_M0L6lengthS506 > _M0L6_2atmpS2586) {
    int32_t _M0L6_2atmpS2588 = _M0Lm8capacityS505;
    _M0Lm8capacityS505 = _M0L6_2atmpS2588 * 2;
  }
  return _M0Lm8capacityS505;
}

struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _M0FPB8new__mapGsRP19moonbitDB10RedisValueE(
  int32_t _M0L8capacityS476
) {
  int32_t _M0L8capacityS475;
  int32_t _M0L7_2abindS477;
  int32_t _M0L7_2abindS478;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L6_2atmpS2581;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE** _M0L7_2abindS479;
  struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE* _M0L7_2abindS480;
  struct _M0TPB3MapGsRP19moonbitDB10RedisValueE* _block_4632;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS475
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS476);
  _M0L7_2abindS477 = _M0L8capacityS475 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS478 = _M0FPB21calc__grow__threshold(_M0L8capacityS475);
  _M0L6_2atmpS2581 = 0;
  _M0L7_2abindS479
  = (struct _M0TPB5EntryGsRP19moonbitDB10RedisValueE**)moonbit_make_ref_array(_M0L8capacityS475, _M0L6_2atmpS2581);
  _M0L7_2abindS480 = 0;
  _block_4632
  = (struct _M0TPB3MapGsRP19moonbitDB10RedisValueE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRP19moonbitDB10RedisValueE));
  Moonbit_object_header(_block_4632)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 100, 0);
  _block_4632->$0 = _M0L7_2abindS479;
  _block_4632->$1 = 0;
  _block_4632->$2 = _M0L8capacityS475;
  _block_4632->$3 = _M0L7_2abindS477;
  _block_4632->$4 = _M0L7_2abindS478;
  _block_4632->$5 = _M0L7_2abindS480;
  _block_4632->$6 = -1;
  return _block_4632;
}

struct _M0TPB3MapGsiE* _M0FPB8new__mapGsiE(int32_t _M0L8capacityS482) {
  int32_t _M0L8capacityS481;
  int32_t _M0L7_2abindS483;
  int32_t _M0L7_2abindS484;
  struct _M0TPB5EntryGsiE* _M0L6_2atmpS2582;
  struct _M0TPB5EntryGsiE** _M0L7_2abindS485;
  struct _M0TPB5EntryGsiE* _M0L7_2abindS486;
  struct _M0TPB3MapGsiE* _block_4633;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS481
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS482);
  _M0L7_2abindS483 = _M0L8capacityS481 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS484 = _M0FPB21calc__grow__threshold(_M0L8capacityS481);
  _M0L6_2atmpS2582 = 0;
  _M0L7_2abindS485
  = (struct _M0TPB5EntryGsiE**)moonbit_make_ref_array(_M0L8capacityS481, _M0L6_2atmpS2582);
  _M0L7_2abindS486 = 0;
  _block_4633
  = (struct _M0TPB3MapGsiE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsiE));
  Moonbit_object_header(_block_4633)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 104, 0);
  _block_4633->$0 = _M0L7_2abindS485;
  _block_4633->$1 = 0;
  _block_4633->$2 = _M0L8capacityS481;
  _block_4633->$3 = _M0L7_2abindS483;
  _block_4633->$4 = _M0L7_2abindS484;
  _block_4633->$5 = _M0L7_2abindS486;
  _block_4633->$6 = -1;
  return _block_4633;
}

struct _M0TPB3MapGssE* _M0FPB8new__mapGssE(int32_t _M0L8capacityS488) {
  int32_t _M0L8capacityS487;
  int32_t _M0L7_2abindS489;
  int32_t _M0L7_2abindS490;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2583;
  struct _M0TPB5EntryGssE** _M0L7_2abindS491;
  struct _M0TPB5EntryGssE* _M0L7_2abindS492;
  struct _M0TPB3MapGssE* _block_4634;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS487
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS488);
  _M0L7_2abindS489 = _M0L8capacityS487 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS490 = _M0FPB21calc__grow__threshold(_M0L8capacityS487);
  _M0L6_2atmpS2583 = 0;
  _M0L7_2abindS491
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L8capacityS487, _M0L6_2atmpS2583);
  _M0L7_2abindS492 = 0;
  _block_4634
  = (struct _M0TPB3MapGssE*)moonbit_malloc(sizeof(struct _M0TPB3MapGssE));
  Moonbit_object_header(_block_4634)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 108, 0);
  _block_4634->$0 = _M0L7_2abindS491;
  _block_4634->$1 = 0;
  _block_4634->$2 = _M0L8capacityS487;
  _block_4634->$3 = _M0L7_2abindS489;
  _block_4634->$4 = _M0L7_2abindS490;
  _block_4634->$5 = _M0L7_2abindS492;
  _block_4634->$6 = -1;
  return _block_4634;
}

struct _M0TPB3MapGsbE* _M0FPB8new__mapGsbE(int32_t _M0L8capacityS494) {
  int32_t _M0L8capacityS493;
  int32_t _M0L7_2abindS495;
  int32_t _M0L7_2abindS496;
  struct _M0TPB5EntryGsbE* _M0L6_2atmpS2584;
  struct _M0TPB5EntryGsbE** _M0L7_2abindS497;
  struct _M0TPB5EntryGsbE* _M0L7_2abindS498;
  struct _M0TPB3MapGsbE* _block_4635;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS493
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS494);
  _M0L7_2abindS495 = _M0L8capacityS493 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS496 = _M0FPB21calc__grow__threshold(_M0L8capacityS493);
  _M0L6_2atmpS2584 = 0;
  _M0L7_2abindS497
  = (struct _M0TPB5EntryGsbE**)moonbit_make_ref_array(_M0L8capacityS493, _M0L6_2atmpS2584);
  _M0L7_2abindS498 = 0;
  _block_4635
  = (struct _M0TPB3MapGsbE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsbE));
  Moonbit_object_header(_block_4635)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 112, 0);
  _block_4635->$0 = _M0L7_2abindS497;
  _block_4635->$1 = 0;
  _block_4635->$2 = _M0L8capacityS493;
  _block_4635->$3 = _M0L7_2abindS495;
  _block_4635->$4 = _M0L7_2abindS496;
  _block_4635->$5 = _M0L7_2abindS498;
  _block_4635->$6 = -1;
  return _block_4635;
}

struct _M0TPB3MapGsfE* _M0FPB8new__mapGsfE(int32_t _M0L8capacityS500) {
  int32_t _M0L8capacityS499;
  int32_t _M0L7_2abindS501;
  int32_t _M0L7_2abindS502;
  struct _M0TPB5EntryGsfE* _M0L6_2atmpS2585;
  struct _M0TPB5EntryGsfE** _M0L7_2abindS503;
  struct _M0TPB5EntryGsfE* _M0L7_2abindS504;
  struct _M0TPB3MapGsfE* _block_4636;
  #line 57 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  #line 58 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L8capacityS499
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS500);
  _M0L7_2abindS501 = _M0L8capacityS499 - 1;
  #line 63 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L7_2abindS502 = _M0FPB21calc__grow__threshold(_M0L8capacityS499);
  _M0L6_2atmpS2585 = 0;
  _M0L7_2abindS503
  = (struct _M0TPB5EntryGsfE**)moonbit_make_ref_array(_M0L8capacityS499, _M0L6_2atmpS2585);
  _M0L7_2abindS504 = 0;
  _block_4636
  = (struct _M0TPB3MapGsfE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsfE));
  Moonbit_object_header(_block_4636)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 116, 0);
  _block_4636->$0 = _M0L7_2abindS503;
  _block_4636->$1 = 0;
  _block_4636->$2 = _M0L8capacityS499;
  _block_4636->$3 = _M0L7_2abindS501;
  _block_4636->$4 = _M0L7_2abindS502;
  _block_4636->$5 = _M0L7_2abindS504;
  _block_4636->$6 = -1;
  return _block_4636;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS474) {
  #line 33 "/home/developer/.moon/lib/core/builtin/int.mbt"
  if (_M0L4selfS474 >= 0) {
    int32_t _M0L6_2atmpS2580;
    int32_t _M0L6_2atmpS2579;
    int32_t _M0L6_2atmpS2578;
    int32_t _M0L6_2atmpS2577;
    if (_M0L4selfS474 <= 1) {
      return 1;
    }
    if (_M0L4selfS474 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2580 = _M0L4selfS474 - 1;
    #line 44 "/home/developer/.moon/lib/core/builtin/int.mbt"
    _M0L6_2atmpS2579 = moonbit_clz32(_M0L6_2atmpS2580);
    _M0L6_2atmpS2578 = _M0L6_2atmpS2579 - 1;
    _M0L6_2atmpS2577 = 2147483647 >> (_M0L6_2atmpS2578 & 31);
    return _M0L6_2atmpS2577 + 1;
  } else {
    #line 34 "/home/developer/.moon/lib/core/builtin/int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS473) {
  int32_t _M0L6_2atmpS2576;
  #line 610 "/home/developer/.moon/lib/core/builtin/linked_hash_map.mbt"
  _M0L6_2atmpS2576 = _M0L8capacityS473 * 13;
  return _M0L6_2atmpS2576 / 16;
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
  int32_t _M0L3endS2551;
  int32_t _M0L5startS2552;
  int32_t _M0L6_2atmpS2550;
  #line 1497 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
  _M0L3endS2551 = _M0L4selfS435.$2;
  _M0L5startS2552 = _M0L4selfS435.$1;
  _M0L6_2atmpS2550 = _M0L3endS2551 - _M0L5startS2552;
  if (_M0L6_2atmpS2550 == 0) {
    return (moonbit_string_t)moonbit_string_literal_96.data;
  } else {
    moonbit_string_t* _M0L3bufS2574 = _M0L4selfS435.$0;
    int32_t _M0L5startS2575 = _M0L4selfS435.$1;
    moonbit_string_t _M0L5_2ahdS436 =
      (moonbit_string_t)_M0L3bufS2574[_M0L5startS2575];
    moonbit_string_t* _M0L9_2ax__bufS437 = _M0L4selfS435.$0;
    int32_t _M0L5startS2573 = _M0L4selfS435.$1;
    int32_t _M0L11_2ax__startS438 = 1 + _M0L5startS2573;
    int32_t _M0L9_2ax__endS439 = _M0L4selfS435.$2;
    struct _M0TPC16string10StringView _M0L2hdS440;
    int32_t _M0L7_2abindS441;
    int32_t _M0L3endS2571;
    int32_t _M0L5startS2572;
    int32_t _M0L6_2atmpS2570;
    int32_t _M0L10size__hintS442;
    int32_t _M0L2__S443;
    int32_t _M0L10size__hintS444;
    int32_t _M0L10size__hintS449;
    struct _M0TPB13StringBuilder* _M0L3bufS450;
    int32_t _M0L3endS2554;
    int32_t _M0L5startS2555;
    int32_t _M0L6_2atmpS2553;
    moonbit_string_t _result_4640;
    moonbit_incref(_M0L9_2ax__bufS437);
    moonbit_incref(_M0L5_2ahdS436);
    #line 1504 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _M0L2hdS440
    = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L5_2ahdS436);
    moonbit_decref(_M0L5_2ahdS436);
    _M0L7_2abindS441 = _M0L9_2ax__endS439 - _M0L11_2ax__startS438;
    _M0L3endS2571 = _M0L2hdS440.$2;
    _M0L5startS2572 = _M0L2hdS440.$1;
    _M0L6_2atmpS2570 = _M0L3endS2571 - _M0L5startS2572;
    _M0L2__S443 = 0;
    _M0L10size__hintS444 = _M0L6_2atmpS2570;
    while (1) {
      if (_M0L2__S443 < _M0L7_2abindS441) {
        int32_t _M0L6_2atmpS2569 = _M0L11_2ax__startS438 + _M0L2__S443;
        moonbit_string_t _M0L1sS445 =
          (moonbit_string_t)_M0L9_2ax__bufS437[_M0L6_2atmpS2569];
        int32_t _M0L6_2atmpS2560 = _M0L2__S443 + 1;
        struct _M0TPC16string10StringView _M0L7_2abindS447;
        int32_t _M0L3endS2567;
        int32_t _M0L5startS2568;
        int32_t _M0L6_2atmpS2566;
        int32_t _M0L6_2atmpS2562;
        int32_t _M0L3endS2564;
        int32_t _M0L5startS2565;
        int32_t _M0L6_2atmpS2563;
        int32_t _M0L6_2atmpS2561;
        moonbit_incref(_M0L1sS445);
        #line 1506 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
        _M0L7_2abindS447
        = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS445);
        moonbit_decref(_M0L1sS445);
        _M0L3endS2567 = _M0L7_2abindS447.$2;
        _M0L5startS2568 = _M0L7_2abindS447.$1;
        moonbit_decref(_M0L7_2abindS447.$0);
        _M0L6_2atmpS2566 = _M0L3endS2567 - _M0L5startS2568;
        _M0L6_2atmpS2562 = _M0L10size__hintS444 + _M0L6_2atmpS2566;
        _M0L3endS2564 = _M0L9separatorS448.$2;
        _M0L5startS2565 = _M0L9separatorS448.$1;
        _M0L6_2atmpS2563 = _M0L3endS2564 - _M0L5startS2565;
        _M0L6_2atmpS2561 = _M0L6_2atmpS2562 + _M0L6_2atmpS2563;
        _M0L2__S443 = _M0L6_2atmpS2560;
        _M0L10size__hintS444 = _M0L6_2atmpS2561;
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
    _M0L3endS2554 = _M0L9separatorS448.$2;
    _M0L5startS2555 = _M0L9separatorS448.$1;
    _M0L6_2atmpS2553 = _M0L3endS2554 - _M0L5startS2555;
    if (_M0L6_2atmpS2553 == 0) {
      int32_t _M0L7_2abindS451 = _M0L9_2ax__endS439 - _M0L11_2ax__startS438;
      int32_t _M0L2__S452 = 0;
      while (1) {
        if (_M0L2__S452 < _M0L7_2abindS451) {
          int32_t _M0L6_2atmpS2557 = _M0L11_2ax__startS438 + _M0L2__S452;
          moonbit_string_t _M0L1sS453 =
            (moonbit_string_t)_M0L9_2ax__bufS437[_M0L6_2atmpS2557];
          struct _M0TPC16string10StringView _M0L1sS454;
          int32_t _M0L6_2atmpS2556;
          moonbit_incref(_M0L1sS453);
          #line 1517 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0L1sS454
          = _M0IPC16string6StringPB12ToStringView16to__string__view(_M0L1sS453);
          moonbit_decref(_M0L1sS453);
          #line 1518 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
          _M0IPB13StringBuilderPB6Logger11write__view(_M0L3bufS450, _M0L1sS454);
          moonbit_decref(_M0L1sS454.$0);
          _M0L6_2atmpS2556 = _M0L2__S452 + 1;
          _M0L2__S452 = _M0L6_2atmpS2556;
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
          int32_t _M0L6_2atmpS2559 = _M0L11_2ax__startS438 + _M0L2__S457;
          moonbit_string_t _M0L1sS458 =
            (moonbit_string_t)_M0L9_2ax__bufS437[_M0L6_2atmpS2559];
          struct _M0TPC16string10StringView _M0L1sS459;
          int32_t _M0L6_2atmpS2558;
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
          _M0L6_2atmpS2558 = _M0L2__S457 + 1;
          _M0L2__S457 = _M0L6_2atmpS2558;
          continue;
        } else {
          moonbit_decref(_M0L9_2ax__bufS437);
        }
        break;
      }
    }
    #line 1528 "/home/developer/.moon/lib/core/builtin/arrayview.mbt"
    _result_4640 = _M0MPB13StringBuilder10to__string(_M0L3bufS450);
    moonbit_decref(_M0L3bufS450);
    return _result_4640;
  }
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS431,
  int32_t _M0L5indexS432
) {
  uint64_t* _M0L6_2atmpS2548;
  uint64_t _result_4641;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS431);
  _M0L6_2atmpS2548 = _M0L4selfS431;
  if (
    _M0L5indexS432 < 0
    || _M0L5indexS432 >= Moonbit_array_length(_M0L6_2atmpS2548)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_4641 = (uint64_t)_M0L6_2atmpS2548[_M0L5indexS432];
  moonbit_decref(_M0L6_2atmpS2548);
  return _result_4641;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS433,
  int32_t _M0L5indexS434
) {
  uint32_t* _M0L6_2atmpS2549;
  uint32_t _result_4642;
  #line 38 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
  moonbit_incref(_M0L4selfS433);
  _M0L6_2atmpS2549 = _M0L4selfS433;
  if (
    _M0L5indexS434 < 0
    || _M0L5indexS434 >= Moonbit_array_length(_M0L6_2atmpS2549)
  ) {
    #line 40 "/home/developer/.moon/lib/core/builtin/readonlyarray.mbt"
    moonbit_panic();
  }
  _result_4642 = (uint32_t)_M0L6_2atmpS2549[_M0L5indexS434];
  moonbit_decref(_M0L6_2atmpS2549);
  return _result_4642;
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
    return (moonbit_string_t)moonbit_string_literal_101.data;
  } else {
    return (moonbit_string_t)moonbit_string_literal_102.data;
  }
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS427) {
  #line 2494 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  return (uint64_t)_M0L4selfS427;
}

struct _M0TPC16string10StringView _M0IPC16string6StringPB12ToStringView16to__string__view(
  moonbit_string_t _M0L4selfS426
) {
  int32_t _M0L6_2atmpS2547;
  #line 24 "/home/developer/.moon/lib/core/builtin/string_like.mbt"
  _M0L6_2atmpS2547 = Moonbit_array_length(_M0L4selfS426);
  moonbit_incref(_M0L4selfS426);
  return (struct _M0TPC16string10StringView){.$0 = _M0L4selfS426,
                                               .$1 = 0,
                                               .$2 = _M0L6_2atmpS2547};
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS417,
  moonbit_string_t _M0L5valueS419
) {
  int32_t _M0L3lenS2532;
  moonbit_string_t* _M0L6_2atmpS2534;
  int32_t _M0L6_2atmpS2533;
  int32_t _M0L6lengthS418;
  moonbit_string_t* _M0L3bufS2535;
  moonbit_string_t _M0L6_2aoldS4227;
  int32_t _M0L6_2atmpS2536;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2532 = _M0L4selfS417->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2534 = _M0MPC15array5Array6bufferGsE(_M0L4selfS417);
  _M0L6_2atmpS2533 = Moonbit_array_length(_M0L6_2atmpS2534);
  moonbit_decref(_M0L6_2atmpS2534);
  if (_M0L3lenS2532 == _M0L6_2atmpS2533) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS417);
  }
  _M0L6lengthS418 = _M0L4selfS417->$1;
  _M0L3bufS2535 = _M0L4selfS417->$0;
  _M0L6_2aoldS4227 = (moonbit_string_t)_M0L3bufS2535[_M0L6lengthS418];
  moonbit_incref(_M0L5valueS419);
  moonbit_decref(_M0L6_2aoldS4227);
  _M0L3bufS2535[_M0L6lengthS418] = _M0L5valueS419;
  _M0L6_2atmpS2536 = _M0L6lengthS418 + 1;
  _M0L4selfS417->$1 = _M0L6_2atmpS2536;
  return 0;
}

int32_t _M0MPC15array5Array4pushGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS420,
  moonbit_string_t _M0L5valueS422
) {
  int32_t _M0L3lenS2537;
  moonbit_string_t* _M0L6_2atmpS2539;
  int32_t _M0L6_2atmpS2538;
  int32_t _M0L6lengthS421;
  moonbit_string_t* _M0L3bufS2540;
  moonbit_string_t _M0L6_2aoldS4229;
  int32_t _M0L6_2atmpS2541;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2537 = _M0L4selfS420->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2539 = _M0MPC15array5Array6bufferGOsE(_M0L4selfS420);
  _M0L6_2atmpS2538 = Moonbit_array_length(_M0L6_2atmpS2539);
  moonbit_decref(_M0L6_2atmpS2539);
  if (_M0L3lenS2537 == _M0L6_2atmpS2538) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGOsE(_M0L4selfS420);
  }
  _M0L6lengthS421 = _M0L4selfS420->$1;
  _M0L3bufS2540 = _M0L4selfS420->$0;
  _M0L6_2aoldS4229 = (moonbit_string_t)_M0L3bufS2540[_M0L6lengthS421];
  if (_M0L5valueS422) {
    moonbit_incref(_M0L5valueS422);
  }
  if (_M0L6_2aoldS4229) {
    moonbit_decref(_M0L6_2aoldS4229);
  }
  _M0L3bufS2540[_M0L6lengthS421] = _M0L5valueS422;
  _M0L6_2atmpS2541 = _M0L6lengthS421 + 1;
  _M0L4selfS420->$1 = _M0L6_2atmpS2541;
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS423,
  struct _M0TUsfE* _M0L5valueS425
) {
  int32_t _M0L3lenS2542;
  struct _M0TUsfE** _M0L6_2atmpS2544;
  int32_t _M0L6_2atmpS2543;
  int32_t _M0L6lengthS424;
  struct _M0TUsfE** _M0L3bufS2545;
  struct _M0TUsfE* _M0L6_2aoldS4231;
  int32_t _M0L6_2atmpS2546;
  #line 305 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L3lenS2542 = _M0L4selfS423->$1;
  #line 306 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L6_2atmpS2544 = _M0MPC15array5Array6bufferGUsfEE(_M0L4selfS423);
  _M0L6_2atmpS2543 = Moonbit_array_length(_M0L6_2atmpS2544);
  moonbit_decref(_M0L6_2atmpS2544);
  if (_M0L3lenS2542 == _M0L6_2atmpS2543) {
    #line 307 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsfEE(_M0L4selfS423);
  }
  _M0L6lengthS424 = _M0L4selfS423->$1;
  _M0L3bufS2545 = _M0L4selfS423->$0;
  _M0L6_2aoldS4231 = (struct _M0TUsfE*)_M0L3bufS2545[_M0L6lengthS424];
  moonbit_incref(_M0L5valueS425);
  if (_M0L6_2aoldS4231) {
    moonbit_decref(_M0L6_2aoldS4231);
  }
  _M0L3bufS2545[_M0L6lengthS424] = _M0L5valueS425;
  _M0L6_2atmpS2546 = _M0L6lengthS424 + 1;
  _M0L4selfS423->$1 = _M0L6_2atmpS2546;
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
  moonbit_string_t* _M0L6_2aoldS4233;
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
  _M0L6_2aoldS4233 = _M0L4selfS391->$0;
  moonbit_decref(_M0L6_2aoldS4233);
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
  moonbit_string_t* _M0L6_2aoldS4235;
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
  _M0L6_2aoldS4235 = _M0L4selfS397->$0;
  moonbit_decref(_M0L6_2aoldS4235);
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
  struct _M0TUsfE** _M0L6_2aoldS4237;
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
  _M0L6_2aoldS4237 = _M0L4selfS403->$0;
  moonbit_decref(_M0L6_2aoldS4237);
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
  int32_t _M0L3endS2530;
  int32_t _M0L5startS2531;
  int32_t _M0L8str__lenS383;
  int32_t _M0L3lenS2523;
  int32_t _M0L6_2atmpS2522;
  uint16_t* _M0L4dataS2524;
  int32_t _M0L3lenS2525;
  moonbit_string_t _M0L6_2atmpS2526;
  int32_t _M0L6_2atmpS2527;
  int32_t _M0L3lenS2529;
  int32_t _M0L6_2atmpS2528;
  #line 131 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3endS2530 = _M0L3strS384.$2;
  _M0L5startS2531 = _M0L3strS384.$1;
  _M0L8str__lenS383 = _M0L3endS2530 - _M0L5startS2531;
  _M0L3lenS2523 = _M0L4selfS385->$1;
  _M0L6_2atmpS2522 = _M0L3lenS2523 + _M0L8str__lenS383;
  #line 136 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS385, _M0L6_2atmpS2522);
  _M0L4dataS2524 = _M0L4selfS385->$0;
  _M0L3lenS2525 = _M0L4selfS385->$1;
  moonbit_incref(_M0L4dataS2524);
  #line 139 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2526 = _M0MPC16string10StringView4data(_M0L3strS384);
  #line 140 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2527 = _M0MPC16string10StringView13start__offset(_M0L3strS384);
  #line 137 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2524, _M0L3lenS2525, _M0L6_2atmpS2526, _M0L6_2atmpS2527, _M0L8str__lenS383);
  moonbit_decref(_M0L4dataS2524);
  moonbit_decref(_M0L6_2atmpS2526);
  _M0L3lenS2529 = _M0L4selfS385->$1;
  _M0L6_2atmpS2528 = _M0L3lenS2529 + _M0L8str__lenS383;
  _M0L4selfS385->$1 = _M0L6_2atmpS2528;
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "/home/developer/.moon/lib/core/builtin/byte.mbt"
  return 0;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS380
) {
  moonbit_string_t* _M0L8_2afieldS4240;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS4240 = _M0L4selfS380->$0;
  moonbit_incref(_M0L8_2afieldS4240);
  return _M0L8_2afieldS4240;
}

moonbit_string_t* _M0MPC15array5Array6bufferGOsE(
  struct _M0TPB5ArrayGOsE* _M0L4selfS381
) {
  moonbit_string_t* _M0L8_2afieldS4241;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS4241 = _M0L4selfS381->$0;
  moonbit_incref(_M0L8_2afieldS4241);
  return _M0L8_2afieldS4241;
}

struct _M0TUsfE** _M0MPC15array5Array6bufferGUsfEE(
  struct _M0TPB5ArrayGUsfEE* _M0L4selfS382
) {
  struct _M0TUsfE** _M0L8_2afieldS4242;
  #line 184 "/home/developer/.moon/lib/core/builtin/arraycore_nonjs.mbt"
  _M0L8_2afieldS4242 = _M0L4selfS382->$0;
  moonbit_incref(_M0L8_2afieldS4242);
  return _M0L8_2afieldS4242;
}

struct _M0TPB4IterGUssEE* _M0MPB4Iter3newGUssEE(
  struct _M0TWEOUssE* _M0L1fS364,
  int64_t _M0L10size__hintS361
) {
  int64_t _M0L10size__hintS360;
  struct _M0TPB4IterGUssEE* _block_4643;
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
  _block_4643
  = (struct _M0TPB4IterGUssEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUssEE));
  Moonbit_object_header(_block_4643)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 120, 0);
  _block_4643->$0 = _M0L1fS364;
  _block_4643->$1 = _M0L10size__hintS360;
  return _block_4643;
}

struct _M0TPB4IterGUsbEE* _M0MPB4Iter3newGUsbEE(
  struct _M0TWEOUsbE* _M0L1fS369,
  int64_t _M0L10size__hintS366
) {
  int64_t _M0L10size__hintS365;
  struct _M0TPB4IterGUsbEE* _block_4644;
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
  _block_4644
  = (struct _M0TPB4IterGUsbEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsbEE));
  Moonbit_object_header(_block_4644)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 123, 0);
  _block_4644->$0 = _M0L1fS369;
  _block_4644->$1 = _M0L10size__hintS365;
  return _block_4644;
}

struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _M0MPB4Iter3newGUsRP19moonbitDB10RedisValueEE(
  struct _M0TWEOUsRP19moonbitDB10RedisValueE* _M0L1fS374,
  int64_t _M0L10size__hintS371
) {
  int64_t _M0L10size__hintS370;
  struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE* _block_4645;
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
  _block_4645
  = (struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsRP19moonbitDB10RedisValueEE));
  Moonbit_object_header(_block_4645)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 126, 0);
  _block_4645->$0 = _M0L1fS374;
  _block_4645->$1 = _M0L10size__hintS370;
  return _block_4645;
}

struct _M0TPB4IterGUsfEE* _M0MPB4Iter3newGUsfEE(
  struct _M0TWEOUsfE* _M0L1fS379,
  int64_t _M0L10size__hintS376
) {
  int64_t _M0L10size__hintS375;
  struct _M0TPB4IterGUsfEE* _block_4646;
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
  _block_4646
  = (struct _M0TPB4IterGUsfEE*)moonbit_malloc(sizeof(struct _M0TPB4IterGUsfEE));
  Moonbit_object_header(_block_4646)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 129, 0);
  _block_4646->$0 = _M0L1fS379;
  _block_4646->$1 = _M0L10size__hintS375;
  return _block_4646;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS352,
  int32_t _M0L5radixS351
) {
  int32_t _if__result_4647;
  uint16_t* _M0L6bufferS353;
  #line 607 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS351 < 2) {
    _if__result_4647 = 1;
  } else {
    _if__result_4647 = _M0L5radixS351 > 36;
  }
  if (_if__result_4647) {
    #line 611 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_103.data);
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
  int32_t _M0L6_2atmpS2521;
  uint64_t _M0L3numS327;
  int32_t _M0L6offsetS328;
  #line 493 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2521 = _M0L10total__lenS350 - _M0L12digit__startS338;
  _M0L3numS327 = _M0L3numS349;
  _M0L6offsetS328 = _M0L6_2atmpS2521;
  while (1) {
    if (_M0L3numS327 >= 10000ull) {
      uint64_t _M0L1tS329 = _M0L3numS327 / 10000ull;
      uint64_t _M0L6_2atmpS2498 = _M0L3numS327 % 10000ull;
      int32_t _M0L1rS330 = (int32_t)_M0L6_2atmpS2498;
      int32_t _M0L2d1S331 = _M0L1rS330 / 100;
      int32_t _M0L2d2S332 = _M0L1rS330 % 100;
      int32_t _M0L6_2atmpS2497 = _M0L2d1S331 / 10;
      int32_t _M0L6_2atmpS2496 = 48 + _M0L6_2atmpS2497;
      int32_t _M0L6d1__hiS333 = (uint16_t)_M0L6_2atmpS2496;
      int32_t _M0L6_2atmpS2495 = _M0L2d1S331 % 10;
      int32_t _M0L6_2atmpS2494 = 48 + _M0L6_2atmpS2495;
      int32_t _M0L6d1__loS334 = (uint16_t)_M0L6_2atmpS2494;
      int32_t _M0L6_2atmpS2493 = _M0L2d2S332 / 10;
      int32_t _M0L6_2atmpS2492 = 48 + _M0L6_2atmpS2493;
      int32_t _M0L6d2__hiS335 = (uint16_t)_M0L6_2atmpS2492;
      int32_t _M0L6_2atmpS2491 = _M0L2d2S332 % 10;
      int32_t _M0L6_2atmpS2490 = 48 + _M0L6_2atmpS2491;
      int32_t _M0L6d2__loS336 = (uint16_t)_M0L6_2atmpS2490;
      int32_t _M0L6_2atmpS2482 = _M0L12digit__startS338 + _M0L6offsetS328;
      int32_t _M0L6_2atmpS2481 = _M0L6_2atmpS2482 - 4;
      int32_t _M0L6_2atmpS2484;
      int32_t _M0L6_2atmpS2483;
      int32_t _M0L6_2atmpS2486;
      int32_t _M0L6_2atmpS2485;
      int32_t _M0L6_2atmpS2488;
      int32_t _M0L6_2atmpS2487;
      int32_t _M0L6_2atmpS2489;
      _M0L6bufferS337[_M0L6_2atmpS2481] = _M0L6d1__hiS333;
      _M0L6_2atmpS2484 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2483 = _M0L6_2atmpS2484 - 3;
      _M0L6bufferS337[_M0L6_2atmpS2483] = _M0L6d1__loS334;
      _M0L6_2atmpS2486 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2485 = _M0L6_2atmpS2486 - 2;
      _M0L6bufferS337[_M0L6_2atmpS2485] = _M0L6d2__hiS335;
      _M0L6_2atmpS2488 = _M0L12digit__startS338 + _M0L6offsetS328;
      _M0L6_2atmpS2487 = _M0L6_2atmpS2488 - 1;
      _M0L6bufferS337[_M0L6_2atmpS2487] = _M0L6d2__loS336;
      _M0L6_2atmpS2489 = _M0L6offsetS328 - 4;
      _M0L3numS327 = _M0L1tS329;
      _M0L6offsetS328 = _M0L6_2atmpS2489;
      continue;
    } else {
      int32_t _M0L6_2atmpS2520 = (int32_t)_M0L3numS327;
      int32_t _M0L9remainingS340 = _M0L6_2atmpS2520;
      int32_t _M0L6offsetS341 = _M0L6offsetS328;
      while (1) {
        if (_M0L9remainingS340 >= 100) {
          int32_t _M0L1tS342 = _M0L9remainingS340 / 100;
          int32_t _M0L1dS343 = _M0L9remainingS340 % 100;
          int32_t _M0L6_2atmpS2507 = _M0L1dS343 / 10;
          int32_t _M0L6_2atmpS2506 = 48 + _M0L6_2atmpS2507;
          int32_t _M0L5d__hiS344 = (uint16_t)_M0L6_2atmpS2506;
          int32_t _M0L6_2atmpS2505 = _M0L1dS343 % 10;
          int32_t _M0L6_2atmpS2504 = 48 + _M0L6_2atmpS2505;
          int32_t _M0L5d__loS345 = (uint16_t)_M0L6_2atmpS2504;
          int32_t _M0L6_2atmpS2500 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2499 = _M0L6_2atmpS2500 - 2;
          int32_t _M0L6_2atmpS2502;
          int32_t _M0L6_2atmpS2501;
          int32_t _M0L6_2atmpS2503;
          _M0L6bufferS337[_M0L6_2atmpS2499] = _M0L5d__hiS344;
          _M0L6_2atmpS2502 = _M0L12digit__startS338 + _M0L6offsetS341;
          _M0L6_2atmpS2501 = _M0L6_2atmpS2502 - 1;
          _M0L6bufferS337[_M0L6_2atmpS2501] = _M0L5d__loS345;
          _M0L6_2atmpS2503 = _M0L6offsetS341 - 2;
          _M0L9remainingS340 = _M0L1tS342;
          _M0L6offsetS341 = _M0L6_2atmpS2503;
          continue;
        } else if (_M0L9remainingS340 >= 10) {
          int32_t _M0L6_2atmpS2515 = _M0L9remainingS340 / 10;
          int32_t _M0L6_2atmpS2514 = 48 + _M0L6_2atmpS2515;
          int32_t _M0L5d__hiS347 = (uint16_t)_M0L6_2atmpS2514;
          int32_t _M0L6_2atmpS2513 = _M0L9remainingS340 % 10;
          int32_t _M0L6_2atmpS2512 = 48 + _M0L6_2atmpS2513;
          int32_t _M0L5d__loS348 = (uint16_t)_M0L6_2atmpS2512;
          int32_t _M0L6_2atmpS2509 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2508 = _M0L6_2atmpS2509 - 2;
          int32_t _M0L6_2atmpS2511;
          int32_t _M0L6_2atmpS2510;
          _M0L6bufferS337[_M0L6_2atmpS2508] = _M0L5d__hiS347;
          _M0L6_2atmpS2511 = _M0L12digit__startS338 + _M0L6offsetS341;
          _M0L6_2atmpS2510 = _M0L6_2atmpS2511 - 1;
          _M0L6bufferS337[_M0L6_2atmpS2510] = _M0L5d__loS348;
        } else {
          int32_t _M0L6_2atmpS2519 = _M0L12digit__startS338 + _M0L6offsetS341;
          int32_t _M0L6_2atmpS2516 = _M0L6_2atmpS2519 - 1;
          int32_t _M0L6_2atmpS2518 = 48 + _M0L9remainingS340;
          int32_t _M0L6_2atmpS2517 = (uint16_t)_M0L6_2atmpS2518;
          _M0L6bufferS337[_M0L6_2atmpS2516] = _M0L6_2atmpS2517;
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
  int32_t _M0L6_2atmpS2466;
  int32_t _M0L6_2atmpS2465;
  #line 462 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  #line 470 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS310 = _M0MPC13int3Int10to__uint64(_M0L5radixS311);
  _M0L6_2atmpS2466 = _M0L5radixS311 - 1;
  _M0L6_2atmpS2465 = _M0L5radixS311 & _M0L6_2atmpS2466;
  if (_M0L6_2atmpS2465 == 0) {
    int32_t _M0L5shiftS312;
    uint64_t _M0L4maskS313;
    int32_t _M0L6_2atmpS2473;
    int32_t _M0L6offsetS314;
    uint64_t _M0L1nS315;
    #line 473 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS312 = moonbit_ctz32(_M0L5radixS311);
    _M0L4maskS313 = _M0L4baseS310 - 1ull;
    _M0L6_2atmpS2473 = _M0L10total__lenS320 - _M0L12digit__startS318;
    _M0L6offsetS314 = _M0L6_2atmpS2473;
    _M0L1nS315 = _M0L3numS321;
    while (1) {
      if (_M0L1nS315 > 0ull) {
        uint64_t _M0L6_2atmpS2472 = _M0L1nS315 & _M0L4maskS313;
        int32_t _M0L5digitS316 = (int32_t)_M0L6_2atmpS2472;
        int32_t _M0L6_2atmpS2469 = _M0L12digit__startS318 + _M0L6offsetS314;
        int32_t _M0L6_2atmpS2467 = _M0L6_2atmpS2469 - 1;
        int32_t _M0L6_2atmpS2468 =
          ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L5digitS316];
        int32_t _M0L6_2atmpS2470;
        uint64_t _M0L6_2atmpS2471;
        _M0L6bufferS317[_M0L6_2atmpS2467] = _M0L6_2atmpS2468;
        _M0L6_2atmpS2470 = _M0L6offsetS314 - 1;
        _M0L6_2atmpS2471 = _M0L1nS315 >> (_M0L5shiftS312 & 63);
        _M0L6offsetS314 = _M0L6_2atmpS2470;
        _M0L1nS315 = _M0L6_2atmpS2471;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2480 = _M0L10total__lenS320 - _M0L12digit__startS318;
    int32_t _M0L6offsetS322 = _M0L6_2atmpS2480;
    uint64_t _M0L1nS323 = _M0L3numS321;
    while (1) {
      if (_M0L1nS323 > 0ull) {
        uint64_t _M0L1qS324 = _M0L1nS323 / _M0L4baseS310;
        uint64_t _M0L6_2atmpS2479 = _M0L1qS324 * _M0L4baseS310;
        uint64_t _M0L6_2atmpS2478 = _M0L1nS323 - _M0L6_2atmpS2479;
        int32_t _M0L5digitS325 = (int32_t)_M0L6_2atmpS2478;
        int32_t _M0L6_2atmpS2476 = _M0L12digit__startS318 + _M0L6offsetS322;
        int32_t _M0L6_2atmpS2474 = _M0L6_2atmpS2476 - 1;
        int32_t _M0L6_2atmpS2475 =
          ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L5digitS325];
        int32_t _M0L6_2atmpS2477;
        _M0L6bufferS317[_M0L6_2atmpS2474] = _M0L6_2atmpS2475;
        _M0L6_2atmpS2477 = _M0L6offsetS322 - 1;
        _M0L6offsetS322 = _M0L6_2atmpS2477;
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
  int32_t _M0L6_2atmpS2464;
  int32_t _M0L6offsetS299;
  uint64_t _M0L1nS300;
  #line 434 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2464 = _M0L10total__lenS308 - _M0L12digit__startS305;
  _M0L6offsetS299 = _M0L6_2atmpS2464;
  _M0L1nS300 = _M0L3numS309;
  while (1) {
    if (_M0L6offsetS299 >= 2) {
      uint64_t _M0L6_2atmpS2461 = _M0L1nS300 & 255ull;
      int32_t _M0L9byte__valS301 = (int32_t)_M0L6_2atmpS2461;
      int32_t _M0L2hiS302 = _M0L9byte__valS301 / 16;
      int32_t _M0L2loS303 = _M0L9byte__valS301 % 16;
      int32_t _M0L6_2atmpS2455 = _M0L12digit__startS305 + _M0L6offsetS299;
      int32_t _M0L6_2atmpS2453 = _M0L6_2atmpS2455 - 2;
      int32_t _M0L6_2atmpS2454 =
        ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L2hiS302];
      int32_t _M0L6_2atmpS2458;
      int32_t _M0L6_2atmpS2456;
      int32_t _M0L6_2atmpS2457;
      int32_t _M0L6_2atmpS2459;
      uint64_t _M0L6_2atmpS2460;
      _M0L6bufferS304[_M0L6_2atmpS2453] = _M0L6_2atmpS2454;
      _M0L6_2atmpS2458 = _M0L12digit__startS305 + _M0L6offsetS299;
      _M0L6_2atmpS2456 = _M0L6_2atmpS2458 - 1;
      _M0L6_2atmpS2457
      = ((moonbit_string_t)moonbit_string_literal_104.data)[
        _M0L2loS303
      ];
      _M0L6bufferS304[_M0L6_2atmpS2456] = _M0L6_2atmpS2457;
      _M0L6_2atmpS2459 = _M0L6offsetS299 - 2;
      _M0L6_2atmpS2460 = _M0L1nS300 >> 8;
      _M0L6offsetS299 = _M0L6_2atmpS2459;
      _M0L1nS300 = _M0L6_2atmpS2460;
      continue;
    } else if (_M0L6offsetS299 == 1) {
      uint64_t _M0L6_2atmpS2463 = _M0L1nS300 & 15ull;
      int32_t _M0L6nibbleS307 = (int32_t)_M0L6_2atmpS2463;
      int32_t _M0L6_2atmpS2462 =
        ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L6nibbleS307];
      _M0L6bufferS304[_M0L12digit__startS305] = _M0L6_2atmpS2462;
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
      uint64_t _M0L6_2atmpS2451 = _M0L3numS296 / _M0L4baseS294;
      int32_t _M0L6_2atmpS2452 = _M0L5countS297 + 1;
      _M0L3numS296 = _M0L6_2atmpS2451;
      _M0L5countS297 = _M0L6_2atmpS2452;
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
    int32_t _M0L6_2atmpS2450;
    int32_t _M0L6_2atmpS2449;
    #line 412 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS292 = moonbit_clz64(_M0L5valueS291);
    _M0L6_2atmpS2450 = 63 - _M0L14leading__zerosS292;
    _M0L6_2atmpS2449 = _M0L6_2atmpS2450 / 4;
    return _M0L6_2atmpS2449 + 1;
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
  int32_t _if__result_4654;
  int32_t _M0L12is__negativeS275;
  uint32_t _M0L3numS276;
  uint16_t* _M0L6bufferS277;
  #line 209 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  if (_M0L5radixS273 < 2) {
    _if__result_4654 = 1;
  } else {
    _if__result_4654 = _M0L5radixS273 > 36;
  }
  if (_if__result_4654) {
    #line 213 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0FPC15abort5abortGuE((moonbit_string_t)moonbit_string_literal_103.data);
  }
  if (_M0L4selfS274 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  }
  _M0L12is__negativeS275 = _M0L4selfS274 < 0;
  if (_M0L12is__negativeS275) {
    int32_t _M0L6_2atmpS2448 = -_M0L4selfS274;
    _M0L3numS276 = *(uint32_t*)&_M0L6_2atmpS2448;
  } else {
    _M0L3numS276 = *(uint32_t*)&_M0L4selfS274;
  }
  switch (_M0L5radixS273) {
    case 10: {
      int32_t _M0L10digit__lenS278;
      int32_t _M0L6_2atmpS2445;
      int32_t _M0L10total__lenS279;
      uint16_t* _M0L6bufferS280;
      int32_t _M0L12digit__startS281;
      #line 235 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS278 = _M0FPB12dec__count32(_M0L3numS276);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2445 = 1;
      } else {
        _M0L6_2atmpS2445 = 0;
      }
      _M0L10total__lenS279 = _M0L10digit__lenS278 + _M0L6_2atmpS2445;
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
      int32_t _M0L6_2atmpS2446;
      int32_t _M0L10total__lenS283;
      uint16_t* _M0L6bufferS284;
      int32_t _M0L12digit__startS285;
      #line 243 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS282 = _M0FPB12hex__count32(_M0L3numS276);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2446 = 1;
      } else {
        _M0L6_2atmpS2446 = 0;
      }
      _M0L10total__lenS283 = _M0L10digit__lenS282 + _M0L6_2atmpS2446;
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
      int32_t _M0L6_2atmpS2447;
      int32_t _M0L10total__lenS287;
      uint16_t* _M0L6bufferS288;
      int32_t _M0L12digit__startS289;
      #line 251 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
      _M0L10digit__lenS286
      = _M0FPB14radix__count32(_M0L3numS276, _M0L5radixS273);
      if (_M0L12is__negativeS275) {
        _M0L6_2atmpS2447 = 1;
      } else {
        _M0L6_2atmpS2447 = 0;
      }
      _M0L10total__lenS287 = _M0L10digit__lenS286 + _M0L6_2atmpS2447;
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
      uint32_t _M0L6_2atmpS2443 = _M0L3numS270 / _M0L4baseS268;
      int32_t _M0L6_2atmpS2444 = _M0L5countS271 + 1;
      _M0L3numS270 = _M0L6_2atmpS2443;
      _M0L5countS271 = _M0L6_2atmpS2444;
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
    int32_t _M0L6_2atmpS2442;
    int32_t _M0L6_2atmpS2441;
    #line 182 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L14leading__zerosS266 = moonbit_clz32(_M0L5valueS265);
    _M0L6_2atmpS2442 = 31 - _M0L14leading__zerosS266;
    _M0L6_2atmpS2441 = _M0L6_2atmpS2442 / 4;
    return _M0L6_2atmpS2441 + 1;
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
  int32_t _M0L6_2atmpS2440;
  uint32_t _M0L3numS240;
  int32_t _M0L6offsetS241;
  #line 88 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2440 = _M0L10total__lenS263 - _M0L12digit__startS251;
  _M0L3numS240 = _M0L3numS262;
  _M0L6offsetS241 = _M0L6_2atmpS2440;
  while (1) {
    if (_M0L3numS240 >= 10000u) {
      uint32_t _M0L1tS242 = _M0L3numS240 / 10000u;
      uint32_t _M0L6_2atmpS2417 = _M0L3numS240 % 10000u;
      int32_t _M0L1rS243 = *(int32_t*)&_M0L6_2atmpS2417;
      int32_t _M0L2d1S244 = _M0L1rS243 / 100;
      int32_t _M0L2d2S245 = _M0L1rS243 % 100;
      int32_t _M0L6_2atmpS2416 = _M0L2d1S244 / 10;
      int32_t _M0L6_2atmpS2415 = 48 + _M0L6_2atmpS2416;
      int32_t _M0L6d1__hiS246 = (uint16_t)_M0L6_2atmpS2415;
      int32_t _M0L6_2atmpS2414 = _M0L2d1S244 % 10;
      int32_t _M0L6_2atmpS2413 = 48 + _M0L6_2atmpS2414;
      int32_t _M0L6d1__loS247 = (uint16_t)_M0L6_2atmpS2413;
      int32_t _M0L6_2atmpS2412 = _M0L2d2S245 / 10;
      int32_t _M0L6_2atmpS2411 = 48 + _M0L6_2atmpS2412;
      int32_t _M0L6d2__hiS248 = (uint16_t)_M0L6_2atmpS2411;
      int32_t _M0L6_2atmpS2410 = _M0L2d2S245 % 10;
      int32_t _M0L6_2atmpS2409 = 48 + _M0L6_2atmpS2410;
      int32_t _M0L6d2__loS249 = (uint16_t)_M0L6_2atmpS2409;
      int32_t _M0L6_2atmpS2401 = _M0L12digit__startS251 + _M0L6offsetS241;
      int32_t _M0L6_2atmpS2400 = _M0L6_2atmpS2401 - 4;
      int32_t _M0L6_2atmpS2403;
      int32_t _M0L6_2atmpS2402;
      int32_t _M0L6_2atmpS2405;
      int32_t _M0L6_2atmpS2404;
      int32_t _M0L6_2atmpS2407;
      int32_t _M0L6_2atmpS2406;
      int32_t _M0L6_2atmpS2408;
      _M0L6bufferS250[_M0L6_2atmpS2400] = _M0L6d1__hiS246;
      _M0L6_2atmpS2403 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2402 = _M0L6_2atmpS2403 - 3;
      _M0L6bufferS250[_M0L6_2atmpS2402] = _M0L6d1__loS247;
      _M0L6_2atmpS2405 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2404 = _M0L6_2atmpS2405 - 2;
      _M0L6bufferS250[_M0L6_2atmpS2404] = _M0L6d2__hiS248;
      _M0L6_2atmpS2407 = _M0L12digit__startS251 + _M0L6offsetS241;
      _M0L6_2atmpS2406 = _M0L6_2atmpS2407 - 1;
      _M0L6bufferS250[_M0L6_2atmpS2406] = _M0L6d2__loS249;
      _M0L6_2atmpS2408 = _M0L6offsetS241 - 4;
      _M0L3numS240 = _M0L1tS242;
      _M0L6offsetS241 = _M0L6_2atmpS2408;
      continue;
    } else {
      int32_t _M0L6_2atmpS2439 = *(int32_t*)&_M0L3numS240;
      int32_t _M0L9remainingS253 = _M0L6_2atmpS2439;
      int32_t _M0L6offsetS254 = _M0L6offsetS241;
      while (1) {
        if (_M0L9remainingS253 >= 100) {
          int32_t _M0L1tS255 = _M0L9remainingS253 / 100;
          int32_t _M0L1dS256 = _M0L9remainingS253 % 100;
          int32_t _M0L6_2atmpS2426 = _M0L1dS256 / 10;
          int32_t _M0L6_2atmpS2425 = 48 + _M0L6_2atmpS2426;
          int32_t _M0L5d__hiS257 = (uint16_t)_M0L6_2atmpS2425;
          int32_t _M0L6_2atmpS2424 = _M0L1dS256 % 10;
          int32_t _M0L6_2atmpS2423 = 48 + _M0L6_2atmpS2424;
          int32_t _M0L5d__loS258 = (uint16_t)_M0L6_2atmpS2423;
          int32_t _M0L6_2atmpS2419 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2418 = _M0L6_2atmpS2419 - 2;
          int32_t _M0L6_2atmpS2421;
          int32_t _M0L6_2atmpS2420;
          int32_t _M0L6_2atmpS2422;
          _M0L6bufferS250[_M0L6_2atmpS2418] = _M0L5d__hiS257;
          _M0L6_2atmpS2421 = _M0L12digit__startS251 + _M0L6offsetS254;
          _M0L6_2atmpS2420 = _M0L6_2atmpS2421 - 1;
          _M0L6bufferS250[_M0L6_2atmpS2420] = _M0L5d__loS258;
          _M0L6_2atmpS2422 = _M0L6offsetS254 - 2;
          _M0L9remainingS253 = _M0L1tS255;
          _M0L6offsetS254 = _M0L6_2atmpS2422;
          continue;
        } else if (_M0L9remainingS253 >= 10) {
          int32_t _M0L6_2atmpS2434 = _M0L9remainingS253 / 10;
          int32_t _M0L6_2atmpS2433 = 48 + _M0L6_2atmpS2434;
          int32_t _M0L5d__hiS260 = (uint16_t)_M0L6_2atmpS2433;
          int32_t _M0L6_2atmpS2432 = _M0L9remainingS253 % 10;
          int32_t _M0L6_2atmpS2431 = 48 + _M0L6_2atmpS2432;
          int32_t _M0L5d__loS261 = (uint16_t)_M0L6_2atmpS2431;
          int32_t _M0L6_2atmpS2428 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2427 = _M0L6_2atmpS2428 - 2;
          int32_t _M0L6_2atmpS2430;
          int32_t _M0L6_2atmpS2429;
          _M0L6bufferS250[_M0L6_2atmpS2427] = _M0L5d__hiS260;
          _M0L6_2atmpS2430 = _M0L12digit__startS251 + _M0L6offsetS254;
          _M0L6_2atmpS2429 = _M0L6_2atmpS2430 - 1;
          _M0L6bufferS250[_M0L6_2atmpS2429] = _M0L5d__loS261;
        } else {
          int32_t _M0L6_2atmpS2438 = _M0L12digit__startS251 + _M0L6offsetS254;
          int32_t _M0L6_2atmpS2435 = _M0L6_2atmpS2438 - 1;
          int32_t _M0L6_2atmpS2437 = 48 + _M0L9remainingS253;
          int32_t _M0L6_2atmpS2436 = (uint16_t)_M0L6_2atmpS2437;
          _M0L6bufferS250[_M0L6_2atmpS2435] = _M0L6_2atmpS2436;
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
  int32_t _M0L6_2atmpS2385;
  int32_t _M0L6_2atmpS2384;
  #line 57 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L4baseS223 = *(uint32_t*)&_M0L5radixS224;
  _M0L6_2atmpS2385 = _M0L5radixS224 - 1;
  _M0L6_2atmpS2384 = _M0L5radixS224 & _M0L6_2atmpS2385;
  if (_M0L6_2atmpS2384 == 0) {
    int32_t _M0L5shiftS225;
    uint32_t _M0L4maskS226;
    int32_t _M0L6_2atmpS2392;
    int32_t _M0L6offsetS227;
    uint32_t _M0L1nS228;
    #line 68 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
    _M0L5shiftS225 = moonbit_ctz32(_M0L5radixS224);
    _M0L4maskS226 = _M0L4baseS223 - 1u;
    _M0L6_2atmpS2392 = _M0L10total__lenS233 - _M0L12digit__startS231;
    _M0L6offsetS227 = _M0L6_2atmpS2392;
    _M0L1nS228 = _M0L3numS234;
    while (1) {
      if (_M0L1nS228 > 0u) {
        uint32_t _M0L6_2atmpS2391 = _M0L1nS228 & _M0L4maskS226;
        int32_t _M0L5digitS229 = *(int32_t*)&_M0L6_2atmpS2391;
        int32_t _M0L6_2atmpS2388 = _M0L12digit__startS231 + _M0L6offsetS227;
        int32_t _M0L6_2atmpS2386 = _M0L6_2atmpS2388 - 1;
        int32_t _M0L6_2atmpS2387 =
          ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L5digitS229];
        int32_t _M0L6_2atmpS2389;
        uint32_t _M0L6_2atmpS2390;
        _M0L6bufferS230[_M0L6_2atmpS2386] = _M0L6_2atmpS2387;
        _M0L6_2atmpS2389 = _M0L6offsetS227 - 1;
        _M0L6_2atmpS2390 = _M0L1nS228 >> (_M0L5shiftS225 & 31);
        _M0L6offsetS227 = _M0L6_2atmpS2389;
        _M0L1nS228 = _M0L6_2atmpS2390;
        continue;
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2399 = _M0L10total__lenS233 - _M0L12digit__startS231;
    int32_t _M0L6offsetS235 = _M0L6_2atmpS2399;
    uint32_t _M0L1nS236 = _M0L3numS234;
    while (1) {
      if (_M0L1nS236 > 0u) {
        uint32_t _M0L1qS237 = _M0L1nS236 / _M0L4baseS223;
        uint32_t _M0L6_2atmpS2398 = _M0L1qS237 * _M0L4baseS223;
        uint32_t _M0L6_2atmpS2397 = _M0L1nS236 - _M0L6_2atmpS2398;
        int32_t _M0L5digitS238 = *(int32_t*)&_M0L6_2atmpS2397;
        int32_t _M0L6_2atmpS2395 = _M0L12digit__startS231 + _M0L6offsetS235;
        int32_t _M0L6_2atmpS2393 = _M0L6_2atmpS2395 - 1;
        int32_t _M0L6_2atmpS2394 =
          ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L5digitS238];
        int32_t _M0L6_2atmpS2396;
        _M0L6bufferS230[_M0L6_2atmpS2393] = _M0L6_2atmpS2394;
        _M0L6_2atmpS2396 = _M0L6offsetS235 - 1;
        _M0L6offsetS235 = _M0L6_2atmpS2396;
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
  int32_t _M0L6_2atmpS2383;
  int32_t _M0L6offsetS212;
  uint32_t _M0L1nS213;
  #line 29 "/home/developer/.moon/lib/core/builtin/to_string.mbt"
  _M0L6_2atmpS2383 = _M0L10total__lenS221 - _M0L12digit__startS218;
  _M0L6offsetS212 = _M0L6_2atmpS2383;
  _M0L1nS213 = _M0L3numS222;
  while (1) {
    if (_M0L6offsetS212 >= 2) {
      uint32_t _M0L6_2atmpS2380 = _M0L1nS213 & 255u;
      int32_t _M0L9byte__valS214 = *(int32_t*)&_M0L6_2atmpS2380;
      int32_t _M0L2hiS215 = _M0L9byte__valS214 / 16;
      int32_t _M0L2loS216 = _M0L9byte__valS214 % 16;
      int32_t _M0L6_2atmpS2374 = _M0L12digit__startS218 + _M0L6offsetS212;
      int32_t _M0L6_2atmpS2372 = _M0L6_2atmpS2374 - 2;
      int32_t _M0L6_2atmpS2373 =
        ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L2hiS215];
      int32_t _M0L6_2atmpS2377;
      int32_t _M0L6_2atmpS2375;
      int32_t _M0L6_2atmpS2376;
      int32_t _M0L6_2atmpS2378;
      uint32_t _M0L6_2atmpS2379;
      _M0L6bufferS217[_M0L6_2atmpS2372] = _M0L6_2atmpS2373;
      _M0L6_2atmpS2377 = _M0L12digit__startS218 + _M0L6offsetS212;
      _M0L6_2atmpS2375 = _M0L6_2atmpS2377 - 1;
      _M0L6_2atmpS2376
      = ((moonbit_string_t)moonbit_string_literal_104.data)[
        _M0L2loS216
      ];
      _M0L6bufferS217[_M0L6_2atmpS2375] = _M0L6_2atmpS2376;
      _M0L6_2atmpS2378 = _M0L6offsetS212 - 2;
      _M0L6_2atmpS2379 = _M0L1nS213 >> 8;
      _M0L6offsetS212 = _M0L6_2atmpS2378;
      _M0L1nS213 = _M0L6_2atmpS2379;
      continue;
    } else if (_M0L6offsetS212 == 1) {
      uint32_t _M0L6_2atmpS2382 = _M0L1nS213 & 15u;
      int32_t _M0L6nibbleS220 = *(int32_t*)&_M0L6_2atmpS2382;
      int32_t _M0L6_2atmpS2381 =
        ((moonbit_string_t)moonbit_string_literal_104.data)[_M0L6nibbleS220];
      _M0L6bufferS217[_M0L12digit__startS218] = _M0L6_2atmpS2381;
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
    int64_t _M0L6_2atmpS2364;
    if (_M0L4_2anS193 > 0) {
      int32_t _M0L6_2atmpS2365 = _M0L4_2anS193 - 1;
      _M0L6_2atmpS2364 = (int64_t)_M0L6_2atmpS2365;
    } else {
      _M0L6_2atmpS2364 = _M0MPB4Iter4nextN6constrS9980GUssEE;
    }
    _M0L4selfS189->$1 = _M0L6_2atmpS2364;
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
    int64_t _M0L6_2atmpS2366;
    if (_M0L4_2anS199 > 0) {
      int32_t _M0L6_2atmpS2367 = _M0L4_2anS199 - 1;
      _M0L6_2atmpS2366 = (int64_t)_M0L6_2atmpS2367;
    } else {
      _M0L6_2atmpS2366 = _M0MPB4Iter4nextN6constrS9980GUsbEE;
    }
    _M0L4selfS195->$1 = _M0L6_2atmpS2366;
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
    int64_t _M0L6_2atmpS2368;
    if (_M0L4_2anS205 > 0) {
      int32_t _M0L6_2atmpS2369 = _M0L4_2anS205 - 1;
      _M0L6_2atmpS2368 = (int64_t)_M0L6_2atmpS2369;
    } else {
      _M0L6_2atmpS2368
      = _M0MPB4Iter4nextN6constrS9980GUsRP19moonbitDB10RedisValueEE;
    }
    _M0L4selfS201->$1 = _M0L6_2atmpS2368;
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
    int64_t _M0L6_2atmpS2370;
    if (_M0L4_2anS211 > 0) {
      int32_t _M0L6_2atmpS2371 = _M0L4_2anS211 - 1;
      _M0L6_2atmpS2370 = (int64_t)_M0L6_2atmpS2371;
    } else {
      _M0L6_2atmpS2370 = _M0MPB4Iter4nextN6constrS9980GUsfEE;
    }
    _M0L4selfS207->$1 = _M0L6_2atmpS2370;
  }
  return _M0L6resultS208;
}

int32_t _M0IP016_24default__implPB4Show6outputGsE(
  moonbit_string_t _M0L4selfS179,
  struct _M0TPB6Logger _M0L6loggerS178
) {
  moonbit_string_t _M0L6_2atmpS2359;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2359 = _M0IPC16string6StringPB4Show10to__string(_M0L4selfS179);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS178.$0->$method_0(_M0L6loggerS178.$1, _M0L6_2atmpS2359);
  moonbit_decref(_M0L6_2atmpS2359);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGiE(
  int32_t _M0L4selfS181,
  struct _M0TPB6Logger _M0L6loggerS180
) {
  moonbit_string_t _M0L6_2atmpS2360;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2360 = _M0IPC13int3IntPB4Show10to__string(_M0L4selfS181);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS180.$0->$method_0(_M0L6loggerS180.$1, _M0L6_2atmpS2360);
  moonbit_decref(_M0L6_2atmpS2360);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGbE(
  int32_t _M0L4selfS183,
  struct _M0TPB6Logger _M0L6loggerS182
) {
  moonbit_string_t _M0L6_2atmpS2361;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2361 = _M0IPC14bool4BoolPB4Show10to__string(_M0L4selfS183);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS182.$0->$method_0(_M0L6loggerS182.$1, _M0L6_2atmpS2361);
  moonbit_decref(_M0L6_2atmpS2361);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGfE(
  float _M0L4selfS185,
  struct _M0TPB6Logger _M0L6loggerS184
) {
  moonbit_string_t _M0L6_2atmpS2362;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2362 = _M0IPC15float5FloatPB4Show10to__string(_M0L4selfS185);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS184.$0->$method_0(_M0L6loggerS184.$1, _M0L6_2atmpS2362);
  moonbit_decref(_M0L6_2atmpS2362);
  return 0;
}

int32_t _M0IP016_24default__implPB4Show6outputGmE(
  uint64_t _M0L4selfS187,
  struct _M0TPB6Logger _M0L6loggerS186
) {
  moonbit_string_t _M0L6_2atmpS2363;
  #line 159 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2363 = _M0IPC16uint646UInt64PB4Show10to__string(_M0L4selfS187);
  #line 160 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6loggerS186.$0->$method_0(_M0L6loggerS186.$1, _M0L6_2atmpS2363);
  moonbit_decref(_M0L6_2atmpS2363);
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
  moonbit_string_t _M0L8_2afieldS4247;
  #line 92 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
  _M0L8_2afieldS4247 = _M0L4selfS176.$0;
  moonbit_incref(_M0L8_2afieldS4247);
  return _M0L8_2afieldS4247;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS172,
  moonbit_string_t _M0L5valueS173,
  int32_t _M0L5startS174,
  int32_t _M0L3lenS175
) {
  int32_t _M0L6_2atmpS2358;
  int64_t _M0L6_2atmpS2357;
  struct _M0TPC16string10StringView _M0L6_2atmpS2356;
  #line 122 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2358 = _M0L5startS174 + _M0L3lenS175;
  _M0L6_2atmpS2357 = (int64_t)_M0L6_2atmpS2358;
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L6_2atmpS2356
  = _M0MPC16string6String11sub_2einner(_M0L5valueS173, _M0L5startS174, _M0L6_2atmpS2357);
  #line 123 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS172, _M0L6_2atmpS2356);
  moonbit_decref(_M0L6_2atmpS2356.$0);
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
  int32_t _if__result_4661;
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
      _if__result_4661 = _M0L3endS166 <= _M0L3lenS164;
    } else {
      _if__result_4661 = 0;
    }
  } else {
    _if__result_4661 = 0;
  }
  if (_if__result_4661) {
    if (_M0L5startS170 < _M0L3lenS164) {
      int32_t _M0L6_2atmpS2353 = _M0L4selfS165[_M0L5startS170];
      int32_t _M0L6_2atmpS2352;
      #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2352
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2353);
      if (!_M0L6_2atmpS2352) {
        
      } else {
        #line 765 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS166 < _M0L3lenS164) {
      int32_t _M0L6_2atmpS2355 = _M0L4selfS165[_M0L3endS166];
      int32_t _M0L6_2atmpS2354;
      #line 768 "/home/developer/.moon/lib/core/builtin/stringview.mbt"
      _M0L6_2atmpS2354
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2355);
      if (!_M0L6_2atmpS2354) {
        
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
  struct _M0TPB6Logger _M0L6_2atmpS2351;
  #line 116 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS163);
  _M0L6_2atmpS2351
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS163
  };
  #line 117 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS162.$0->$method_0(_M0L4showS162.$1, _M0L6_2atmpS2351);
  if (_M0L6_2atmpS2351.$1) {
    moonbit_decref(_M0L6_2atmpS2351.$1);
  }
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS161,
  struct _M0TPB4Show _M0L4showS160
) {
  struct _M0TPB6Logger _M0L6_2atmpS2350;
  #line 111 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  moonbit_incref(_M0L4selfS161);
  _M0L6_2atmpS2350
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS161
  };
  #line 112 "/home/developer/.moon/lib/core/builtin/traits.mbt"
  _M0L4showS160.$0->$method_0(_M0L4showS160.$1, _M0L6_2atmpS2350);
  if (_M0L6_2atmpS2350.$1) {
    moonbit_decref(_M0L6_2atmpS2350.$1);
  }
  return 0;
}

int32_t _M0FPB13finalize__acc(uint32_t _M0L3accS159) {
  uint32_t _M0L6_2atmpS2349;
  #line 444 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  #line 445 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2349 = _M0FPB14avalanche__acc(_M0L3accS159);
  return *(int32_t*)&_M0L6_2atmpS2349;
}

uint32_t _M0FPB14avalanche__acc(uint32_t _M0L3accS158) {
  uint32_t _M0Lm3accS157;
  uint32_t _M0L6_2atmpS2338;
  uint32_t _M0L6_2atmpS2340;
  uint32_t _M0L6_2atmpS2339;
  uint32_t _M0L6_2atmpS2341;
  uint32_t _M0L6_2atmpS2342;
  uint32_t _M0L6_2atmpS2344;
  uint32_t _M0L6_2atmpS2343;
  uint32_t _M0L6_2atmpS2345;
  uint32_t _M0L6_2atmpS2346;
  uint32_t _M0L6_2atmpS2348;
  uint32_t _M0L6_2atmpS2347;
  #line 449 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0Lm3accS157 = _M0L3accS158;
  _M0L6_2atmpS2338 = _M0Lm3accS157;
  _M0L6_2atmpS2340 = _M0Lm3accS157;
  _M0L6_2atmpS2339 = _M0L6_2atmpS2340 >> 15;
  _M0Lm3accS157 = _M0L6_2atmpS2338 ^ _M0L6_2atmpS2339;
  _M0L6_2atmpS2341 = _M0Lm3accS157;
  _M0Lm3accS157 = _M0L6_2atmpS2341 * 2246822519u;
  _M0L6_2atmpS2342 = _M0Lm3accS157;
  _M0L6_2atmpS2344 = _M0Lm3accS157;
  _M0L6_2atmpS2343 = _M0L6_2atmpS2344 >> 13;
  _M0Lm3accS157 = _M0L6_2atmpS2342 ^ _M0L6_2atmpS2343;
  _M0L6_2atmpS2345 = _M0Lm3accS157;
  _M0Lm3accS157 = _M0L6_2atmpS2345 * 3266489917u;
  _M0L6_2atmpS2346 = _M0Lm3accS157;
  _M0L6_2atmpS2348 = _M0Lm3accS157;
  _M0L6_2atmpS2347 = _M0L6_2atmpS2348 >> 16;
  _M0Lm3accS157 = _M0L6_2atmpS2346 ^ _M0L6_2atmpS2347;
  return _M0Lm3accS157;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS156) {
  int64_t _M0L6_2atmpS2337;
  #line 907 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2337 = (int64_t)_M0L4selfS156;
  return *(uint64_t*)&_M0L6_2atmpS2337;
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS155,
  moonbit_string_t _M0L3strS154
) {
  int32_t _M0L8str__lenS153;
  int32_t _M0L3lenS2332;
  int32_t _M0L6_2atmpS2331;
  uint16_t* _M0L4dataS2333;
  int32_t _M0L3lenS2334;
  int32_t _M0L3lenS2336;
  int32_t _M0L6_2atmpS2335;
  #line 86 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L8str__lenS153 = Moonbit_array_length(_M0L3strS154);
  _M0L3lenS2332 = _M0L4selfS155->$1;
  _M0L6_2atmpS2331 = _M0L3lenS2332 + _M0L8str__lenS153;
  #line 88 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS155, _M0L6_2atmpS2331);
  _M0L4dataS2333 = _M0L4selfS155->$0;
  _M0L3lenS2334 = _M0L4selfS155->$1;
  moonbit_incref(_M0L4dataS2333);
  #line 89 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray26unsafe__blit__from__string(_M0L4dataS2333, _M0L3lenS2334, _M0L3strS154, 0, _M0L8str__lenS153);
  moonbit_decref(_M0L4dataS2333);
  _M0L3lenS2336 = _M0L4selfS155->$1;
  _M0L6_2atmpS2335 = _M0L3lenS2336 + _M0L8str__lenS153;
  _M0L4selfS155->$1 = _M0L6_2atmpS2335;
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
      int32_t _M0L6_2atmpS2328 = _M0L3strS150[_M0L1iS147];
      int32_t _M0L6_2atmpS2329;
      int32_t _M0L6_2atmpS2330;
      if (
        _M0L1jS148 < 0 || _M0L1jS148 >= Moonbit_array_length(_M0L4selfS149)
      ) {
        #line 80 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
        moonbit_panic();
      }
      _M0L4selfS149[_M0L1jS148] = _M0L6_2atmpS2328;
      _M0L6_2atmpS2329 = _M0L1iS147 + 1;
      _M0L6_2atmpS2330 = _M0L1jS148 + 1;
      _M0L1iS147 = _M0L6_2atmpS2329;
      _M0L1jS148 = _M0L6_2atmpS2330;
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
    int32_t _M0L3lenS2307 = _M0L4selfS141->$1;
    int32_t _M0L6_2atmpS2306 = _M0L3lenS2307 + 1;
    uint16_t* _M0L4dataS2308;
    int32_t _M0L3lenS2309;
    int32_t _M0L6_2atmpS2310;
    int32_t _M0L3lenS2312;
    int32_t _M0L6_2atmpS2311;
    #line 98 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS141, _M0L6_2atmpS2306);
    _M0L4dataS2308 = _M0L4selfS141->$0;
    _M0L3lenS2309 = _M0L4selfS141->$1;
    moonbit_incref(_M0L4dataS2308);
    #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2310 = _M0MPC14uint4UInt10to__uint16(_M0L4codeS139);
    if (
      _M0L3lenS2309 < 0
      || _M0L3lenS2309 >= Moonbit_array_length(_M0L4dataS2308)
    ) {
      #line 99 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2308[_M0L3lenS2309] = _M0L6_2atmpS2310;
    moonbit_decref(_M0L4dataS2308);
    _M0L3lenS2312 = _M0L4selfS141->$1;
    _M0L6_2atmpS2311 = _M0L3lenS2312 + 1;
    _M0L4selfS141->$1 = _M0L6_2atmpS2311;
  } else if (_M0L4codeS139 <= 1114111u) {
    int32_t _M0L3lenS2314 = _M0L4selfS141->$1;
    int32_t _M0L6_2atmpS2313 = _M0L3lenS2314 + 2;
    uint32_t _M0L4codeS142;
    uint16_t* _M0L4dataS2315;
    int32_t _M0L3lenS2316;
    uint32_t _M0L6_2atmpS2319;
    uint32_t _M0L6_2atmpS2318;
    int32_t _M0L6_2atmpS2317;
    uint16_t* _M0L4dataS2320;
    int32_t _M0L3lenS2325;
    int32_t _M0L6_2atmpS2321;
    uint32_t _M0L6_2atmpS2324;
    uint32_t _M0L6_2atmpS2323;
    int32_t _M0L6_2atmpS2322;
    int32_t _M0L3lenS2327;
    int32_t _M0L6_2atmpS2326;
    #line 102 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS141, _M0L6_2atmpS2313);
    _M0L4codeS142 = _M0L4codeS139 - 65536u;
    _M0L4dataS2315 = _M0L4selfS141->$0;
    _M0L3lenS2316 = _M0L4selfS141->$1;
    _M0L6_2atmpS2319 = _M0L4codeS142 >> 10;
    _M0L6_2atmpS2318 = 55296u + _M0L6_2atmpS2319;
    moonbit_incref(_M0L4dataS2315);
    #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2317 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2318);
    if (
      _M0L3lenS2316 < 0
      || _M0L3lenS2316 >= Moonbit_array_length(_M0L4dataS2315)
    ) {
      #line 104 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2315[_M0L3lenS2316] = _M0L6_2atmpS2317;
    moonbit_decref(_M0L4dataS2315);
    _M0L4dataS2320 = _M0L4selfS141->$0;
    _M0L3lenS2325 = _M0L4selfS141->$1;
    _M0L6_2atmpS2321 = _M0L3lenS2325 + 1;
    _M0L6_2atmpS2324 = _M0L4codeS142 & 1023u;
    _M0L6_2atmpS2323 = 56320u + _M0L6_2atmpS2324;
    moonbit_incref(_M0L4dataS2320);
    #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2322 = _M0MPC14uint4UInt10to__uint16(_M0L6_2atmpS2323);
    if (
      _M0L6_2atmpS2321 < 0
      || _M0L6_2atmpS2321 >= Moonbit_array_length(_M0L4dataS2320)
    ) {
      #line 105 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2320[_M0L6_2atmpS2321] = _M0L6_2atmpS2322;
    moonbit_decref(_M0L4dataS2320);
    _M0L3lenS2327 = _M0L4selfS141->$1;
    _M0L6_2atmpS2326 = _M0L3lenS2327 + 2;
    _M0L4selfS141->$1 = _M0L6_2atmpS2326;
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
  uint16_t* _M0L4dataS2305;
  int32_t _M0L12current__lenS132;
  int32_t _M0L13enough__spaceS135;
  int32_t _M0L13enough__spaceS136;
  uint16_t* _M0L4dataS2301;
  int32_t _M0L6_2atmpS2302;
  int32_t _M0L3lenS2303;
  uint16_t* _M0L9new__dataS138;
  uint16_t* _M0L6_2aoldS4252;
  #line 46 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L4dataS2305 = _M0L4selfS133->$0;
  _M0L12current__lenS132 = Moonbit_array_length(_M0L4dataS2305);
  if (_M0L8requiredS134 <= _M0L12current__lenS132) {
    return 0;
  }
  _M0L13enough__spaceS136 = _M0L12current__lenS132;
  while (1) {
    if (_M0L13enough__spaceS136 < _M0L8requiredS134) {
      int32_t _M0L6_2atmpS2304 = _M0L13enough__spaceS136 * 2;
      _M0L13enough__spaceS136 = _M0L6_2atmpS2304;
      continue;
    } else {
      _M0L13enough__spaceS135 = _M0L13enough__spaceS136;
    }
    break;
  }
  _M0L4dataS2301 = _M0L4selfS133->$0;
  moonbit_incref(_M0L4dataS2301);
  #line 64 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L6_2atmpS2302 = _M0IPC16uint166UInt16PB7Default7default();
  _M0L3lenS2303 = _M0L4selfS133->$1;
  #line 61 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L9new__dataS138
  = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS2301, _M0L13enough__spaceS135, _M0L6_2atmpS2302, _M0L3lenS2303, 0, 0);
  moonbit_decref(_M0L4dataS2301);
  _M0L6_2aoldS4252 = _M0L4selfS133->$0;
  moonbit_decref(_M0L6_2aoldS4252);
  _M0L4selfS133->$0 = _M0L9new__dataS138;
  return 0;
}

int32_t _M0MPC14uint4UInt10to__uint16(uint32_t _M0L4selfS131) {
  int32_t _M0L6_2atmpS2300;
  #line 2676 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2300 = *(int32_t*)&_M0L4selfS131;
  return (uint16_t)_M0L6_2atmpS2300;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS130) {
  int32_t _M0L6_2atmpS2299;
  #line 1254 "/home/developer/.moon/lib/core/builtin/intrinsics.mbt"
  _M0L6_2atmpS2299 = _M0L4selfS130;
  return *(uint32_t*)&_M0L6_2atmpS2299;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS128
) {
  int32_t _M0L3lenS2291;
  uint16_t* _M0L4dataS2293;
  int32_t _M0L6_2atmpS2292;
  #line 148 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  _M0L3lenS2291 = _M0L4selfS128->$1;
  _M0L4dataS2293 = _M0L4selfS128->$0;
  _M0L6_2atmpS2292 = Moonbit_array_length(_M0L4dataS2293);
  if (_M0L3lenS2291 == _M0L6_2atmpS2292) {
    uint16_t* _M0L4dataS2294 = _M0L4selfS128->$0;
    moonbit_incref(_M0L4dataS2294);
    return _M0L4dataS2294;
  } else {
    uint16_t* _M0L4dataS2295 = _M0L4selfS128->$0;
    int32_t _M0L3lenS2296 = _M0L4selfS128->$1;
    int32_t _M0L6_2atmpS2297;
    int32_t _M0L3lenS2298;
    uint16_t* _M0L4dataS129;
    moonbit_incref(_M0L4dataS2295);
    #line 155 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L6_2atmpS2297 = _M0IPC16uint166UInt16PB7Default7default();
    _M0L3lenS2298 = _M0L4selfS128->$1;
    #line 152 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
    _M0L4dataS129
    = _M0MPC15array10FixedArray23make__and__blit_2einnerGkE(_M0L4dataS2295, _M0L3lenS2296, _M0L6_2atmpS2297, _M0L3lenS2298, 0, 0);
    moonbit_decref(_M0L4dataS2295);
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
  int32_t _if__result_4664;
  #line 97 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L13allocate__lenS121 >= 0) {
    if (_M0L3lenS122 >= 0) {
      if (_M0L11src__offsetS123 >= 0) {
        if (_M0L11dst__offsetS124 >= 0) {
          int32_t _M0L6_2atmpS2287 = _M0L11src__offsetS123 + _M0L3lenS122;
          int32_t _M0L6_2atmpS2288 = Moonbit_array_length(_M0L3srcS125);
          if (_M0L6_2atmpS2287 <= _M0L6_2atmpS2288) {
            int32_t _M0L6_2atmpS2286 = _M0L11dst__offsetS124 + _M0L3lenS122;
            _if__result_4664 = _M0L6_2atmpS2286 <= _M0L13allocate__lenS121;
          } else {
            _if__result_4664 = 0;
          }
        } else {
          _if__result_4664 = 0;
        }
      } else {
        _if__result_4664 = 0;
      }
    } else {
      _if__result_4664 = 0;
    }
  } else {
    _if__result_4664 = 0;
  }
  if (_if__result_4664) {
    moonbit_incref(_M0L3srcS125);
    #line 115 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    return _M0MPC15array10FixedArray23unsafe__make__and__blitGkE(_M0L3srcS125, _M0L13allocate__lenS121, _M0L4initS126, _M0L11src__offsetS123, _M0L11dst__offsetS124, _M0L3lenS122);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS127;
    int32_t _M0L6_2atmpS2290;
    moonbit_string_t _M0L6_2atmpS2289;
    uint16_t* _result_4665;
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
    _M0L6_2atmpS2290 = Moonbit_array_length(_M0L3srcS125);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS127, _M0L6_2atmpS2290);
    #line 112 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _M0L6_2atmpS2289
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS127);
    moonbit_decref(_M0L18_2astring__builderS127);
    #line 111 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
    _result_4665 = _M0FPC15abort5abortGAkE(_M0L6_2atmpS2289);
    moonbit_decref(_M0L6_2atmpS2289);
    return _result_4665;
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
  struct _M0TPB13StringBuilder* _block_4666;
  #line 32 "/home/developer/.moon/lib/core/builtin/stringbuilder_buffer.mbt"
  if (_M0L10size__hintS112 < 1) {
    _M0L7initialS111 = 1;
  } else {
    int32_t _M0L6_2atmpS2285 = _M0L10size__hintS112 + 1;
    _M0L7initialS111 = _M0L6_2atmpS2285 / 2;
  }
  _M0L4dataS113 = (uint16_t*)moonbit_make_string(_M0L7initialS111, 0);
  _block_4666
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4666)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 132, 0);
  _block_4666->$0 = _M0L4dataS113;
  _block_4666->$1 = 0;
  return _block_4666;
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGsE(
  moonbit_string_t* _M0L3srcS97,
  int32_t _M0L13allocate__lenS93,
  int32_t _M0L3lenS94,
  int32_t _M0L11src__offsetS95,
  int32_t _M0L11dst__offsetS96
) {
  int32_t _if__result_4667;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS93 >= 0) {
    if (_M0L3lenS94 >= 0) {
      if (_M0L11src__offsetS95 >= 0) {
        if (_M0L11dst__offsetS96 >= 0) {
          int32_t _M0L6_2atmpS2271 = _M0L11src__offsetS95 + _M0L3lenS94;
          int32_t _M0L6_2atmpS2272;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2272
          = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS97);
          if (_M0L6_2atmpS2271 <= _M0L6_2atmpS2272) {
            int32_t _M0L6_2atmpS2270 = _M0L11dst__offsetS96 + _M0L3lenS94;
            _if__result_4667 = _M0L6_2atmpS2270 <= _M0L13allocate__lenS93;
          } else {
            _if__result_4667 = 0;
          }
        } else {
          _if__result_4667 = 0;
        }
      } else {
        _if__result_4667 = 0;
      }
    } else {
      _if__result_4667 = 0;
    }
  } else {
    _if__result_4667 = 0;
  }
  if (_if__result_4667) {
    moonbit_incref(_M0L3srcS97);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS93, (moonbit_string_t)moonbit_string_literal_96.data, _M0L3srcS97, _M0L11src__offsetS95, _M0L11dst__offsetS96, _M0L3lenS94);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS98;
    int32_t _M0L6_2atmpS2274;
    moonbit_string_t _M0L6_2atmpS2273;
    moonbit_string_t* _result_4668;
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
    _M0L6_2atmpS2274 = _M0MPB18UninitializedArray6lengthGsE(_M0L3srcS97);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS98, _M0L6_2atmpS2274);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2273
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS98);
    moonbit_decref(_M0L18_2astring__builderS98);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4668
    = _M0FPC15abort5abortGRPB18UninitializedArrayGsEE(_M0L6_2atmpS2273);
    moonbit_decref(_M0L6_2atmpS2273);
    return _result_4668;
  }
}

moonbit_string_t* _M0MPB18UninitializedArray23make__and__blit_2einnerGOsE(
  moonbit_string_t* _M0L3srcS103,
  int32_t _M0L13allocate__lenS99,
  int32_t _M0L3lenS100,
  int32_t _M0L11src__offsetS101,
  int32_t _M0L11dst__offsetS102
) {
  int32_t _if__result_4669;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS99 >= 0) {
    if (_M0L3lenS100 >= 0) {
      if (_M0L11src__offsetS101 >= 0) {
        if (_M0L11dst__offsetS102 >= 0) {
          int32_t _M0L6_2atmpS2276 = _M0L11src__offsetS101 + _M0L3lenS100;
          int32_t _M0L6_2atmpS2277;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2277
          = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS103);
          if (_M0L6_2atmpS2276 <= _M0L6_2atmpS2277) {
            int32_t _M0L6_2atmpS2275 = _M0L11dst__offsetS102 + _M0L3lenS100;
            _if__result_4669 = _M0L6_2atmpS2275 <= _M0L13allocate__lenS99;
          } else {
            _if__result_4669 = 0;
          }
        } else {
          _if__result_4669 = 0;
        }
      } else {
        _if__result_4669 = 0;
      }
    } else {
      _if__result_4669 = 0;
    }
  } else {
    _if__result_4669 = 0;
  }
  if (_if__result_4669) {
    moonbit_incref(_M0L3srcS103);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (moonbit_string_t*)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS99, 0, _M0L3srcS103, _M0L11src__offsetS101, _M0L11dst__offsetS102, _M0L3lenS100);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS104;
    int32_t _M0L6_2atmpS2279;
    moonbit_string_t _M0L6_2atmpS2278;
    moonbit_string_t* _result_4670;
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
    _M0L6_2atmpS2279 = _M0MPB18UninitializedArray6lengthGOsE(_M0L3srcS103);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS104, _M0L6_2atmpS2279);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2278
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS104);
    moonbit_decref(_M0L18_2astring__builderS104);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4670
    = _M0FPC15abort5abortGRPB18UninitializedArrayGOsEE(_M0L6_2atmpS2278);
    moonbit_decref(_M0L6_2atmpS2278);
    return _result_4670;
  }
}

struct _M0TUsfE** _M0MPB18UninitializedArray23make__and__blit_2einnerGUsfEE(
  struct _M0TUsfE** _M0L3srcS109,
  int32_t _M0L13allocate__lenS105,
  int32_t _M0L3lenS106,
  int32_t _M0L11src__offsetS107,
  int32_t _M0L11dst__offsetS108
) {
  int32_t _if__result_4671;
  #line 168 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
  if (_M0L13allocate__lenS105 >= 0) {
    if (_M0L3lenS106 >= 0) {
      if (_M0L11src__offsetS107 >= 0) {
        if (_M0L11dst__offsetS108 >= 0) {
          int32_t _M0L6_2atmpS2281 = _M0L11src__offsetS107 + _M0L3lenS106;
          int32_t _M0L6_2atmpS2282;
          #line 179 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
          _M0L6_2atmpS2282
          = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS109);
          if (_M0L6_2atmpS2281 <= _M0L6_2atmpS2282) {
            int32_t _M0L6_2atmpS2280 = _M0L11dst__offsetS108 + _M0L3lenS106;
            _if__result_4671 = _M0L6_2atmpS2280 <= _M0L13allocate__lenS105;
          } else {
            _if__result_4671 = 0;
          }
        } else {
          _if__result_4671 = 0;
        }
      } else {
        _if__result_4671 = 0;
      }
    } else {
      _if__result_4671 = 0;
    }
  } else {
    _if__result_4671 = 0;
  }
  if (_if__result_4671) {
    moonbit_incref(_M0L3srcS109);
    #line 185 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    return (struct _M0TUsfE**)moonbit_make_ref_array_with_blit(_M0L13allocate__lenS105, 0, _M0L3srcS109, _M0L11src__offsetS107, _M0L11dst__offsetS108, _M0L3lenS106);
  } else {
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS110;
    int32_t _M0L6_2atmpS2284;
    moonbit_string_t _M0L6_2atmpS2283;
    struct _M0TUsfE** _result_4672;
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
    _M0L6_2atmpS2284 = _M0MPB18UninitializedArray6lengthGUsfEE(_M0L3srcS109);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS110, _M0L6_2atmpS2284);
    #line 182 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _M0L6_2atmpS2283
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS110);
    moonbit_decref(_M0L18_2astring__builderS110);
    #line 181 "/home/developer/.moon/lib/core/builtin/uninitialized_array.mbt"
    _result_4672
    = _M0FPC15abort5abortGRPB18UninitializedArrayGUsfEEE(_M0L6_2atmpS2283);
    moonbit_decref(_M0L6_2atmpS2283);
    return _result_4672;
  }
}

int32_t _M0MPB13StringBuilder13write__objectGsE(
  struct _M0TPB13StringBuilder* _M0L4selfS84,
  moonbit_string_t _M0L3objS83
) {
  struct _M0TPB6Logger _M0L6_2atmpS2265;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS84);
  _M0L6_2atmpS2265
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS84
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGsE(_M0L3objS83, _M0L6_2atmpS2265);
  if (_M0L6_2atmpS2265.$1) {
    moonbit_decref(_M0L6_2atmpS2265.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGiE(
  struct _M0TPB13StringBuilder* _M0L4selfS86,
  int32_t _M0L3objS85
) {
  struct _M0TPB6Logger _M0L6_2atmpS2266;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS86);
  _M0L6_2atmpS2266
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS86
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGiE(_M0L3objS85, _M0L6_2atmpS2266);
  if (_M0L6_2atmpS2266.$1) {
    moonbit_decref(_M0L6_2atmpS2266.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGbE(
  struct _M0TPB13StringBuilder* _M0L4selfS88,
  int32_t _M0L3objS87
) {
  struct _M0TPB6Logger _M0L6_2atmpS2267;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS88);
  _M0L6_2atmpS2267
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS88
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGbE(_M0L3objS87, _M0L6_2atmpS2267);
  if (_M0L6_2atmpS2267.$1) {
    moonbit_decref(_M0L6_2atmpS2267.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGfE(
  struct _M0TPB13StringBuilder* _M0L4selfS90,
  float _M0L3objS89
) {
  struct _M0TPB6Logger _M0L6_2atmpS2268;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS90);
  _M0L6_2atmpS2268
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS90
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGfE(_M0L3objS89, _M0L6_2atmpS2268);
  if (_M0L6_2atmpS2268.$1) {
    moonbit_decref(_M0L6_2atmpS2268.$1);
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGmE(
  struct _M0TPB13StringBuilder* _M0L4selfS92,
  uint64_t _M0L3objS91
) {
  struct _M0TPB6Logger _M0L6_2atmpS2269;
  #line 17 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  moonbit_incref(_M0L4selfS92);
  _M0L6_2atmpS2269
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS92
  };
  #line 23 "/home/developer/.moon/lib/core/builtin/stringbuilder.mbt"
  _M0IP016_24default__implPB4Show6outputGmE(_M0L3objS91, _M0L6_2atmpS2269);
  if (_M0L6_2atmpS2269.$1) {
    moonbit_decref(_M0L6_2atmpS2269.$1);
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
  int32_t _if__result_4673;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS14 == _M0L3srcS15) {
    _if__result_4673 = _M0L11dst__offsetS16 < _M0L11src__offsetS17;
  } else {
    _if__result_4673 = 0;
  }
  if (_if__result_4673) {
    int32_t _M0L1iS18 = 0;
    while (1) {
      if (_M0L1iS18 < _M0L3lenS19) {
        int32_t _M0L6_2atmpS2229 = _M0L11dst__offsetS16 + _M0L1iS18;
        int32_t _M0L6_2atmpS2231 = _M0L11src__offsetS17 + _M0L1iS18;
        int32_t _M0L6_2atmpS2230;
        int32_t _M0L6_2atmpS2232;
        if (
          _M0L6_2atmpS2231 < 0
          || _M0L6_2atmpS2231 >= Moonbit_array_length(_M0L3srcS15)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2230 = (int32_t)_M0L3srcS15[_M0L6_2atmpS2231];
        if (
          _M0L6_2atmpS2229 < 0
          || _M0L6_2atmpS2229 >= Moonbit_array_length(_M0L3dstS14)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS14[_M0L6_2atmpS2229] = _M0L6_2atmpS2230;
        _M0L6_2atmpS2232 = _M0L1iS18 + 1;
        _M0L1iS18 = _M0L6_2atmpS2232;
        continue;
      } else {
        moonbit_decref(_M0L3srcS15);
        moonbit_decref(_M0L3dstS14);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2237 = _M0L3lenS19 - 1;
    int32_t _M0L1iS21 = _M0L6_2atmpS2237;
    while (1) {
      if (_M0L1iS21 >= 0) {
        int32_t _M0L6_2atmpS2233 = _M0L11dst__offsetS16 + _M0L1iS21;
        int32_t _M0L6_2atmpS2235 = _M0L11src__offsetS17 + _M0L1iS21;
        int32_t _M0L6_2atmpS2234;
        int32_t _M0L6_2atmpS2236;
        if (
          _M0L6_2atmpS2235 < 0
          || _M0L6_2atmpS2235 >= Moonbit_array_length(_M0L3srcS15)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2234 = (int32_t)_M0L3srcS15[_M0L6_2atmpS2235];
        if (
          _M0L6_2atmpS2233 < 0
          || _M0L6_2atmpS2233 >= Moonbit_array_length(_M0L3dstS14)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS14[_M0L6_2atmpS2233] = _M0L6_2atmpS2234;
        _M0L6_2atmpS2236 = _M0L1iS21 - 1;
        _M0L1iS21 = _M0L6_2atmpS2236;
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
  int32_t _if__result_4676;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS23 == _M0L3srcS24) {
    _if__result_4676 = _M0L11dst__offsetS25 < _M0L11src__offsetS26;
  } else {
    _if__result_4676 = 0;
  }
  if (_if__result_4676) {
    int32_t _M0L1iS27 = 0;
    while (1) {
      if (_M0L1iS27 < _M0L3lenS28) {
        int32_t _M0L6_2atmpS2238 = _M0L11dst__offsetS25 + _M0L1iS27;
        int32_t _M0L6_2atmpS2240 = _M0L11src__offsetS26 + _M0L1iS27;
        moonbit_string_t _M0L6_2atmpS2239;
        moonbit_string_t _M0L6_2aoldS4258;
        int32_t _M0L6_2atmpS2241;
        if (
          _M0L6_2atmpS2240 < 0
          || _M0L6_2atmpS2240 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2239 = (moonbit_string_t)_M0L3srcS24[_M0L6_2atmpS2240];
        if (
          _M0L6_2atmpS2238 < 0
          || _M0L6_2atmpS2238 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4258 = (moonbit_string_t)_M0L3dstS23[_M0L6_2atmpS2238];
        moonbit_incref(_M0L6_2atmpS2239);
        moonbit_decref(_M0L6_2aoldS4258);
        _M0L3dstS23[_M0L6_2atmpS2238] = _M0L6_2atmpS2239;
        _M0L6_2atmpS2241 = _M0L1iS27 + 1;
        _M0L1iS27 = _M0L6_2atmpS2241;
        continue;
      } else {
        moonbit_decref(_M0L3srcS24);
        moonbit_decref(_M0L3dstS23);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2246 = _M0L3lenS28 - 1;
    int32_t _M0L1iS30 = _M0L6_2atmpS2246;
    while (1) {
      if (_M0L1iS30 >= 0) {
        int32_t _M0L6_2atmpS2242 = _M0L11dst__offsetS25 + _M0L1iS30;
        int32_t _M0L6_2atmpS2244 = _M0L11src__offsetS26 + _M0L1iS30;
        moonbit_string_t _M0L6_2atmpS2243;
        moonbit_string_t _M0L6_2aoldS4260;
        int32_t _M0L6_2atmpS2245;
        if (
          _M0L6_2atmpS2244 < 0
          || _M0L6_2atmpS2244 >= Moonbit_array_length(_M0L3srcS24)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2243 = (moonbit_string_t)_M0L3srcS24[_M0L6_2atmpS2244];
        if (
          _M0L6_2atmpS2242 < 0
          || _M0L6_2atmpS2242 >= Moonbit_array_length(_M0L3dstS23)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4260 = (moonbit_string_t)_M0L3dstS23[_M0L6_2atmpS2242];
        moonbit_incref(_M0L6_2atmpS2243);
        moonbit_decref(_M0L6_2aoldS4260);
        _M0L3dstS23[_M0L6_2atmpS2242] = _M0L6_2atmpS2243;
        _M0L6_2atmpS2245 = _M0L1iS30 - 1;
        _M0L1iS30 = _M0L6_2atmpS2245;
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
  int32_t _if__result_4679;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS32 == _M0L3srcS33) {
    _if__result_4679 = _M0L11dst__offsetS34 < _M0L11src__offsetS35;
  } else {
    _if__result_4679 = 0;
  }
  if (_if__result_4679) {
    int32_t _M0L1iS36 = 0;
    while (1) {
      if (_M0L1iS36 < _M0L3lenS37) {
        int32_t _M0L6_2atmpS2247 = _M0L11dst__offsetS34 + _M0L1iS36;
        int32_t _M0L6_2atmpS2249 = _M0L11src__offsetS35 + _M0L1iS36;
        moonbit_string_t _M0L6_2atmpS2248;
        moonbit_string_t _M0L6_2aoldS4262;
        int32_t _M0L6_2atmpS2250;
        if (
          _M0L6_2atmpS2249 < 0
          || _M0L6_2atmpS2249 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2248 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS2249];
        if (
          _M0L6_2atmpS2247 < 0
          || _M0L6_2atmpS2247 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4262 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS2247];
        if (_M0L6_2atmpS2248) {
          moonbit_incref(_M0L6_2atmpS2248);
        }
        if (_M0L6_2aoldS4262) {
          moonbit_decref(_M0L6_2aoldS4262);
        }
        _M0L3dstS32[_M0L6_2atmpS2247] = _M0L6_2atmpS2248;
        _M0L6_2atmpS2250 = _M0L1iS36 + 1;
        _M0L1iS36 = _M0L6_2atmpS2250;
        continue;
      } else {
        moonbit_decref(_M0L3srcS33);
        moonbit_decref(_M0L3dstS32);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2255 = _M0L3lenS37 - 1;
    int32_t _M0L1iS39 = _M0L6_2atmpS2255;
    while (1) {
      if (_M0L1iS39 >= 0) {
        int32_t _M0L6_2atmpS2251 = _M0L11dst__offsetS34 + _M0L1iS39;
        int32_t _M0L6_2atmpS2253 = _M0L11src__offsetS35 + _M0L1iS39;
        moonbit_string_t _M0L6_2atmpS2252;
        moonbit_string_t _M0L6_2aoldS4264;
        int32_t _M0L6_2atmpS2254;
        if (
          _M0L6_2atmpS2253 < 0
          || _M0L6_2atmpS2253 >= Moonbit_array_length(_M0L3srcS33)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2252 = (moonbit_string_t)_M0L3srcS33[_M0L6_2atmpS2253];
        if (
          _M0L6_2atmpS2251 < 0
          || _M0L6_2atmpS2251 >= Moonbit_array_length(_M0L3dstS32)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4264 = (moonbit_string_t)_M0L3dstS32[_M0L6_2atmpS2251];
        if (_M0L6_2atmpS2252) {
          moonbit_incref(_M0L6_2atmpS2252);
        }
        if (_M0L6_2aoldS4264) {
          moonbit_decref(_M0L6_2aoldS4264);
        }
        _M0L3dstS32[_M0L6_2atmpS2251] = _M0L6_2atmpS2252;
        _M0L6_2atmpS2254 = _M0L1iS39 - 1;
        _M0L1iS39 = _M0L6_2atmpS2254;
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
  int32_t _if__result_4682;
  #line 38 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
  if (_M0L3dstS41 == _M0L3srcS42) {
    _if__result_4682 = _M0L11dst__offsetS43 < _M0L11src__offsetS44;
  } else {
    _if__result_4682 = 0;
  }
  if (_if__result_4682) {
    int32_t _M0L1iS45 = 0;
    while (1) {
      if (_M0L1iS45 < _M0L3lenS46) {
        int32_t _M0L6_2atmpS2256 = _M0L11dst__offsetS43 + _M0L1iS45;
        int32_t _M0L6_2atmpS2258 = _M0L11src__offsetS44 + _M0L1iS45;
        struct _M0TUsfE* _M0L6_2atmpS2257;
        struct _M0TUsfE* _M0L6_2aoldS4266;
        int32_t _M0L6_2atmpS2259;
        if (
          _M0L6_2atmpS2258 < 0
          || _M0L6_2atmpS2258 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2257 = (struct _M0TUsfE*)_M0L3srcS42[_M0L6_2atmpS2258];
        if (
          _M0L6_2atmpS2256 < 0
          || _M0L6_2atmpS2256 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 50 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4266 = (struct _M0TUsfE*)_M0L3dstS41[_M0L6_2atmpS2256];
        if (_M0L6_2atmpS2257) {
          moonbit_incref(_M0L6_2atmpS2257);
        }
        if (_M0L6_2aoldS4266) {
          moonbit_decref(_M0L6_2aoldS4266);
        }
        _M0L3dstS41[_M0L6_2atmpS2256] = _M0L6_2atmpS2257;
        _M0L6_2atmpS2259 = _M0L1iS45 + 1;
        _M0L1iS45 = _M0L6_2atmpS2259;
        continue;
      } else {
        moonbit_decref(_M0L3srcS42);
        moonbit_decref(_M0L3dstS41);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS2264 = _M0L3lenS46 - 1;
    int32_t _M0L1iS48 = _M0L6_2atmpS2264;
    while (1) {
      if (_M0L1iS48 >= 0) {
        int32_t _M0L6_2atmpS2260 = _M0L11dst__offsetS43 + _M0L1iS48;
        int32_t _M0L6_2atmpS2262 = _M0L11src__offsetS44 + _M0L1iS48;
        struct _M0TUsfE* _M0L6_2atmpS2261;
        struct _M0TUsfE* _M0L6_2aoldS4268;
        int32_t _M0L6_2atmpS2263;
        if (
          _M0L6_2atmpS2262 < 0
          || _M0L6_2atmpS2262 >= Moonbit_array_length(_M0L3srcS42)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2261 = (struct _M0TUsfE*)_M0L3srcS42[_M0L6_2atmpS2262];
        if (
          _M0L6_2atmpS2260 < 0
          || _M0L6_2atmpS2260 >= Moonbit_array_length(_M0L3dstS41)
        ) {
          #line 54 "/home/developer/.moon/lib/core/builtin/fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4268 = (struct _M0TUsfE*)_M0L3dstS41[_M0L6_2atmpS2260];
        if (_M0L6_2atmpS2261) {
          moonbit_incref(_M0L6_2atmpS2261);
        }
        if (_M0L6_2aoldS4268) {
          moonbit_decref(_M0L6_2aoldS4268);
        }
        _M0L3dstS41[_M0L6_2atmpS2260] = _M0L6_2atmpS2261;
        _M0L6_2atmpS2263 = _M0L1iS48 - 1;
        _M0L1iS48 = _M0L6_2atmpS2263;
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
  uint32_t _M0L6_2atmpS2228;
  uint32_t _M0L6_2atmpS2227;
  uint32_t _M0L6_2atmpS2226;
  #line 465 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2228 = _M0L5inputS10 * 3266489917u;
  _M0L6_2atmpS2227 = _M0L3accS9 + _M0L6_2atmpS2228;
  #line 466 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2226 = _M0FPB4rotl(_M0L6_2atmpS2227, 17);
  return _M0L6_2atmpS2226 * 668265263u;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS7, int32_t _M0L1rS8) {
  uint32_t _M0L6_2atmpS2223;
  int32_t _M0L6_2atmpS2225;
  uint32_t _M0L6_2atmpS2224;
  #line 475 "/home/developer/.moon/lib/core/builtin/hasher.mbt"
  _M0L6_2atmpS2223 = _M0L1xS7 << (_M0L1rS8 & 31);
  _M0L6_2atmpS2225 = 32 - _M0L1rS8;
  _M0L6_2atmpS2224 = _M0L1xS7 >> (_M0L6_2atmpS2225 & 31);
  return _M0L6_2atmpS2223 | _M0L6_2atmpS2224;
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
  void* _M0L11_2aobj__ptrS2091,
  struct _M0TPB4Show _M0L8_2aparamS2090
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2089 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2091;
  _M0IP016_24default__implPB6Logger5writeGRPB13StringBuilderE(_M0L7_2aselfS2089, _M0L8_2aparamS2090);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger84write__string__interpolation_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS2088,
  struct _M0TPB4Show _M0L8_2aparamS2087
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2086 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2088;
  _M0IP016_24default__implPB6Logger28write__string__interpolationGRPB13StringBuilderE(_M0L7_2aselfS2086, _M0L8_2aparamS2087);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS2085,
  int32_t _M0L8_2aparamS2084
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2083 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2085;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS2083, _M0L8_2aparamS2084);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS2082,
  struct _M0TPC16string10StringView _M0L8_2aparamS2081
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2080 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2082;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS2080, _M0L8_2aparamS2081);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS2079,
  moonbit_string_t _M0L8_2aparamS2076,
  int32_t _M0L8_2aparamS2077,
  int32_t _M0L8_2aparamS2078
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2075 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2079;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS2075, _M0L8_2aparamS2076, _M0L8_2aparamS2077, _M0L8_2aparamS2078);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS2074,
  moonbit_string_t _M0L8_2aparamS2073
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS2072 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS2074;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS2072, _M0L8_2aparamS2073);
  return 0;
}

void moonbit_init() {
  moonbit_layout_table = moonbit_layout_table_data;
}

int main(int argc, char** argv) {
  struct _M0TP19moonbitDB8Database* _M0L2dbS1955;
  struct _M0TP39moonbitDB8examples14shopping__cart7Product* _M0L6_2atmpS2218;
  struct _M0TP39moonbitDB8examples14shopping__cart7Product* _M0L6_2atmpS2219;
  struct _M0TP39moonbitDB8examples14shopping__cart7Product* _M0L6_2atmpS2220;
  struct _M0TP39moonbitDB8examples14shopping__cart7Product* _M0L6_2atmpS2221;
  struct _M0TP39moonbitDB8examples14shopping__cart7Product* _M0L6_2atmpS2222;
  struct _M0TP39moonbitDB8examples14shopping__cart7Product** _M0L6_2atmpS2217;
  struct _M0TPB5ArrayGRP39moonbitDB8examples14shopping__cart7ProductE* _M0L8productsS1956;
  int32_t _M0L7_2abindS1957;
  int32_t _M0L2__S1958;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1966;
  int32_t _M0L6_2atmpS2108;
  moonbit_string_t _M0L6_2atmpS2107;
  moonbit_string_t _M0L9cart__keyS1967;
  int32_t _M0L6_2atmpS2109;
  int32_t _M0L6_2atmpS2110;
  int32_t _M0L6_2atmpS2111;
  struct _M0TPB3MapGssE* _M0L11cart__itemsS1968;
  struct _M0TPB8MutLocalGfE* _M0L5totalS1969;
  struct _M0TPB4IterGUssEE* _M0L5_2aitS1970;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1987;
  float _M0L3valS2122;
  moonbit_string_t _M0L6_2atmpS2121;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1988;
  int32_t _M0L6_2atmpS2124;
  moonbit_string_t _M0L6_2atmpS2123;
  moonbit_string_t _M0L12history__keyS1989;
  moonbit_string_t* _M0L6_2atmpS2216;
  struct _M0TPB5ArrayGsE* _M0L6viewedS1990;
  int32_t _M0L7_2abindS1991;
  int32_t _M0L2__S1992;
  struct _M0TPB5ArrayGsE* _M0L6recentS1995;
  int32_t _M0L1iS1996;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2002;
  int32_t _M0L6_2atmpS2135;
  moonbit_string_t _M0L6_2atmpS2134;
  moonbit_string_t _M0L8fav__keyS2003;
  int32_t _M0L6_2atmpS2136;
  int32_t _M0L6_2atmpS2137;
  int32_t _M0L6_2atmpS2138;
  int32_t _M0L6_2atmpS2139;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2004;
  int32_t _M0L6_2atmpS2141;
  moonbit_string_t _M0L6_2atmpS2140;
  struct _M0TPB5ArrayGsE* _M0L4favsS2005;
  int32_t _M0L7_2abindS2006;
  int32_t _M0L2__S2007;
  moonbit_string_t _M0L10user2__favS2013;
  int32_t _M0L6_2atmpS2147;
  int32_t _M0L6_2atmpS2148;
  int32_t _M0L6_2atmpS2149;
  moonbit_string_t* _M0L6_2atmpS2215;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2214;
  struct _M0TPB5ArrayGsE* _M0L6commonS2014;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2015;
  int32_t _M0L6_2atmpS2151;
  moonbit_string_t _M0L6_2atmpS2150;
  int32_t _M0L7_2abindS2016;
  int32_t _M0L2__S2017;
  int32_t _M0L6_2atmpS2157;
  int32_t _M0L6_2atmpS2158;
  int32_t _M0L6_2atmpS2159;
  int32_t _M0L6_2atmpS2160;
  int32_t _M0L6_2atmpS2161;
  int32_t _M0L6_2atmpS2162;
  int32_t _M0L6_2atmpS2163;
  struct _M0TPB5ArrayGsE* _M0L8hardwareS2023;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2024;
  int32_t _M0L6_2atmpS2165;
  moonbit_string_t _M0L6_2atmpS2164;
  int32_t _M0L7_2abindS2025;
  int32_t _M0L2__S2026;
  moonbit_string_t* _M0L6_2atmpS2213;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2212;
  struct _M0TPB5ArrayGsE* _M0L15hw__and__periphS2032;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2033;
  int32_t _M0L6_2atmpS2172;
  moonbit_string_t _M0L6_2atmpS2171;
  moonbit_string_t* _M0L6_2atmpS2211;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2210;
  struct _M0TPB5ArrayGsE* _M0L9all__tagsS2034;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2035;
  int32_t _M0L6_2atmpS2174;
  moonbit_string_t _M0L6_2atmpS2173;
  moonbit_string_t _M0L10sales__keyS2036;
  int32_t _M0L6_2atmpS2175;
  int32_t _M0L6_2atmpS2176;
  int32_t _M0L6_2atmpS2177;
  int32_t _M0L6_2atmpS2178;
  int32_t _M0L6_2atmpS2179;
  struct _M0TPB5ArrayGsE* _M0L4top3S2037;
  int32_t _M0L1iS2038;
  struct _M0TPB5ArrayGsE* _M0L10mid__salesS2051;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2052;
  int32_t _M0L6_2atmpS2187;
  moonbit_string_t _M0L6_2atmpS2186;
  moonbit_string_t _M0L11session__idS2053;
  int32_t _M0L6_2atmpS2188;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2054;
  moonbit_string_t _M0L6_2atmpS2189;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2055;
  moonbit_string_t _M0L6_2atmpS2192;
  moonbit_string_t _M0L6_2atmpS2191;
  moonbit_string_t _M0L6_2atmpS2190;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2056;
  int32_t _M0L6_2atmpS2194;
  moonbit_string_t _M0L6_2atmpS2193;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2057;
  int32_t _M0L6_2atmpS2196;
  moonbit_string_t _M0L6_2atmpS2195;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2058;
  int32_t _M0L6_2atmpS2198;
  moonbit_string_t _M0L6_2atmpS2197;
  moonbit_string_t* _M0L6_2atmpS2209;
  struct _M0TPB5ArrayGsE* _M0L9hot__keysS2059;
  moonbit_string_t* _M0L6_2atmpS2208;
  struct _M0TPB5ArrayGsE* _M0L9hot__valsS2060;
  int32_t _M0L6_2atmpS2199;
  struct _M0TPB5ArrayGOsE* _M0L13batch__resultS2061;
  int32_t _M0L1iS2062;
  struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2071;
  int32_t _M0L6_2atmpS2207;
  moonbit_string_t _M0L6_2atmpS2206;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  #line 66 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L2dbS1955 = _M0MP19moonbitDB8Database3new();
  #line 68 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_111.data);
  #line 69 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_112.data);
  #line 70 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_111.data);
  #line 73 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2218
  = _M0MP39moonbitDB8examples14shopping__cart7Product3new((moonbit_string_t)moonbit_string_literal_113.data, (moonbit_string_t)moonbit_string_literal_114.data, 0x1.8cp+6f, 100);
  #line 74 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2219
  = _M0MP39moonbitDB8examples14shopping__cart7Product3new((moonbit_string_t)moonbit_string_literal_115.data, (moonbit_string_t)moonbit_string_literal_116.data, 0x1.76ep+11f, 50);
  #line 75 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2220
  = _M0MP39moonbitDB8examples14shopping__cart7Product3new((moonbit_string_t)moonbit_string_literal_117.data, (moonbit_string_t)moonbit_string_literal_118.data, 0x1.8fp+8f, 200);
  #line 76 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2221
  = _M0MP39moonbitDB8examples14shopping__cart7Product3new((moonbit_string_t)moonbit_string_literal_119.data, (moonbit_string_t)moonbit_string_literal_120.data, 0x1.f3cp+10f, 30);
  #line 77 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2222
  = _M0MP39moonbitDB8examples14shopping__cart7Product3new((moonbit_string_t)moonbit_string_literal_121.data, (moonbit_string_t)moonbit_string_literal_122.data, 0x1.02p+7f, 500);
  _M0L6_2atmpS2217
  = (struct _M0TP39moonbitDB8examples14shopping__cart7Product**)moonbit_make_ref_array_raw(5);
  _M0L6_2atmpS2217[0] = _M0L6_2atmpS2218;
  _M0L6_2atmpS2217[1] = _M0L6_2atmpS2219;
  _M0L6_2atmpS2217[2] = _M0L6_2atmpS2220;
  _M0L6_2atmpS2217[3] = _M0L6_2atmpS2221;
  _M0L6_2atmpS2217[4] = _M0L6_2atmpS2222;
  _M0L8productsS1956
  = (struct _M0TPB5ArrayGRP39moonbitDB8examples14shopping__cart7ProductE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP39moonbitDB8examples14shopping__cart7ProductE));
  Moonbit_object_header(_M0L8productsS1956)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 135, 0);
  _M0L8productsS1956->$0 = _M0L6_2atmpS2217;
  _M0L8productsS1956->$1 = 5;
  #line 80 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_123.data);
  #line 81 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_124.data);
  _M0L7_2abindS1957 = _M0L8productsS1956->$1;
  _M0L2__S1958 = 0;
  while (1) {
    if (_M0L2__S1958 < _M0L7_2abindS1957) {
      struct _M0TP39moonbitDB8examples14shopping__cart7Product** _M0L3bufS2106 =
        _M0L8productsS1956->$0;
      struct _M0TP39moonbitDB8examples14shopping__cart7Product* _M0L7productS1959 =
        (struct _M0TP39moonbitDB8examples14shopping__cart7Product*)_M0L3bufS2106[
          _M0L2__S1958
        ];
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1961;
      moonbit_string_t _M0L2idS2104;
      moonbit_string_t _M0L3keyS1960;
      moonbit_string_t _M0L4nameS2093;
      int32_t _M0L6_2atmpS2092;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1962;
      float _M0L5priceS2096;
      moonbit_string_t _M0L6_2atmpS2095;
      int32_t _M0L6_2atmpS2094;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1963;
      int32_t _M0L5stockS2099;
      moonbit_string_t _M0L6_2atmpS2098;
      int32_t _M0L6_2atmpS2097;
      int32_t _M0L6_2atmpS2100;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1964;
      moonbit_string_t _M0L4nameS2102;
      moonbit_string_t _M0L8_2afieldS4280;
      int32_t _M0L6_2acntS4367;
      moonbit_string_t _M0L2idS2103;
      moonbit_string_t _M0L6_2atmpS2101;
      int32_t _M0L6_2atmpS2105;
      moonbit_incref(_M0L7productS1959);
      #line 83 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1961
      = _M0MPB13StringBuilder21StringBuilder_2einner(8);
      #line 83 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1961, (moonbit_string_t)moonbit_string_literal_125.data);
      _M0L2idS2104 = _M0L7productS1959->$0;
      moonbit_incref(_M0L2idS2104);
      #line 83 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1961, _M0L2idS2104);
      moonbit_decref(_M0L2idS2104);
      #line 83 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L3keyS1960
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1961);
      moonbit_decref(_M0L18_2astring__builderS1961);
      _M0L4nameS2093 = _M0L7productS1959->$1;
      moonbit_incref(_M0L4nameS2093);
      #line 84 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2092
      = _M0MP19moonbitDB8Database4hset(_M0L2dbS1955, _M0L3keyS1960, (moonbit_string_t)moonbit_string_literal_126.data, _M0L4nameS2093);
      moonbit_decref(_M0L4nameS2093);
      #line 85 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1962
      = _M0MPB13StringBuilder21StringBuilder_2einner(0);
      _M0L5priceS2096 = _M0L7productS1959->$2;
      #line 85 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1962, _M0L5priceS2096);
      #line 85 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2095
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1962);
      moonbit_decref(_M0L18_2astring__builderS1962);
      #line 85 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2094
      = _M0MP19moonbitDB8Database4hset(_M0L2dbS1955, _M0L3keyS1960, (moonbit_string_t)moonbit_string_literal_127.data, _M0L6_2atmpS2095);
      moonbit_decref(_M0L6_2atmpS2095);
      #line 86 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1963
      = _M0MPB13StringBuilder21StringBuilder_2einner(0);
      _M0L5stockS2099 = _M0L7productS1959->$3;
      #line 86 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1963, _M0L5stockS2099);
      #line 86 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2098
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1963);
      moonbit_decref(_M0L18_2astring__builderS1963);
      #line 86 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2097
      = _M0MP19moonbitDB8Database4hset(_M0L2dbS1955, _M0L3keyS1960, (moonbit_string_t)moonbit_string_literal_128.data, _M0L6_2atmpS2098);
      moonbit_decref(_M0L6_2atmpS2098);
      #line 87 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2100
      = _M0MP19moonbitDB8Database6expire(_M0L2dbS1955, _M0L3keyS1960, 3600);
      moonbit_decref(_M0L3keyS1960);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1964
      = _M0MPB13StringBuilder21StringBuilder_2einner(35);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1964, (moonbit_string_t)moonbit_string_literal_129.data);
      _M0L4nameS2102 = _M0L7productS1959->$1;
      moonbit_incref(_M0L4nameS2102);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1964, _M0L4nameS2102);
      moonbit_decref(_M0L4nameS2102);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1964, (moonbit_string_t)moonbit_string_literal_130.data);
      _M0L8_2afieldS4280 = _M0L7productS1959->$0;
      _M0L6_2acntS4367
      = Moonbit_rc_count(Moonbit_object_header(_M0L7productS1959));
      if (_M0L6_2acntS4367 > 1) {
        int32_t _M0L11_2anew__cntS4369 = _M0L6_2acntS4367 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L7productS1959), _M0L11_2anew__cntS4369);
        moonbit_incref(_M0L8_2afieldS4280);
      } else if (_M0L6_2acntS4367 == 1) {
        moonbit_string_t _M0L8_2afieldS4368 = _M0L7productS1959->$1;
        moonbit_decref(_M0L8_2afieldS4368);
        #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        moonbit_free(_M0L7productS1959);
      }
      _M0L2idS2103 = _M0L8_2afieldS4280;
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1964, _M0L2idS2103);
      moonbit_decref(_M0L2idS2103);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1964, (moonbit_string_t)moonbit_string_literal_131.data);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2101
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1964);
      moonbit_decref(_M0L18_2astring__builderS1964);
      #line 88 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2101);
      moonbit_decref(_M0L6_2atmpS2101);
      _M0L6_2atmpS2105 = _M0L2__S1958 + 1;
      _M0L2__S1958 = _M0L6_2atmpS2105;
      continue;
    } else {
      moonbit_decref(_M0L8productsS1956);
    }
    break;
  }
  #line 90 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1966
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 90 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1966, (moonbit_string_t)moonbit_string_literal_132.data);
  #line 90 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2108 = _M0MP19moonbitDB8Database6dbsize(_M0L2dbS1955);
  #line 90 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1966, _M0L6_2atmpS2108);
  #line 90 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2107
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1966);
  moonbit_decref(_M0L18_2astring__builderS1966);
  #line 90 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2107);
  moonbit_decref(_M0L6_2atmpS2107);
  #line 92 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_133.data);
  #line 93 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_124.data);
  _M0L9cart__keyS1967 = (moonbit_string_t)moonbit_string_literal_134.data;
  #line 95 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2109
  = _M0MP19moonbitDB8Database4hset(_M0L2dbS1955, _M0L9cart__keyS1967, (moonbit_string_t)moonbit_string_literal_113.data, (moonbit_string_t)moonbit_string_literal_87.data);
  #line 96 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2110
  = _M0MP19moonbitDB8Database4hset(_M0L2dbS1955, _M0L9cart__keyS1967, (moonbit_string_t)moonbit_string_literal_117.data, (moonbit_string_t)moonbit_string_literal_86.data);
  #line 97 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2111
  = _M0MP19moonbitDB8Database4hset(_M0L2dbS1955, _M0L9cart__keyS1967, (moonbit_string_t)moonbit_string_literal_121.data, (moonbit_string_t)moonbit_string_literal_88.data);
  #line 98 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_135.data);
  #line 99 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L11cart__itemsS1968
  = _M0MP19moonbitDB8Database7hgetall(_M0L2dbS1955, _M0L9cart__keyS1967);
  _M0L5totalS1969
  = (struct _M0TPB8MutLocalGfE*)moonbit_malloc(sizeof(struct _M0TPB8MutLocalGfE));
  Moonbit_object_header(_M0L5totalS1969)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_SCALAR, 0, 0);
  _M0L5totalS1969->$0 = 0x0p+0f;
  #line 100 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L5_2aitS1970 = _M0MPB3Map5iter2GssE(_M0L11cart__itemsS1968);
  moonbit_decref(_M0L11cart__itemsS1968);
  while (1) {
    moonbit_string_t _M0L3pidS1972;
    moonbit_string_t _M0L8qty__strS1973;
    struct _M0TUssE* _M0L7_2abindS1982;
    int32_t _M0L3qtyS1974;
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1976;
    moonbit_string_t _M0L6_2atmpS2120;
    moonbit_string_t _M0L6_2atmpS2119;
    moonbit_string_t _M0L6_2atmpS2118;
    float _M0L5priceS1975;
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1978;
    moonbit_string_t _M0L6_2atmpS2117;
    moonbit_string_t _M0L6_2atmpS2116;
    moonbit_string_t _M0L4nameS1977;
    float _M0L6_2atmpS2115;
    float _M0L8subtotalS1979;
    float _M0L3valS2113;
    float _M0L6_2atmpS2112;
    struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1980;
    moonbit_string_t _M0L6_2atmpS2114;
    #line 101 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L7_2abindS1982 = _M0MPB5Iter24nextGssE(_M0L5_2aitS1970);
    if (_M0L7_2abindS1982 == 0) {
      if (_M0L7_2abindS1982) {
        moonbit_decref(_M0L7_2abindS1982);
      }
      moonbit_decref(_M0L5_2aitS1970);
    } else {
      struct _M0TUssE* _M0L7_2aSomeS1983 = _M0L7_2abindS1982;
      struct _M0TUssE* _M0L4_2axS1984 = _M0L7_2aSomeS1983;
      moonbit_string_t _M0L6_2apidS1985 = _M0L4_2axS1984->$0;
      moonbit_string_t _M0L8_2afieldS4278 = _M0L4_2axS1984->$1;
      int32_t _M0L6_2acntS4370 =
        Moonbit_rc_count(Moonbit_object_header(_M0L4_2axS1984));
      moonbit_string_t _M0L11_2aqty__strS1986;
      if (_M0L6_2acntS4370 > 1) {
        int32_t _M0L11_2anew__cntS4371 = _M0L6_2acntS4370 - 1;
        Moonbit_set_rc_count(Moonbit_object_header(_M0L4_2axS1984), _M0L11_2anew__cntS4371);
        moonbit_incref(_M0L8_2afieldS4278);
        moonbit_incref(_M0L6_2apidS1985);
      } else if (_M0L6_2acntS4370 == 1) {
        #line 101 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        moonbit_free(_M0L4_2axS1984);
      }
      _M0L11_2aqty__strS1986 = _M0L8_2afieldS4278;
      _M0L3pidS1972 = _M0L6_2apidS1985;
      _M0L8qty__strS1973 = _M0L11_2aqty__strS1986;
      goto join_1971;
    }
    goto joinlet_4687;
    join_1971:;
    #line 102 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L3qtyS1974
    = _M0FP39moonbitDB8examples14shopping__cart10parse__int(_M0L8qty__strS1973);
    moonbit_decref(_M0L8qty__strS1973);
    #line 103 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L18_2astring__builderS1976
    = _M0MPB13StringBuilder21StringBuilder_2einner(8);
    #line 103 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1976, (moonbit_string_t)moonbit_string_literal_125.data);
    #line 103 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1976, _M0L3pidS1972);
    #line 103 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS2120
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1976);
    moonbit_decref(_M0L18_2astring__builderS1976);
    #line 103 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS2119
    = _M0MP19moonbitDB8Database4hget(_M0L2dbS1955, _M0L6_2atmpS2120, (moonbit_string_t)moonbit_string_literal_127.data);
    moonbit_decref(_M0L6_2atmpS2120);
    #line 103 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS2118
    = _M0FP39moonbitDB8examples14shopping__cart9show__opt(_M0L6_2atmpS2119);
    if (_M0L6_2atmpS2119) {
      moonbit_decref(_M0L6_2atmpS2119);
    }
    #line 103 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L5priceS1975
    = _M0FP39moonbitDB8examples14shopping__cart12parse__float(_M0L6_2atmpS2118);
    moonbit_decref(_M0L6_2atmpS2118);
    #line 104 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L18_2astring__builderS1978
    = _M0MPB13StringBuilder21StringBuilder_2einner(8);
    #line 104 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1978, (moonbit_string_t)moonbit_string_literal_125.data);
    #line 104 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1978, _M0L3pidS1972);
    moonbit_decref(_M0L3pidS1972);
    #line 104 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS2117
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1978);
    moonbit_decref(_M0L18_2astring__builderS1978);
    #line 104 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS2116
    = _M0MP19moonbitDB8Database4hget(_M0L2dbS1955, _M0L6_2atmpS2117, (moonbit_string_t)moonbit_string_literal_126.data);
    moonbit_decref(_M0L6_2atmpS2117);
    #line 104 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L4nameS1977
    = _M0FP39moonbitDB8examples14shopping__cart9show__opt(_M0L6_2atmpS2116);
    if (_M0L6_2atmpS2116) {
      moonbit_decref(_M0L6_2atmpS2116);
    }
    _M0L6_2atmpS2115 = (float)_M0L3qtyS1974;
    _M0L8subtotalS1979 = _M0L5priceS1975 * _M0L6_2atmpS2115;
    _M0L3valS2113 = _M0L5totalS1969->$0;
    _M0L6_2atmpS2112 = _M0L3valS2113 + _M0L8subtotalS1979;
    _M0L5totalS1969->$0 = _M0L6_2atmpS2112;
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L18_2astring__builderS1980
    = _M0MPB13StringBuilder21StringBuilder_2einner(12);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1980, (moonbit_string_t)moonbit_string_literal_136.data);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1980, _M0L4nameS1977);
    moonbit_decref(_M0L4nameS1977);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1980, (moonbit_string_t)moonbit_string_literal_137.data);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1980, _M0L3qtyS1974);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1980, (moonbit_string_t)moonbit_string_literal_138.data);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1980, _M0L8subtotalS1979);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS2114
    = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1980);
    moonbit_decref(_M0L18_2astring__builderS1980);
    #line 107 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0FPB7printlnGsE(_M0L6_2atmpS2114);
    moonbit_decref(_M0L6_2atmpS2114);
    continue;
    joinlet_4687:;
    break;
  }
  #line 109 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1987
  = _M0MPB13StringBuilder21StringBuilder_2einner(21);
  #line 109 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1987, (moonbit_string_t)moonbit_string_literal_139.data);
  _M0L3valS2122 = _M0L5totalS1969->$0;
  moonbit_decref(_M0L5totalS1969);
  #line 109 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGfE(_M0L18_2astring__builderS1987, _M0L3valS2122);
  #line 109 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2121
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1987);
  moonbit_decref(_M0L18_2astring__builderS1987);
  #line 109 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2121);
  moonbit_decref(_M0L6_2atmpS2121);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS1988
  = _M0MPB13StringBuilder21StringBuilder_2einner(16);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1988, (moonbit_string_t)moonbit_string_literal_140.data);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2124
  = _M0MP19moonbitDB8Database4hlen(_M0L2dbS1955, _M0L9cart__keyS1967);
  moonbit_decref(_M0L9cart__keyS1967);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS1988, _M0L6_2atmpS2124);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2123
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1988);
  moonbit_decref(_M0L18_2astring__builderS1988);
  #line 110 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2123);
  moonbit_decref(_M0L6_2atmpS2123);
  #line 112 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_141.data);
  #line 113 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_124.data);
  _M0L12history__keyS1989 = (moonbit_string_t)moonbit_string_literal_142.data;
  _M0L6_2atmpS2216 = (moonbit_string_t*)moonbit_make_ref_array_raw(7);
  _M0L6_2atmpS2216[0] = (moonbit_string_t)moonbit_string_literal_115.data;
  _M0L6_2atmpS2216[1] = (moonbit_string_t)moonbit_string_literal_113.data;
  _M0L6_2atmpS2216[2] = (moonbit_string_t)moonbit_string_literal_119.data;
  _M0L6_2atmpS2216[3] = (moonbit_string_t)moonbit_string_literal_117.data;
  _M0L6_2atmpS2216[4] = (moonbit_string_t)moonbit_string_literal_121.data;
  _M0L6_2atmpS2216[5] = (moonbit_string_t)moonbit_string_literal_113.data;
  _M0L6_2atmpS2216[6] = (moonbit_string_t)moonbit_string_literal_115.data;
  _M0L6viewedS1990
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6viewedS1990)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6viewedS1990->$0 = _M0L6_2atmpS2216;
  _M0L6viewedS1990->$1 = 7;
  _M0L7_2abindS1991 = _M0L6viewedS1990->$1;
  _M0L2__S1992 = 0;
  while (1) {
    if (_M0L2__S1992 < _M0L7_2abindS1991) {
      moonbit_string_t* _M0L3bufS2127 = _M0L6viewedS1990->$0;
      moonbit_string_t _M0L3pidS1993 =
        (moonbit_string_t)_M0L3bufS2127[_M0L2__S1992];
      int32_t _M0L6_2atmpS2125;
      int32_t _M0L6_2atmpS2126;
      moonbit_incref(_M0L3pidS1993);
      #line 117 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2125
      = _M0MP19moonbitDB8Database5lpush(_M0L2dbS1955, _M0L12history__keyS1989, _M0L3pidS1993);
      moonbit_decref(_M0L3pidS1993);
      _M0L6_2atmpS2126 = _M0L2__S1992 + 1;
      _M0L2__S1992 = _M0L6_2atmpS2126;
      continue;
    } else {
      moonbit_decref(_M0L6viewedS1990);
    }
    break;
  }
  #line 119 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_143.data);
  #line 120 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6recentS1995
  = _M0MP19moonbitDB8Database6lrange(_M0L2dbS1955, _M0L12history__keyS1989, 0, 4);
  _M0L1iS1996 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2128;
    #line 121 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS2128 = _M0MPC15array5Array6lengthGsE(_M0L6recentS1995);
    if (_M0L1iS1996 < _M0L6_2atmpS2128) {
      moonbit_string_t _M0L3pidS1997;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS1999;
      moonbit_string_t _M0L6_2atmpS2132;
      moonbit_string_t _M0L6_2atmpS2131;
      moonbit_string_t _M0L4nameS1998;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2000;
      int32_t _M0L6_2atmpS2130;
      moonbit_string_t _M0L6_2atmpS2129;
      int32_t _M0L6_2atmpS2133;
      #line 122 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L3pidS1997
      = _M0MPC15array5Array2atGsE(_M0L6recentS1995, _M0L1iS1996);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS1999
      = _M0MPB13StringBuilder21StringBuilder_2einner(8);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS1999, (moonbit_string_t)moonbit_string_literal_125.data);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS1999, _M0L3pidS1997);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2132
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS1999);
      moonbit_decref(_M0L18_2astring__builderS1999);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2131
      = _M0MP19moonbitDB8Database4hget(_M0L2dbS1955, _M0L6_2atmpS2132, (moonbit_string_t)moonbit_string_literal_126.data);
      moonbit_decref(_M0L6_2atmpS2132);
      #line 123 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L4nameS1998
      = _M0FP39moonbitDB8examples14shopping__cart9show__opt(_M0L6_2atmpS2131);
      if (_M0L6_2atmpS2131) {
        moonbit_decref(_M0L6_2atmpS2131);
      }
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS2000
      = _M0MPB13StringBuilder21StringBuilder_2einner(9);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2000, (moonbit_string_t)moonbit_string_literal_136.data);
      _M0L6_2atmpS2130 = _M0L1iS1996 + 1;
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2000, _M0L6_2atmpS2130);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2000, (moonbit_string_t)moonbit_string_literal_144.data);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2000, _M0L4nameS1998);
      moonbit_decref(_M0L4nameS1998);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2000, (moonbit_string_t)moonbit_string_literal_145.data);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2000, _M0L3pidS1997);
      moonbit_decref(_M0L3pidS1997);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2000, (moonbit_string_t)moonbit_string_literal_146.data);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2129
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2000);
      moonbit_decref(_M0L18_2astring__builderS2000);
      #line 124 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2129);
      moonbit_decref(_M0L6_2atmpS2129);
      _M0L6_2atmpS2133 = _M0L1iS1996 + 1;
      _M0L1iS1996 = _M0L6_2atmpS2133;
      continue;
    } else {
      moonbit_decref(_M0L6recentS1995);
    }
    break;
  }
  #line 126 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS2002
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 126 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2002, (moonbit_string_t)moonbit_string_literal_147.data);
  #line 126 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2135
  = _M0MP19moonbitDB8Database4llen(_M0L2dbS1955, _M0L12history__keyS1989);
  moonbit_decref(_M0L12history__keyS1989);
  #line 126 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2002, _M0L6_2atmpS2135);
  #line 126 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2134
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2002);
  moonbit_decref(_M0L18_2astring__builderS2002);
  #line 126 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2134);
  moonbit_decref(_M0L6_2atmpS2134);
  #line 128 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_148.data);
  #line 129 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_124.data);
  _M0L8fav__keyS2003 = (moonbit_string_t)moonbit_string_literal_149.data;
  #line 131 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2136
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1955, _M0L8fav__keyS2003, (moonbit_string_t)moonbit_string_literal_113.data);
  #line 132 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2137
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1955, _M0L8fav__keyS2003, (moonbit_string_t)moonbit_string_literal_115.data);
  #line 133 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2138
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1955, _M0L8fav__keyS2003, (moonbit_string_t)moonbit_string_literal_119.data);
  #line 134 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2139
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1955, _M0L8fav__keyS2003, (moonbit_string_t)moonbit_string_literal_113.data);
  #line 135 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS2004
  = _M0MPB13StringBuilder21StringBuilder_2einner(19);
  #line 135 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2004, (moonbit_string_t)moonbit_string_literal_150.data);
  #line 135 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2141
  = _M0MP19moonbitDB8Database5scard(_M0L2dbS1955, _M0L8fav__keyS2003);
  #line 135 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2004, _M0L6_2atmpS2141);
  #line 135 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2140
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2004);
  moonbit_decref(_M0L18_2astring__builderS2004);
  #line 135 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2140);
  moonbit_decref(_M0L6_2atmpS2140);
  #line 136 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L4favsS2005
  = _M0MP19moonbitDB8Database8smembers(_M0L2dbS1955, _M0L8fav__keyS2003);
  _M0L7_2abindS2006 = _M0L4favsS2005->$1;
  _M0L2__S2007 = 0;
  while (1) {
    if (_M0L2__S2007 < _M0L7_2abindS2006) {
      moonbit_string_t* _M0L3bufS2146 = _M0L4favsS2005->$0;
      moonbit_string_t _M0L3pidS2008 =
        (moonbit_string_t)_M0L3bufS2146[_M0L2__S2007];
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2010;
      moonbit_string_t _M0L6_2atmpS2144;
      moonbit_string_t _M0L6_2atmpS2143;
      moonbit_string_t _M0L4nameS2009;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2011;
      moonbit_string_t _M0L6_2atmpS2142;
      int32_t _M0L6_2atmpS2145;
      moonbit_incref(_M0L3pidS2008);
      #line 138 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS2010
      = _M0MPB13StringBuilder21StringBuilder_2einner(8);
      #line 138 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2010, (moonbit_string_t)moonbit_string_literal_125.data);
      #line 138 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2010, _M0L3pidS2008);
      moonbit_decref(_M0L3pidS2008);
      #line 138 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2144
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2010);
      moonbit_decref(_M0L18_2astring__builderS2010);
      #line 138 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2143
      = _M0MP19moonbitDB8Database4hget(_M0L2dbS1955, _M0L6_2atmpS2144, (moonbit_string_t)moonbit_string_literal_126.data);
      moonbit_decref(_M0L6_2atmpS2144);
      #line 138 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L4nameS2009
      = _M0FP39moonbitDB8examples14shopping__cart9show__opt(_M0L6_2atmpS2143);
      if (_M0L6_2atmpS2143) {
        moonbit_decref(_M0L6_2atmpS2143);
      }
      #line 139 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS2011
      = _M0MPB13StringBuilder21StringBuilder_2einner(6);
      #line 139 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2011, (moonbit_string_t)moonbit_string_literal_151.data);
      #line 139 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2011, _M0L4nameS2009);
      moonbit_decref(_M0L4nameS2009);
      #line 139 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2142
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2011);
      moonbit_decref(_M0L18_2astring__builderS2011);
      #line 139 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2142);
      moonbit_decref(_M0L6_2atmpS2142);
      _M0L6_2atmpS2145 = _M0L2__S2007 + 1;
      _M0L2__S2007 = _M0L6_2atmpS2145;
      continue;
    } else {
      moonbit_decref(_M0L4favsS2005);
    }
    break;
  }
  #line 142 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_152.data);
  #line 143 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_124.data);
  _M0L10user2__favS2013 = (moonbit_string_t)moonbit_string_literal_153.data;
  #line 145 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2147
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1955, _M0L10user2__favS2013, (moonbit_string_t)moonbit_string_literal_113.data);
  #line 146 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2148
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1955, _M0L10user2__favS2013, (moonbit_string_t)moonbit_string_literal_117.data);
  #line 147 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2149
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1955, _M0L10user2__favS2013, (moonbit_string_t)moonbit_string_literal_121.data);
  _M0L6_2atmpS2215 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS2215[0] = _M0L8fav__keyS2003;
  _M0L6_2atmpS2215[1] = _M0L10user2__favS2013;
  _M0L6_2atmpS2214
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2214)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS2214->$0 = _M0L6_2atmpS2215;
  _M0L6_2atmpS2214->$1 = 2;
  #line 148 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6commonS2014
  = _M0MP19moonbitDB8Database6sinter(_M0L2dbS1955, _M0L6_2atmpS2214);
  moonbit_decref(_M0L6_2atmpS2214);
  #line 149 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS2015
  = _M0MPB13StringBuilder21StringBuilder_2einner(32);
  #line 149 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2015, (moonbit_string_t)moonbit_string_literal_154.data);
  #line 149 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2151 = _M0MPC15array5Array6lengthGsE(_M0L6commonS2014);
  #line 149 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2015, _M0L6_2atmpS2151);
  #line 149 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2015, (moonbit_string_t)moonbit_string_literal_155.data);
  #line 149 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2150
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2015);
  moonbit_decref(_M0L18_2astring__builderS2015);
  #line 149 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2150);
  moonbit_decref(_M0L6_2atmpS2150);
  _M0L7_2abindS2016 = _M0L6commonS2014->$1;
  _M0L2__S2017 = 0;
  while (1) {
    if (_M0L2__S2017 < _M0L7_2abindS2016) {
      moonbit_string_t* _M0L3bufS2156 = _M0L6commonS2014->$0;
      moonbit_string_t _M0L3pidS2018 =
        (moonbit_string_t)_M0L3bufS2156[_M0L2__S2017];
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2020;
      moonbit_string_t _M0L6_2atmpS2154;
      moonbit_string_t _M0L6_2atmpS2153;
      moonbit_string_t _M0L4nameS2019;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2021;
      moonbit_string_t _M0L6_2atmpS2152;
      int32_t _M0L6_2atmpS2155;
      moonbit_incref(_M0L3pidS2018);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS2020
      = _M0MPB13StringBuilder21StringBuilder_2einner(8);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2020, (moonbit_string_t)moonbit_string_literal_125.data);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2020, _M0L3pidS2018);
      moonbit_decref(_M0L3pidS2018);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2154
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2020);
      moonbit_decref(_M0L18_2astring__builderS2020);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2153
      = _M0MP19moonbitDB8Database4hget(_M0L2dbS1955, _M0L6_2atmpS2154, (moonbit_string_t)moonbit_string_literal_126.data);
      moonbit_decref(_M0L6_2atmpS2154);
      #line 151 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L4nameS2019
      = _M0FP39moonbitDB8examples14shopping__cart9show__opt(_M0L6_2atmpS2153);
      if (_M0L6_2atmpS2153) {
        moonbit_decref(_M0L6_2atmpS2153);
      }
      #line 152 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS2021
      = _M0MPB13StringBuilder21StringBuilder_2einner(6);
      #line 152 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2021, (moonbit_string_t)moonbit_string_literal_151.data);
      #line 152 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2021, _M0L4nameS2019);
      moonbit_decref(_M0L4nameS2019);
      #line 152 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2152
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2021);
      moonbit_decref(_M0L18_2astring__builderS2021);
      #line 152 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2152);
      moonbit_decref(_M0L6_2atmpS2152);
      _M0L6_2atmpS2155 = _M0L2__S2017 + 1;
      _M0L2__S2017 = _M0L6_2atmpS2155;
      continue;
    } else {
      moonbit_decref(_M0L6commonS2014);
    }
    break;
  }
  #line 155 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_156.data);
  #line 156 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_124.data);
  #line 157 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2157
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1955, (moonbit_string_t)moonbit_string_literal_157.data, (moonbit_string_t)moonbit_string_literal_113.data);
  #line 158 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2158
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1955, (moonbit_string_t)moonbit_string_literal_158.data, (moonbit_string_t)moonbit_string_literal_115.data);
  #line 159 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2159
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1955, (moonbit_string_t)moonbit_string_literal_158.data, (moonbit_string_t)moonbit_string_literal_117.data);
  #line 160 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2160
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1955, (moonbit_string_t)moonbit_string_literal_158.data, (moonbit_string_t)moonbit_string_literal_119.data);
  #line 161 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2161
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1955, (moonbit_string_t)moonbit_string_literal_159.data, (moonbit_string_t)moonbit_string_literal_117.data);
  #line 162 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2162
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1955, (moonbit_string_t)moonbit_string_literal_159.data, (moonbit_string_t)moonbit_string_literal_121.data);
  #line 163 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2163
  = _M0MP19moonbitDB8Database4sadd(_M0L2dbS1955, (moonbit_string_t)moonbit_string_literal_160.data, (moonbit_string_t)moonbit_string_literal_119.data);
  #line 164 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L8hardwareS2023
  = _M0MP19moonbitDB8Database8smembers(_M0L2dbS1955, (moonbit_string_t)moonbit_string_literal_158.data);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS2024
  = _M0MPB13StringBuilder21StringBuilder_2einner(27);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2024, (moonbit_string_t)moonbit_string_literal_161.data);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2165 = _M0MPC15array5Array6lengthGsE(_M0L8hardwareS2023);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2024, _M0L6_2atmpS2165);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2164
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2024);
  moonbit_decref(_M0L18_2astring__builderS2024);
  #line 165 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2164);
  moonbit_decref(_M0L6_2atmpS2164);
  _M0L7_2abindS2025 = _M0L8hardwareS2023->$1;
  _M0L2__S2026 = 0;
  while (1) {
    if (_M0L2__S2026 < _M0L7_2abindS2025) {
      moonbit_string_t* _M0L3bufS2170 = _M0L8hardwareS2023->$0;
      moonbit_string_t _M0L3pidS2027 =
        (moonbit_string_t)_M0L3bufS2170[_M0L2__S2026];
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2029;
      moonbit_string_t _M0L6_2atmpS2168;
      moonbit_string_t _M0L6_2atmpS2167;
      moonbit_string_t _M0L4nameS2028;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2030;
      moonbit_string_t _M0L6_2atmpS2166;
      int32_t _M0L6_2atmpS2169;
      moonbit_incref(_M0L3pidS2027);
      #line 167 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS2029
      = _M0MPB13StringBuilder21StringBuilder_2einner(8);
      #line 167 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2029, (moonbit_string_t)moonbit_string_literal_125.data);
      #line 167 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2029, _M0L3pidS2027);
      moonbit_decref(_M0L3pidS2027);
      #line 167 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2168
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2029);
      moonbit_decref(_M0L18_2astring__builderS2029);
      #line 167 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2167
      = _M0MP19moonbitDB8Database4hget(_M0L2dbS1955, _M0L6_2atmpS2168, (moonbit_string_t)moonbit_string_literal_126.data);
      moonbit_decref(_M0L6_2atmpS2168);
      #line 167 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L4nameS2028
      = _M0FP39moonbitDB8examples14shopping__cart9show__opt(_M0L6_2atmpS2167);
      if (_M0L6_2atmpS2167) {
        moonbit_decref(_M0L6_2atmpS2167);
      }
      #line 168 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS2030
      = _M0MPB13StringBuilder21StringBuilder_2einner(6);
      #line 168 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2030, (moonbit_string_t)moonbit_string_literal_151.data);
      #line 168 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2030, _M0L4nameS2028);
      moonbit_decref(_M0L4nameS2028);
      #line 168 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2166
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2030);
      moonbit_decref(_M0L18_2astring__builderS2030);
      #line 168 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2166);
      moonbit_decref(_M0L6_2atmpS2166);
      _M0L6_2atmpS2169 = _M0L2__S2026 + 1;
      _M0L2__S2026 = _M0L6_2atmpS2169;
      continue;
    } else {
      moonbit_decref(_M0L8hardwareS2023);
    }
    break;
  }
  _M0L6_2atmpS2213 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS2213[0] = (moonbit_string_t)moonbit_string_literal_158.data;
  _M0L6_2atmpS2213[1] = (moonbit_string_t)moonbit_string_literal_159.data;
  _M0L6_2atmpS2212
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2212)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS2212->$0 = _M0L6_2atmpS2213;
  _M0L6_2atmpS2212->$1 = 2;
  #line 170 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L15hw__and__periphS2032
  = _M0MP19moonbitDB8Database6sinter(_M0L2dbS1955, _M0L6_2atmpS2212);
  moonbit_decref(_M0L6_2atmpS2212);
  #line 171 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS2033
  = _M0MPB13StringBuilder21StringBuilder_2einner(36);
  #line 171 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2033, (moonbit_string_t)moonbit_string_literal_162.data);
  #line 171 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2172
  = _M0MPC15array5Array6lengthGsE(_M0L15hw__and__periphS2032);
  moonbit_decref(_M0L15hw__and__periphS2032);
  #line 171 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2033, _M0L6_2atmpS2172);
  #line 171 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2033, (moonbit_string_t)moonbit_string_literal_155.data);
  #line 171 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2171
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2033);
  moonbit_decref(_M0L18_2astring__builderS2033);
  #line 171 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2171);
  moonbit_decref(_M0L6_2atmpS2171);
  _M0L6_2atmpS2211 = (moonbit_string_t*)moonbit_make_ref_array_raw(3);
  _M0L6_2atmpS2211[0] = (moonbit_string_t)moonbit_string_literal_158.data;
  _M0L6_2atmpS2211[1] = (moonbit_string_t)moonbit_string_literal_157.data;
  _M0L6_2atmpS2211[2] = (moonbit_string_t)moonbit_string_literal_159.data;
  _M0L6_2atmpS2210
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2210)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L6_2atmpS2210->$0 = _M0L6_2atmpS2211;
  _M0L6_2atmpS2210->$1 = 3;
  #line 172 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L9all__tagsS2034
  = _M0MP19moonbitDB8Database6sunion(_M0L2dbS1955, _M0L6_2atmpS2210);
  moonbit_decref(_M0L6_2atmpS2210);
  #line 173 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS2035
  = _M0MPB13StringBuilder21StringBuilder_2einner(34);
  #line 173 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2035, (moonbit_string_t)moonbit_string_literal_163.data);
  #line 173 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2174 = _M0MPC15array5Array6lengthGsE(_M0L9all__tagsS2034);
  moonbit_decref(_M0L9all__tagsS2034);
  #line 173 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2035, _M0L6_2atmpS2174);
  #line 173 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2035, (moonbit_string_t)moonbit_string_literal_155.data);
  #line 173 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2173
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2035);
  moonbit_decref(_M0L18_2astring__builderS2035);
  #line 173 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2173);
  moonbit_decref(_M0L6_2atmpS2173);
  #line 175 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_164.data);
  #line 176 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_124.data);
  _M0L10sales__keyS2036 = (moonbit_string_t)moonbit_string_literal_165.data;
  #line 178 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2175
  = _M0MP19moonbitDB8Database4zadd(_M0L2dbS1955, _M0L10sales__keyS2036, 0x1.2cp+7f, (moonbit_string_t)moonbit_string_literal_113.data);
  #line 179 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2176
  = _M0MP19moonbitDB8Database4zadd(_M0L2dbS1955, _M0L10sales__keyS2036, 0x1.4p+6f, (moonbit_string_t)moonbit_string_literal_115.data);
  #line 180 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2177
  = _M0MP19moonbitDB8Database4zadd(_M0L2dbS1955, _M0L10sales__keyS2036, 0x1.4p+8f, (moonbit_string_t)moonbit_string_literal_117.data);
  #line 181 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2178
  = _M0MP19moonbitDB8Database4zadd(_M0L2dbS1955, _M0L10sales__keyS2036, 0x1.68p+5f, (moonbit_string_t)moonbit_string_literal_119.data);
  #line 182 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2179
  = _M0MP19moonbitDB8Database4zadd(_M0L2dbS1955, _M0L10sales__keyS2036, 0x1.f4p+8f, (moonbit_string_t)moonbit_string_literal_121.data);
  #line 183 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_166.data);
  #line 184 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L4top3S2037
  = _M0MP19moonbitDB8Database9zrevrange(_M0L2dbS1955, _M0L10sales__keyS2036, 0, 2);
  _M0L1iS2038 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2180;
    #line 185 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS2180 = _M0MPC15array5Array6lengthGsE(_M0L4top3S2037);
    if (_M0L1iS2038 < _M0L6_2atmpS2180) {
      moonbit_string_t _M0L3pidS2039;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2041;
      moonbit_string_t _M0L6_2atmpS2184;
      moonbit_string_t _M0L6_2atmpS2183;
      moonbit_string_t _M0L4nameS2040;
      void* _M0L6_2atmpS2182;
      moonbit_string_t _M0L5salesS2042;
      int32_t _M0L1rS2045;
      int32_t _M0L4rankS2043;
      int64_t _M0L7_2abindS2046;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2049;
      moonbit_string_t _M0L6_2atmpS2181;
      int32_t _M0L6_2atmpS2185;
      #line 186 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L3pidS2039 = _M0MPC15array5Array2atGsE(_M0L4top3S2037, _M0L1iS2038);
      #line 187 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS2041
      = _M0MPB13StringBuilder21StringBuilder_2einner(8);
      #line 187 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2041, (moonbit_string_t)moonbit_string_literal_125.data);
      #line 187 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2041, _M0L3pidS2039);
      #line 187 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2184
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2041);
      moonbit_decref(_M0L18_2astring__builderS2041);
      #line 187 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2183
      = _M0MP19moonbitDB8Database4hget(_M0L2dbS1955, _M0L6_2atmpS2184, (moonbit_string_t)moonbit_string_literal_126.data);
      moonbit_decref(_M0L6_2atmpS2184);
      #line 187 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L4nameS2040
      = _M0FP39moonbitDB8examples14shopping__cart9show__opt(_M0L6_2atmpS2183);
      if (_M0L6_2atmpS2183) {
        moonbit_decref(_M0L6_2atmpS2183);
      }
      #line 188 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2182
      = _M0MP19moonbitDB8Database6zscore(_M0L2dbS1955, _M0L10sales__keyS2036, _M0L3pidS2039);
      #line 188 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L5salesS2042
      = _M0FP39moonbitDB8examples14shopping__cart16show__opt__float(_M0L6_2atmpS2182);
      moonbit_decref(_M0L6_2atmpS2182);
      #line 189 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L7_2abindS2046
      = _M0MP19moonbitDB8Database8zrevrank(_M0L2dbS1955, _M0L10sales__keyS2036, _M0L3pidS2039);
      moonbit_decref(_M0L3pidS2039);
      if (_M0L7_2abindS2046 == 4294967296ll) {
        _M0L4rankS2043 = 0;
      } else {
        int64_t _M0L7_2aSomeS2047 = _M0L7_2abindS2046;
        int32_t _M0L4_2arS2048 = (int32_t)_M0L7_2aSomeS2047;
        _M0L1rS2045 = _M0L4_2arS2048;
        goto join_2044;
      }
      goto joinlet_4694;
      join_2044:;
      _M0L4rankS2043 = _M0L1rS2045 + 1;
      joinlet_4694:;
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS2049
      = _M0MPB13StringBuilder21StringBuilder_2einner(19);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2049, (moonbit_string_t)moonbit_string_literal_167.data);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2049, _M0L4rankS2043);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2049, (moonbit_string_t)moonbit_string_literal_168.data);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2049, _M0L4nameS2040);
      moonbit_decref(_M0L4nameS2040);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2049, (moonbit_string_t)moonbit_string_literal_169.data);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2049, _M0L5salesS2042);
      moonbit_decref(_M0L5salesS2042);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2049, (moonbit_string_t)moonbit_string_literal_155.data);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2181
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2049);
      moonbit_decref(_M0L18_2astring__builderS2049);
      #line 193 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2181);
      moonbit_decref(_M0L6_2atmpS2181);
      _M0L6_2atmpS2185 = _M0L1iS2038 + 1;
      _M0L1iS2038 = _M0L6_2atmpS2185;
      continue;
    } else {
      moonbit_decref(_M0L4top3S2037);
    }
    break;
  }
  #line 195 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L10mid__salesS2051
  = _M0MP19moonbitDB8Database13zrangebyscore(_M0L2dbS1955, _M0L10sales__keyS2036, 0x1.9p+6f, 0x1.2cp+8f);
  moonbit_decref(_M0L10sales__keyS2036);
  #line 196 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS2052
  = _M0MPB13StringBuilder21StringBuilder_2einner(39);
  #line 196 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2052, (moonbit_string_t)moonbit_string_literal_170.data);
  #line 196 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2187 = _M0MPC15array5Array6lengthGsE(_M0L10mid__salesS2051);
  moonbit_decref(_M0L10mid__salesS2051);
  #line 196 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2052, _M0L6_2atmpS2187);
  #line 196 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2052, (moonbit_string_t)moonbit_string_literal_155.data);
  #line 196 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2186
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2052);
  moonbit_decref(_M0L18_2astring__builderS2052);
  #line 196 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2186);
  moonbit_decref(_M0L6_2atmpS2186);
  #line 198 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_171.data);
  #line 199 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_124.data);
  _M0L11session__idS2053 = (moonbit_string_t)moonbit_string_literal_172.data;
  #line 201 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MP19moonbitDB8Database3set(_M0L2dbS1955, _M0L11session__idS2053, (moonbit_string_t)moonbit_string_literal_173.data);
  #line 202 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2188
  = _M0MP19moonbitDB8Database6expire(_M0L2dbS1955, _M0L11session__idS2053, 1800);
  #line 203 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS2054
  = _M0MPB13StringBuilder21StringBuilder_2einner(12);
  #line 203 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2054, (moonbit_string_t)moonbit_string_literal_174.data);
  #line 203 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2054, _M0L11session__idS2053);
  #line 203 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2189
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2054);
  moonbit_decref(_M0L18_2astring__builderS2054);
  #line 203 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2189);
  moonbit_decref(_M0L6_2atmpS2189);
  #line 204 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS2055
  = _M0MPB13StringBuilder21StringBuilder_2einner(16);
  #line 204 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2055, (moonbit_string_t)moonbit_string_literal_175.data);
  #line 204 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2192
  = _M0MP19moonbitDB8Database3get(_M0L2dbS1955, _M0L11session__idS2053);
  #line 204 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2191
  = _M0FP39moonbitDB8examples14shopping__cart9show__opt(_M0L6_2atmpS2192);
  if (_M0L6_2atmpS2192) {
    moonbit_decref(_M0L6_2atmpS2192);
  }
  #line 204 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2055, _M0L6_2atmpS2191);
  moonbit_decref(_M0L6_2atmpS2191);
  #line 204 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2190
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2055);
  moonbit_decref(_M0L18_2astring__builderS2055);
  #line 204 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2190);
  moonbit_decref(_M0L6_2atmpS2190);
  #line 205 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS2056
  = _M0MPB13StringBuilder21StringBuilder_2einner(20);
  #line 205 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2056, (moonbit_string_t)moonbit_string_literal_176.data);
  #line 205 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2194
  = _M0MP19moonbitDB8Database3ttl(_M0L2dbS1955, _M0L11session__idS2053);
  #line 205 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2056, _M0L6_2atmpS2194);
  #line 205 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2056, (moonbit_string_t)moonbit_string_literal_177.data);
  #line 205 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2193
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2056);
  moonbit_decref(_M0L18_2astring__builderS2056);
  #line 205 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2193);
  moonbit_decref(_M0L6_2atmpS2193);
  #line 206 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MP19moonbitDB8Database13advance__time(_M0L2dbS1955, 1000000);
  #line 207 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_178.data);
  #line 208 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS2057
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 208 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2057, (moonbit_string_t)moonbit_string_literal_179.data);
  #line 208 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2196
  = _M0MP19moonbitDB8Database6exists(_M0L2dbS1955, _M0L11session__idS2053);
  #line 208 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGbE(_M0L18_2astring__builderS2057, _M0L6_2atmpS2196);
  #line 208 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2195
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2057);
  moonbit_decref(_M0L18_2astring__builderS2057);
  #line 208 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2195);
  moonbit_decref(_M0L6_2atmpS2195);
  #line 209 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MP19moonbitDB8Database13advance__time(_M0L2dbS1955, 1000000);
  #line 210 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_180.data);
  #line 211 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS2058
  = _M0MPB13StringBuilder21StringBuilder_2einner(22);
  #line 211 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2058, (moonbit_string_t)moonbit_string_literal_179.data);
  #line 211 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2198
  = _M0MP19moonbitDB8Database6exists(_M0L2dbS1955, _M0L11session__idS2053);
  moonbit_decref(_M0L11session__idS2053);
  #line 211 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGbE(_M0L18_2astring__builderS2058, _M0L6_2atmpS2198);
  #line 211 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2197
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2058);
  moonbit_decref(_M0L18_2astring__builderS2058);
  #line 211 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2197);
  moonbit_decref(_M0L6_2atmpS2197);
  #line 213 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_181.data);
  #line 214 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_124.data);
  #line 215 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MP19moonbitDB8Database7flushdb(_M0L2dbS1955);
  _M0L6_2atmpS2209 = (moonbit_string_t*)moonbit_make_ref_array_raw(5);
  _M0L6_2atmpS2209[0] = (moonbit_string_t)moonbit_string_literal_182.data;
  _M0L6_2atmpS2209[1] = (moonbit_string_t)moonbit_string_literal_183.data;
  _M0L6_2atmpS2209[2] = (moonbit_string_t)moonbit_string_literal_184.data;
  _M0L6_2atmpS2209[3] = (moonbit_string_t)moonbit_string_literal_185.data;
  _M0L6_2atmpS2209[4] = (moonbit_string_t)moonbit_string_literal_186.data;
  _M0L9hot__keysS2059
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L9hot__keysS2059)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L9hot__keysS2059->$0 = _M0L6_2atmpS2209;
  _M0L9hot__keysS2059->$1 = 5;
  _M0L6_2atmpS2208 = (moonbit_string_t*)moonbit_make_ref_array_raw(5);
  _M0L6_2atmpS2208[0] = (moonbit_string_t)moonbit_string_literal_187.data;
  _M0L6_2atmpS2208[1] = (moonbit_string_t)moonbit_string_literal_188.data;
  _M0L6_2atmpS2208[2] = (moonbit_string_t)moonbit_string_literal_189.data;
  _M0L6_2atmpS2208[3] = (moonbit_string_t)moonbit_string_literal_190.data;
  _M0L6_2atmpS2208[4] = (moonbit_string_t)moonbit_string_literal_191.data;
  _M0L9hot__valsS2060
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L9hot__valsS2060)->meta
  = Moonbit_make_regular_object_header(MOONBIT_REGULAR_LAYOUT_CLASS_INDEXED, 4, 0);
  _M0L9hot__valsS2060->$0 = _M0L6_2atmpS2208;
  _M0L9hot__valsS2060->$1 = 5;
  #line 218 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2199
  = _M0MP19moonbitDB8Database4mset(_M0L2dbS1955, _M0L9hot__keysS2059, _M0L9hot__valsS2060);
  moonbit_decref(_M0L9hot__valsS2060);
  #line 219 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_192.data);
  #line 220 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L13batch__resultS2061
  = _M0MP19moonbitDB8Database4mget(_M0L2dbS1955, _M0L9hot__keysS2059);
  #line 221 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_193.data);
  _M0L1iS2062 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2200;
    #line 222 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
    _M0L6_2atmpS2200
    = _M0MPC15array5Array6lengthGOsE(_M0L13batch__resultS2061);
    if (_M0L1iS2062 < _M0L6_2atmpS2200) {
      moonbit_string_t _M0L1vS2064;
      moonbit_string_t _M0L7_2abindS2066;
      struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2065;
      moonbit_string_t _M0L6_2atmpS2202;
      moonbit_string_t _M0L6_2atmpS2201;
      int32_t _M0L6_2atmpS2205;
      #line 223 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L7_2abindS2066
      = _M0MPC15array5Array2atGOsE(_M0L13batch__resultS2061, _M0L1iS2062);
      if (_M0L7_2abindS2066 == 0) {
        struct _M0TPB13StringBuilder* _M0L18_2astring__builderS2069;
        moonbit_string_t _M0L6_2atmpS2204;
        moonbit_string_t _M0L6_2atmpS2203;
        if (_M0L7_2abindS2066) {
          moonbit_decref(_M0L7_2abindS2066);
        }
        #line 225 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        _M0L18_2astring__builderS2069
        = _M0MPB13StringBuilder21StringBuilder_2einner(12);
        #line 225 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2069, (moonbit_string_t)moonbit_string_literal_136.data);
        #line 225 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        _M0L6_2atmpS2204
        = _M0MPC15array5Array2atGsE(_M0L9hot__keysS2059, _M0L1iS2062);
        #line 225 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2069, _M0L6_2atmpS2204);
        moonbit_decref(_M0L6_2atmpS2204);
        #line 225 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2069, (moonbit_string_t)moonbit_string_literal_194.data);
        #line 225 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        _M0L6_2atmpS2203
        = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2069);
        moonbit_decref(_M0L18_2astring__builderS2069);
        #line 225 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
        _M0FPB7printlnGsE(_M0L6_2atmpS2203);
        moonbit_decref(_M0L6_2atmpS2203);
      } else {
        moonbit_string_t _M0L7_2aSomeS2067 = _M0L7_2abindS2066;
        moonbit_string_t _M0L4_2avS2068 = _M0L7_2aSomeS2067;
        _M0L1vS2064 = _M0L4_2avS2068;
        goto join_2063;
      }
      goto joinlet_4696;
      join_2063:;
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L18_2astring__builderS2065
      = _M0MPB13StringBuilder21StringBuilder_2einner(7);
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2065, (moonbit_string_t)moonbit_string_literal_136.data);
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2202
      = _M0MPC15array5Array2atGsE(_M0L9hot__keysS2059, _M0L1iS2062);
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2065, _M0L6_2atmpS2202);
      moonbit_decref(_M0L6_2atmpS2202);
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2065, (moonbit_string_t)moonbit_string_literal_195.data);
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0MPB13StringBuilder13write__objectGsE(_M0L18_2astring__builderS2065, _M0L1vS2064);
      moonbit_decref(_M0L1vS2064);
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0L6_2atmpS2201
      = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2065);
      moonbit_decref(_M0L18_2astring__builderS2065);
      #line 224 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
      _M0FPB7printlnGsE(_M0L6_2atmpS2201);
      moonbit_decref(_M0L6_2atmpS2201);
      joinlet_4696:;
      _M0L6_2atmpS2205 = _M0L1iS2062 + 1;
      _M0L1iS2062 = _M0L6_2atmpS2205;
      continue;
    } else {
      moonbit_decref(_M0L13batch__resultS2061);
      moonbit_decref(_M0L9hot__keysS2059);
    }
    break;
  }
  #line 229 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_196.data);
  #line 230 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_124.data);
  #line 231 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L18_2astring__builderS2071
  = _M0MPB13StringBuilder21StringBuilder_2einner(15);
  #line 231 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L18_2astring__builderS2071, (moonbit_string_t)moonbit_string_literal_197.data);
  #line 231 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2207 = _M0MP19moonbitDB8Database6dbsize(_M0L2dbS1955);
  moonbit_decref(_M0L2dbS1955);
  #line 231 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0MPB13StringBuilder13write__objectGiE(_M0L18_2astring__builderS2071, _M0L6_2atmpS2207);
  #line 231 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0L6_2atmpS2206
  = _M0MPB13StringBuilder10to__string(_M0L18_2astring__builderS2071);
  moonbit_decref(_M0L18_2astring__builderS2071);
  #line 231 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2206);
  moonbit_decref(_M0L6_2atmpS2206);
  #line 233 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_198.data);
  #line 234 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_199.data);
  #line 235 "/home/developer/Documents2/moonbitDB/examples/shopping_cart/main.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_111.data);
  return 0;
}