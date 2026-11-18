#define ljson_c

#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "ljson.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

static void *defaultalloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud; (void)osize;
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nsize);
}

static int growblock (ljson_Alloc alloc, void *ud, void **block, void *init, size_t *size, size_t need, size_t elem) {
  size_t nsize = (*size > 0) ? *size * 2 : 16;
  void *nb;
  if (nsize < need) nsize = need;
  if (nsize > (size_t)-1 / elem) return 0;
  if (*block == init) {
    nb = alloc(ud, NULL, 0, nsize * elem);
    if (nb != NULL) memcpy(nb, init, *size * elem);
  }
  else nb = alloc(ud, *block, *size * elem, nsize * elem);
  if (nb == NULL) return 0;
  *block = nb;
  *size = nsize;
  return 1;
}

static void freeblock (ljson_Alloc alloc, void *ud, void *block, void *init, size_t size, size_t elem) {
  if (block != init) alloc(ud, block, size * elem, 0);
}

#define DP_SIGNIFICAND_SIZE 52
#define DP_EXPONENT_BIAS (0x3FF + DP_SIGNIFICAND_SIZE)
#define DP_MAX_EXPONENT (0x7FF - DP_EXPONENT_BIAS)
#define DP_MIN_EXPONENT (-DP_EXPONENT_BIAS)
#define DP_DENORMAL_EXPONENT (-DP_EXPONENT_BIAS + 1)
#define DP_EXPONENT_MASK UINT64_C(0x7FF0000000000000)
#define DP_SIGNIFICAND_MASK UINT64_C(0x000FFFFFFFFFFFFF)
#define DP_HIDDEN_BIT UINT64_C(0x0010000000000000)
#define DP_SIGN_MASK UINT64_C(0x8000000000000000)

static uint64_t dbits (double d) {
  uint64_t u;
  memcpy(&u, &d, sizeof(u));
  return u;
}

static double dfrombits (uint64_t u) {
  double d;
  memcpy(&d, &u, sizeof(d));
  return d;
}

static int dnormal (uint64_t u) { return (u & DP_EXPONENT_MASK) != 0 || (u & DP_SIGNIFICAND_MASK) == 0; }

typedef struct DiyFp {
  uint64_t f;
  int e;
} DiyFp;

static DiyFp diyfp (uint64_t f, int e) {
  DiyFp r;
  r.f = f;
  r.e = e;
  return r;
}

static DiyFp diyfromdouble (double d) {
  uint64_t u = dbits(d);
  int biased = (int)((u & DP_EXPONENT_MASK) >> DP_SIGNIFICAND_SIZE);
  uint64_t significand = u & DP_SIGNIFICAND_MASK;
  if (biased != 0) return diyfp(significand + DP_HIDDEN_BIT, biased - DP_EXPONENT_BIAS);
  return diyfp(significand, DP_MIN_EXPONENT + 1);
}

static int clzll (uint64_t x) {
#if defined(_MSC_VER) && defined(_WIN64)
  unsigned long r = 0;
  _BitScanReverse64(&r, x);
  return 63 - (int)r;
#elif defined(__GNUC__)
  return __builtin_clzll(x);
#else
  int r = 0;
  while (!(x & (UINT64_C(1) << 63))) {
    x <<= 1;
    r++;
  }
  return r;
#endif
}

static DiyFp diymul (DiyFp a, DiyFp b) {
#if defined(__GNUC__) && defined(__SIZEOF_INT128__)
  __extension__ typedef unsigned __int128 u128;
  u128 p = (u128)a.f * (u128)b.f;
  uint64_t h = (uint64_t)(p >> 64);
  uint64_t l = (uint64_t)p;
  if (l & (UINT64_C(1) << 63)) h++;
  return diyfp(h, a.e + b.e + 64);
#else
  const uint64_t M32 = 0xFFFFFFFF;
  const uint64_t ah = a.f >> 32, al = a.f & M32, bh = b.f >> 32, bl = b.f & M32;
  const uint64_t hh = ah * bh, lh = al * bh, hl = ah * bl, ll = al * bl;
  uint64_t tmp = (ll >> 32) + (hl & M32) + (lh & M32);
  tmp += 1U << 31;
  return diyfp(hh + (hl >> 32) + (lh >> 32) + (tmp >> 32), a.e + b.e + 64);
#endif
}

static DiyFp diynormalize (DiyFp v) {
  int s = clzll(v.f);
  return diyfp(v.f << s, v.e - s);
}

static DiyFp diynormalizeboundary (DiyFp v) {
  while (!(v.f & (DP_HIDDEN_BIT << 1))) {
    v.f <<= 1;
    v.e--;
  }
  v.f <<= (64 - DP_SIGNIFICAND_SIZE - 2);
  v.e = v.e - (64 - DP_SIGNIFICAND_SIZE - 2);
  return v;
}

static void diyboundaries (DiyFp v, DiyFp *minus, DiyFp *plus) {
  DiyFp pl = diynormalizeboundary(diyfp((v.f << 1) + 1, v.e - 1));
  DiyFp mi = (v.f == DP_HIDDEN_BIT) ? diyfp((v.f << 2) - 1, v.e - 2) : diyfp((v.f << 1) - 1, v.e - 1);
  mi.f <<= mi.e - pl.e;
  mi.e = pl.e;
  *plus = pl;
  *minus = mi;
}

static double diytodouble (DiyFp v) {
  uint64_t be;
  if (v.e < DP_DENORMAL_EXPONENT) return 0.0;
  if (v.e >= DP_MAX_EXPONENT) return dfrombits(DP_EXPONENT_MASK);
  be = (v.e == DP_DENORMAL_EXPONENT && (v.f & DP_HIDDEN_BIT) == 0) ? 0 : (uint64_t)(v.e + DP_EXPONENT_BIAS);
  return dfrombits((v.f & DP_SIGNIFICAND_MASK) | (be << DP_SIGNIFICAND_SIZE));
}

static const uint64_t cachedf[] = {
  UINT64_C(0xfa8fd5a0081c0288), UINT64_C(0xbaaee17fa23ebf76), UINT64_C(0x8b16fb203055ac76), UINT64_C(0xcf42894a5dce35ea),
  UINT64_C(0x9a6bb0aa55653b2d), UINT64_C(0xe61acf033d1a45df), UINT64_C(0xab70fe17c79ac6ca), UINT64_C(0xff77b1fcbebcdc4f),
  UINT64_C(0xbe5691ef416bd60c), UINT64_C(0x8dd01fad907ffc3c), UINT64_C(0xd3515c2831559a83), UINT64_C(0x9d71ac8fada6c9b5),
  UINT64_C(0xea9c227723ee8bcb), UINT64_C(0xaecc49914078536d), UINT64_C(0x823c12795db6ce57), UINT64_C(0xc21094364dfb5637),
  UINT64_C(0x9096ea6f3848984f), UINT64_C(0xd77485cb25823ac7), UINT64_C(0xa086cfcd97bf97f4), UINT64_C(0xef340a98172aace5),
  UINT64_C(0xb23867fb2a35b28e), UINT64_C(0x84c8d4dfd2c63f3b), UINT64_C(0xc5dd44271ad3cdba), UINT64_C(0x936b9fcebb25c996),
  UINT64_C(0xdbac6c247d62a584), UINT64_C(0xa3ab66580d5fdaf6), UINT64_C(0xf3e2f893dec3f126), UINT64_C(0xb5b5ada8aaff80b8),
  UINT64_C(0x87625f056c7c4a8b), UINT64_C(0xc9bcff6034c13053), UINT64_C(0x964e858c91ba2655), UINT64_C(0xdff9772470297ebd),
  UINT64_C(0xa6dfbd9fb8e5b88f), UINT64_C(0xf8a95fcf88747d94), UINT64_C(0xb94470938fa89bcf), UINT64_C(0x8a08f0f8bf0f156b),
  UINT64_C(0xcdb02555653131b6), UINT64_C(0x993fe2c6d07b7fac), UINT64_C(0xe45c10c42a2b3b06), UINT64_C(0xaa242499697392d3),
  UINT64_C(0xfd87b5f28300ca0e), UINT64_C(0xbce5086492111aeb), UINT64_C(0x8cbccc096f5088cc), UINT64_C(0xd1b71758e219652c),
  UINT64_C(0x9c40000000000000), UINT64_C(0xe8d4a51000000000), UINT64_C(0xad78ebc5ac620000), UINT64_C(0x813f3978f8940984),
  UINT64_C(0xc097ce7bc90715b3), UINT64_C(0x8f7e32ce7bea5c70), UINT64_C(0xd5d238a4abe98068), UINT64_C(0x9f4f2726179a2245),
  UINT64_C(0xed63a231d4c4fb27), UINT64_C(0xb0de65388cc8ada8), UINT64_C(0x83c7088e1aab65db), UINT64_C(0xc45d1df942711d9a),
  UINT64_C(0x924d692ca61be758), UINT64_C(0xda01ee641a708dea), UINT64_C(0xa26da3999aef774a), UINT64_C(0xf209787bb47d6b85),
  UINT64_C(0xb454e4a179dd1877), UINT64_C(0x865b86925b9bc5c2), UINT64_C(0xc83553c5c8965d3d), UINT64_C(0x952ab45cfa97a0b3),
  UINT64_C(0xde469fbd99a05fe3), UINT64_C(0xa59bc234db398c25), UINT64_C(0xf6c69a72a3989f5c), UINT64_C(0xb7dcbf5354e9bece),
  UINT64_C(0x88fcf317f22241e2), UINT64_C(0xcc20ce9bd35c78a5), UINT64_C(0x98165af37b2153df), UINT64_C(0xe2a0b5dc971f303a),
  UINT64_C(0xa8d9d1535ce3b396), UINT64_C(0xfb9b7cd9a4a7443c), UINT64_C(0xbb764c4ca7a44410), UINT64_C(0x8bab8eefb6409c1a),
  UINT64_C(0xd01fef10a657842c), UINT64_C(0x9b10a4e5e9913129), UINT64_C(0xe7109bfba19c0c9d), UINT64_C(0xac2820d9623bf429),
  UINT64_C(0x80444b5e7aa7cf85), UINT64_C(0xbf21e44003acdd2d), UINT64_C(0x8e679c2f5e44ff8f), UINT64_C(0xd433179d9c8cb841),
  UINT64_C(0x9e19db92b4e31ba9), UINT64_C(0xeb96bf6ebadf77d9), UINT64_C(0xaf87023b9bf0ee6b),
};

static const int16_t cachede[] = {
  -1220, -1193, -1166, -1140, -1113, -1087, -1060, -1034, -1007, -980, -954, -927, -901, -874, -847,
  -821, -794, -768, -741, -715, -688, -661, -635, -608, -582, -555, -529, -502, -475, -449,
  -422, -396, -369, -343, -316, -289, -263, -236, -210, -183, -157, -130, -103, -77, -50,
  -24, 3, 30, 56, 83, 109, 136, 162, 189, 216, 242, 269, 295, 322, 348,
  375, 402, 428, 455, 481, 508, 534, 561, 588, 614, 641, 667, 694, 720, 747,
  774, 800, 827, 853, 880, 907, 933, 960, 986, 1013, 1039, 1066,
};

static DiyFp cachedpower (int e, int *K) {
  double dk = (-61 - e) * 0.30102999566398114 + 347;
  int k = (int)dk;
  unsigned index;
  if (dk - k > 0.0) k++;
  index = (unsigned)((k >> 3) + 1);
  *K = -(-348 + (int)(index << 3));
  return diyfp(cachedf[index], cachede[index]);
}

static DiyFp cachedpower10 (int exp, int *outexp) {
  unsigned index = (unsigned)(exp + 348) / 8u;
  *outexp = -348 + (int)index * 8;
  return diyfp(cachedf[index], cachede[index]);
}

static const char digitslut[] = "00010203040506070809101112131415161718192021222324252627282930313233343536373839404142434445464748495051525354555657585960616263646566676869707172737475767778798081828384858687888990919293949596979899";

static const uint64_t pow10u64[] = {
  UINT64_C(1), UINT64_C(10), UINT64_C(100), UINT64_C(1000), UINT64_C(10000), UINT64_C(100000), UINT64_C(1000000), UINT64_C(10000000), UINT64_C(100000000), UINT64_C(1000000000),
  UINT64_C(10000000000), UINT64_C(100000000000), UINT64_C(1000000000000), UINT64_C(10000000000000), UINT64_C(100000000000000), UINT64_C(1000000000000000), UINT64_C(10000000000000000), UINT64_C(100000000000000000), UINT64_C(1000000000000000000), UINT64_C(10000000000000000000),
};

static void grisuround (char *buff, int len, uint64_t delta, uint64_t rest, uint64_t tenkappa, uint64_t wpw) {
  while (rest < wpw && delta - rest >= tenkappa && (rest + tenkappa < wpw || wpw - rest > rest + tenkappa - wpw)) {
    buff[len - 1]--;
    rest += tenkappa;
  }
}

static int countdecimaldigit32 (uint32_t n) {
  if (n < 10) return 1;
  if (n < 100) return 2;
  if (n < 1000) return 3;
  if (n < 10000) return 4;
  if (n < 100000) return 5;
  if (n < 1000000) return 6;
  if (n < 10000000) return 7;
  if (n < 100000000) return 8;
  return 9;
}

static void digitgen (DiyFp W, DiyFp Mp, uint64_t delta, char *buff, int *len, int *K) {
  const DiyFp one = diyfp(UINT64_C(1) << -Mp.e, Mp.e);
  const DiyFp wpw = diyfp(Mp.f - W.f, Mp.e);
  uint32_t p1 = (uint32_t)(Mp.f >> -one.e);
  uint64_t p2 = Mp.f & (one.f - 1);
  int kappa = countdecimaldigit32(p1);
  *len = 0;
  while (kappa > 0) {
    uint32_t d = 0;
    uint64_t tmp;
    switch (kappa) {
      case 9: d = p1 / 100000000; p1 %= 100000000; break;
      case 8: d = p1 / 10000000; p1 %= 10000000; break;
      case 7: d = p1 / 1000000; p1 %= 1000000; break;
      case 6: d = p1 / 100000; p1 %= 100000; break;
      case 5: d = p1 / 10000; p1 %= 10000; break;
      case 4: d = p1 / 1000; p1 %= 1000; break;
      case 3: d = p1 / 100; p1 %= 100; break;
      case 2: d = p1 / 10; p1 %= 10; break;
      case 1: d = p1; p1 = 0; break;
      default: break;
    }
    if (d || *len) buff[(*len)++] = (char)('0' + (char)d);
    kappa--;
    tmp = ((uint64_t)p1 << -one.e) + p2;
    if (tmp <= delta) {
      *K += kappa;
      grisuround(buff, *len, delta, tmp, pow10u64[kappa] << -one.e, wpw.f);
      return;
    }
  }
  for (;;) {
    char d;
    p2 *= 10;
    delta *= 10;
    d = (char)(p2 >> -one.e);
    if (d || *len) buff[(*len)++] = (char)('0' + d);
    p2 &= one.f - 1;
    kappa--;
    if (p2 < delta) {
      int index = -kappa;
      *K += kappa;
      grisuround(buff, *len, delta, p2, one.f, wpw.f * (index < 20 ? pow10u64[index] : 0));
      return;
    }
  }
}

static void grisu2 (double value, char *buff, int *length, int *K) {
  const DiyFp v = diyfromdouble(value);
  DiyFp wm, wp, cmk, W, Wp, Wm;
  diyboundaries(v, &wm, &wp);
  cmk = cachedpower(wp.e, K);
  W = diymul(diynormalize(v), cmk);
  Wp = diymul(wp, cmk);
  Wm = diymul(wm, cmk);
  Wm.f++;
  Wp.f--;
  digitgen(W, Wp, Wp.f - Wm.f, buff, length, K);
}

static char *writeexponent (int K, char *buff) {
  if (K < 0) {
    *buff++ = '-';
    K = -K;
  }
  if (K >= 100) {
    *buff++ = (char)('0' + (char)(K / 100));
    K %= 100;
    *buff++ = digitslut[K * 2];
    *buff++ = digitslut[K * 2 + 1];
  }
  else if (K >= 10) {
    *buff++ = digitslut[K * 2];
    *buff++ = digitslut[K * 2 + 1];
  }
  else *buff++ = (char)('0' + (char)K);
  return buff;
}

static char *prettify (char *buff, int length, int k, int maxdecimals) {
  const int kk = length + k;
  int i;
  if (0 <= k && kk <= 21) {
    for (i = length; i < kk; i++) buff[i] = '0';
    buff[kk] = '.';
    buff[kk + 1] = '0';
    return &buff[kk + 2];
  }
  else if (0 < kk && kk <= 21) {
    memmove(&buff[kk + 1], &buff[kk], (size_t)(length - kk));
    buff[kk] = '.';
    if (0 > k + maxdecimals) {
      for (i = kk + maxdecimals; i > kk + 1; i--) {
        if (buff[i] != '0') return &buff[i + 1];
      }
      return &buff[kk + 2];
    }
    return &buff[length + 1];
  }
  else if (-6 < kk && kk <= 0) {
    const int offset = 2 - kk;
    memmove(&buff[offset], &buff[0], (size_t)length);
    buff[0] = '0';
    buff[1] = '.';
    for (i = 2; i < offset; i++) buff[i] = '0';
    if (length - kk > maxdecimals) {
      for (i = maxdecimals + 1; i > 2; i--) {
        if (buff[i] != '0') return &buff[i + 1];
      }
      return &buff[3];
    }
    return &buff[length + offset];
  }
  else if (kk < -maxdecimals) {
    buff[0] = '0';
    buff[1] = '.';
    buff[2] = '0';
    return &buff[3];
  }
  else if (length == 1) {
    buff[1] = 'e';
    return writeexponent(kk - 1, &buff[2]);
  }
  else {
    memmove(&buff[2], &buff[1], (size_t)(length - 1));
    buff[1] = '.';
    buff[length + 1] = 'e';
    return writeexponent(kk - 1, &buff[0 + length + 2]);
  }
}

char *ljson_dtoa (double value, char *buff, int maxdecimals) {
  uint64_t u = dbits(value);
  if ((u & (DP_EXPONENT_MASK | DP_SIGNIFICAND_MASK)) == 0) {
    if (u & DP_SIGN_MASK) *buff++ = '-';
    buff[0] = '0';
    buff[1] = '.';
    buff[2] = '0';
    return &buff[3];
  }
  else {
    int length, K;
    if (value < 0) {
      *buff++ = '-';
      value = -value;
    }
    grisu2(value, buff, &length, &K);
    return prettify(buff, length, K, maxdecimals);
  }
}

char *ljson_u64toa (uint64_t u, char *buff) {
  char tmp[20];
  int n = 0;
  do {
    tmp[n++] = (char)('0' + (int)(u % 10));
    u /= 10;
  } while (u != 0);
  while (n > 0) *buff++ = tmp[--n];
  return buff;
}

char *ljson_i64toa (int64_t i, char *buff) {
  uint64_t u = (uint64_t)i;
  if (i < 0) {
    *buff++ = '-';
    u = ~u + 1;
  }
  return ljson_u64toa(u, buff);
}

static const double pow10_[] = {
  1e+0, 1e+1, 1e+2, 1e+3, 1e+4, 1e+5, 1e+6, 1e+7, 1e+8, 1e+9, 1e+10,
  1e+11, 1e+12, 1e+13, 1e+14, 1e+15, 1e+16, 1e+17, 1e+18, 1e+19, 1e+20, 1e+21,
  1e+22, 1e+23, 1e+24, 1e+25, 1e+26, 1e+27, 1e+28, 1e+29, 1e+30, 1e+31, 1e+32,
  1e+33, 1e+34, 1e+35, 1e+36, 1e+37, 1e+38, 1e+39, 1e+40, 1e+41, 1e+42, 1e+43,
  1e+44, 1e+45, 1e+46, 1e+47, 1e+48, 1e+49, 1e+50, 1e+51, 1e+52, 1e+53, 1e+54,
  1e+55, 1e+56, 1e+57, 1e+58, 1e+59, 1e+60, 1e+61, 1e+62, 1e+63, 1e+64, 1e+65,
  1e+66, 1e+67, 1e+68, 1e+69, 1e+70, 1e+71, 1e+72, 1e+73, 1e+74, 1e+75, 1e+76,
  1e+77, 1e+78, 1e+79, 1e+80, 1e+81, 1e+82, 1e+83, 1e+84, 1e+85, 1e+86, 1e+87,
  1e+88, 1e+89, 1e+90, 1e+91, 1e+92, 1e+93, 1e+94, 1e+95, 1e+96, 1e+97, 1e+98,
  1e+99, 1e+100, 1e+101, 1e+102, 1e+103, 1e+104, 1e+105, 1e+106, 1e+107, 1e+108, 1e+109,
  1e+110, 1e+111, 1e+112, 1e+113, 1e+114, 1e+115, 1e+116, 1e+117, 1e+118, 1e+119, 1e+120,
  1e+121, 1e+122, 1e+123, 1e+124, 1e+125, 1e+126, 1e+127, 1e+128, 1e+129, 1e+130, 1e+131,
  1e+132, 1e+133, 1e+134, 1e+135, 1e+136, 1e+137, 1e+138, 1e+139, 1e+140, 1e+141, 1e+142,
  1e+143, 1e+144, 1e+145, 1e+146, 1e+147, 1e+148, 1e+149, 1e+150, 1e+151, 1e+152, 1e+153,
  1e+154, 1e+155, 1e+156, 1e+157, 1e+158, 1e+159, 1e+160, 1e+161, 1e+162, 1e+163, 1e+164,
  1e+165, 1e+166, 1e+167, 1e+168, 1e+169, 1e+170, 1e+171, 1e+172, 1e+173, 1e+174, 1e+175,
  1e+176, 1e+177, 1e+178, 1e+179, 1e+180, 1e+181, 1e+182, 1e+183, 1e+184, 1e+185, 1e+186,
  1e+187, 1e+188, 1e+189, 1e+190, 1e+191, 1e+192, 1e+193, 1e+194, 1e+195, 1e+196, 1e+197,
  1e+198, 1e+199, 1e+200, 1e+201, 1e+202, 1e+203, 1e+204, 1e+205, 1e+206, 1e+207, 1e+208,
  1e+209, 1e+210, 1e+211, 1e+212, 1e+213, 1e+214, 1e+215, 1e+216, 1e+217, 1e+218, 1e+219,
  1e+220, 1e+221, 1e+222, 1e+223, 1e+224, 1e+225, 1e+226, 1e+227, 1e+228, 1e+229, 1e+230,
  1e+231, 1e+232, 1e+233, 1e+234, 1e+235, 1e+236, 1e+237, 1e+238, 1e+239, 1e+240, 1e+241,
  1e+242, 1e+243, 1e+244, 1e+245, 1e+246, 1e+247, 1e+248, 1e+249, 1e+250, 1e+251, 1e+252,
  1e+253, 1e+254, 1e+255, 1e+256, 1e+257, 1e+258, 1e+259, 1e+260, 1e+261, 1e+262, 1e+263,
  1e+264, 1e+265, 1e+266, 1e+267, 1e+268, 1e+269, 1e+270, 1e+271, 1e+272, 1e+273, 1e+274,
  1e+275, 1e+276, 1e+277, 1e+278, 1e+279, 1e+280, 1e+281, 1e+282, 1e+283, 1e+284, 1e+285,
  1e+286, 1e+287, 1e+288, 1e+289, 1e+290, 1e+291, 1e+292, 1e+293, 1e+294, 1e+295, 1e+296,
  1e+297, 1e+298, 1e+299, 1e+300, 1e+301, 1e+302, 1e+303, 1e+304, 1e+305, 1e+306, 1e+307,
  1e+308,
};

static double fastpath (double significand, int exp) {
  if (exp < -308) return 0.0;
  else if (exp >= 0) return significand * pow10_[exp];
  else return significand / pow10_[-exp];
}

static double strtodnormal (double d, int p) {
  if (p < -308) {
    d = fastpath(d, -308);
    d = fastpath(d, p + 308);
  }
  else d = fastpath(d, p);
  return d;
}

#define BIGCAP (3328 / sizeof(uint64_t))

typedef struct BigInt {
  uint64_t d[BIGCAP];
  size_t n;
} BigInt;

static void bigset (BigInt *b, uint64_t u) {
  b->d[0] = u;
  b->n = 1;
}

static void bigcopy (BigInt *b, const BigInt *a) {
  b->n = a->n;
  memcpy(b->d, a->d, a->n * sizeof(uint64_t));
}

static int bigiszero (const BigInt *b) { return b->n == 1 && b->d[0] == 0; }

static int bigequ (const BigInt *b, uint64_t u) { return b->n == 1 && b->d[0] == u; }

static void bigpush (BigInt *b, uint64_t digit) {
  if (b->n < BIGCAP) b->d[b->n++] = digit;
}

static void bigaddu (BigInt *b, uint64_t u) {
  uint64_t backup = b->d[0];
  size_t i;
  b->d[0] += u;
  for (i = 0; i < b->n - 1; i++) {
    if (b->d[i] >= backup) return;
    backup = b->d[i + 1];
    b->d[i + 1] += 1;
  }
  if (b->d[b->n - 1] < backup) bigpush(b, 1);
}

static uint64_t muladd64 (uint64_t a, uint64_t b, uint64_t k, uint64_t *high) {
#if defined(__GNUC__) && defined(__SIZEOF_INT128__)
  __extension__ typedef unsigned __int128 u128;
  u128 p = (u128)a * (u128)b;
  p += k;
  *high = (uint64_t)(p >> 64);
  return (uint64_t)p;
#else
  const uint64_t a0 = a & 0xFFFFFFFF, a1 = a >> 32, b0 = b & 0xFFFFFFFF, b1 = b >> 32;
  uint64_t x0 = a0 * b0, x1 = a0 * b1, x2 = a1 * b0, x3 = a1 * b1;
  uint64_t lo, hi;
  x1 += (x0 >> 32);
  x1 += x2;
  if (x1 < x2) x3 += (UINT64_C(1) << 32);
  lo = (x1 << 32) + (x0 & 0xFFFFFFFF);
  hi = x3 + (x1 >> 32);
  lo += k;
  if (lo < k) hi++;
  *high = hi;
  return lo;
#endif
}

static void bigmul64 (BigInt *b, uint64_t u) {
  uint64_t k = 0;
  size_t i;
  if (u == 0) { bigset(b, 0); return; }
  if (u == 1) return;
  if (bigequ(b, 1)) { bigset(b, u); return; }
  for (i = 0; i < b->n; i++) {
    uint64_t hi;
    b->d[i] = muladd64(b->d[i], u, k, &hi);
    k = hi;
  }
  if (k > 0) bigpush(b, k);
}

static void bigmul32 (BigInt *b, uint32_t u) {
  uint64_t k = 0;
  size_t i;
  if (u == 0) { bigset(b, 0); return; }
  if (u == 1) return;
  if (bigequ(b, 1)) { bigset(b, u); return; }
  for (i = 0; i < b->n; i++) {
    const uint64_t c = b->d[i] >> 32;
    const uint64_t d = b->d[i] & 0xFFFFFFFF;
    const uint64_t uc = u * c;
    const uint64_t ud = u * d;
    const uint64_t p0 = ud + k;
    const uint64_t p1 = uc + (p0 >> 32);
    b->d[i] = (p0 & 0xFFFFFFFF) | (p1 << 32);
    k = p1 >> 32;
  }
  if (k > 0) bigpush(b, k);
}

static void bigshl (BigInt *b, size_t shift) {
  size_t offset, inter, i;
  if (bigiszero(b) || shift == 0) return;
  offset = shift / 64;
  inter = shift % 64;
  if (b->n + offset > BIGCAP) return;
  if (inter == 0) {
    memmove(b->d + offset, b->d, b->n * sizeof(uint64_t));
    b->n += offset;
  }
  else {
    if (b->n + offset >= BIGCAP) return;
    b->d[b->n] = 0;
    for (i = b->n; i > 0; i--) b->d[i + offset] = (b->d[i] << inter) | (b->d[i - 1] >> (64 - inter));
    b->d[offset] = b->d[0] << inter;
    b->n += offset;
    if (b->d[b->n]) b->n++;
  }
  memset(b->d, 0, offset * sizeof(uint64_t));
}

static void bigmulpow5 (BigInt *b, unsigned exp) {
  static const uint32_t pow5[12] = { 5, 25, 125, 625, 3125, 15625, 78125, 390625, 1953125, 9765625, 48828125, 244140625 };
  if (exp == 0) return;
  for (; exp >= 27; exp -= 27) bigmul64(b, UINT64_C(0x6765C793FA10079D));
  for (; exp >= 13; exp -= 13) bigmul32(b, 1220703125u);
  if (exp > 0) bigmul32(b, pow5[exp - 1]);
}

static int bigcompare (const BigInt *a, const BigInt *b) {
  size_t i;
  if (a->n != b->n) return a->n < b->n ? -1 : 1;
  for (i = a->n; i-- > 0;) {
    if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
  }
  return 0;
}

static void bigdifference (const BigInt *a, const BigInt *b, BigInt *out) {
  const BigInt *x = a, *y = b;
  uint64_t borrow = 0;
  size_t i;
  if (bigcompare(a, b) < 0) {
    x = b;
    y = a;
  }
  for (i = 0; i < x->n; i++) {
    uint64_t d = x->d[i] - borrow;
    if (i < y->n) d -= y->d[i];
    borrow = (d > x->d[i]) ? 1 : 0;
    out->d[i] = d;
    if (d != 0) out->n = i + 1;
  }
}

static uint64_t parseu64 (const char *begin, const char *end) {
  uint64_t r = 0;
  for (; begin != end; begin++) r = r * 10u + (unsigned)(*begin - '0');
  return r;
}

static void bigappenddecimal64 (BigInt *b, const char *begin, const char *end) {
  uint64_t u = parseu64(begin, end);
  if (bigiszero(b)) bigset(b, u);
  else {
    unsigned exp = (unsigned)(end - begin);
    bigmulpow5(b, exp);
    bigshl(b, exp);
    bigaddu(b, u);
  }
}

static void bigfromdecimals (BigInt *b, const char *decimals, size_t length) {
  size_t i = 0;
  bigset(b, 0);
  while (length >= 19) {
    bigappenddecimal64(b, decimals + i, decimals + i + 19);
    length -= 19;
    i += 19;
  }
  if (length > 0) bigappenddecimal64(b, decimals + i, decimals + i + length);
}

static int min3 (int a, int b, int c) {
  int m = a;
  if (m > b) m = b;
  if (m > c) m = c;
  return m;
}

static int checkwithinhalfulp (double b, const BigInt *d, int dexp) {
  const uint64_t u = dbits(b);
  const uint64_t bint = dnormal(u) ? (u & DP_SIGNIFICAND_MASK) | DP_HIDDEN_BIT : (u & DP_SIGNIFICAND_MASK);
  const int bexp = (dnormal(u) ? (int)((u & DP_EXPONENT_MASK) >> DP_SIGNIFICAND_SIZE) - 0x3FF : 1 - 0x3FF) - DP_SIGNIFICAND_SIZE;
  const int hexp = bexp - 1;
  int ds2 = 0, ds5 = 0, bs2 = 0, bs5 = 0, hs2 = 0, hs5 = 0, common;
  BigInt ds, bs, hs, delta;
  if (dexp >= 0) {
    ds2 += dexp;
    ds5 += dexp;
  }
  else {
    bs2 -= dexp;
    bs5 -= dexp;
    hs2 -= dexp;
    hs5 -= dexp;
  }
  if (bexp >= 0) bs2 += bexp;
  else {
    ds2 -= bexp;
    hs2 -= bexp;
  }
  if (hexp >= 0) hs2 += hexp;
  else {
    ds2 -= hexp;
    bs2 -= hexp;
  }
  common = min3(ds2, bs2, hs2);
  ds2 -= common;
  bs2 -= common;
  hs2 -= common;
  bigcopy(&ds, d);
  bigmulpow5(&ds, (unsigned)ds5);
  bigshl(&ds, (unsigned)ds2);
  bigset(&bs, bint);
  bigmulpow5(&bs, (unsigned)bs5);
  bigshl(&bs, (unsigned)bs2);
  bigset(&hs, 1);
  bigmulpow5(&hs, (unsigned)hs5);
  bigshl(&hs, (unsigned)hs2);
  bigset(&delta, 0);
  bigdifference(&ds, &bs, &delta);
  return bigcompare(&delta, &hs);
}

static int strtodfast (double d, int p, double *result) {
  if (p > 22 && p < 22 + 16) {
    d *= pow10_[p - 22];
    p = 22;
  }
  if (p >= -22 && p <= 22 && d <= 9007199254740991.0) {
    *result = fastpath(d, p);
    return 1;
  }
  return 0;
}

static int strtoddiyfp (const char *decimals, int dlen, int dexp, double *result) {
  static const uint64_t adjf[] = { UINT64_C(0xa000000000000000), UINT64_C(0xc800000000000000), UINT64_C(0xfa00000000000000), UINT64_C(0x9c40000000000000), UINT64_C(0xc350000000000000), UINT64_C(0xf424000000000000), UINT64_C(0x9896800000000000) };
  static const int adje[] = { -60, -57, -54, -50, -47, -44, -40 };
  const int ulpshift = 3, ulp = 1 << 3;
  uint64_t significand = 0, precisionbits, halfway;
  int i = 0, remaining, actualexp, oldexp, effective, precisionsize;
  int64_t error;
  DiyFp v, cached, rounded;
  for (; i < dlen; i++) {
    if (significand > UINT64_C(0x1999999999999999) || (significand == UINT64_C(0x1999999999999999) && decimals[i] >= '5')) break;
    significand = significand * 10u + (unsigned)(decimals[i] - '0');
  }
  if (i < dlen && decimals[i] >= '5') significand++;
  remaining = dlen - i;
  error = (remaining == 0) ? 0 : ulp / 2;
  v = diynormalize(diyfp(significand, 0));
  error <<= -v.e;
  dexp += remaining;
  cached = cachedpower10(dexp, &actualexp);
  if (actualexp != dexp) {
    int adjustment = dexp - actualexp;
    v = diymul(v, diyfp(adjf[adjustment - 1], adje[adjustment - 1]));
    if (dlen + adjustment > 19) error += ulp / 2;
  }
  v = diymul(v, cached);
  error += ulp + (error == 0 ? 0 : 1);
  oldexp = v.e;
  v = diynormalize(v);
  error <<= oldexp - v.e;
  effective = (64 + v.e >= -1021) ? 53 : (64 + v.e <= -1074) ? 0 : 64 + v.e + 1074;
  precisionsize = 64 - effective;
  if (precisionsize + ulpshift >= 64) {
    int scaleexp = (precisionsize + ulpshift) - 63;
    v.f >>= scaleexp;
    v.e += scaleexp;
    error = (error >> scaleexp) + 1 + ulp;
    precisionsize -= scaleexp;
  }
  rounded = diyfp(v.f >> precisionsize, v.e + precisionsize);
  precisionbits = (v.f & ((UINT64_C(1) << precisionsize) - 1)) * ulp;
  halfway = (UINT64_C(1) << (precisionsize - 1)) * ulp;
  if (precisionbits >= halfway + (unsigned)error) {
    rounded.f++;
    if (rounded.f & (DP_HIDDEN_BIT << 1)) {
      rounded.f >>= 1;
      rounded.e++;
    }
  }
  *result = diytodouble(rounded);
  return halfway - (unsigned)error >= precisionbits || precisionbits >= halfway + (unsigned)error;
}

static double strtodbig (double approx, const char *decimals, int dlen, int dexp) {
  BigInt dint;
  uint64_t u = dbits(approx);
  int cmp;
  bigfromdecimals(&dint, decimals, (size_t)dlen);
  cmp = checkwithinhalfulp(approx, &dint, dexp);
  if (cmp < 0) return approx;
  else if (cmp == 0) return (u & DP_SIGNIFICAND_MASK & 1) ? dfrombits(u + 1) : approx;
  else return dfrombits(u + 1);
}

static double strtodfull (double d, int p, const char *decimals, size_t length, size_t decimalpos, int exp) {
  double result = 0.0;
  int dlen, dexp;
  if (strtodfast(d, p, &result)) return result;
  dlen = (int)length;
  dexp = exp - (int)(length - decimalpos);
  while (dlen > 0 && *decimals == '0') {
    dlen--;
    decimals++;
  }
  while (dlen > 0 && decimals[dlen - 1] == '0') {
    dlen--;
    dexp++;
  }
  if (dlen == 0) return 0.0;
  if (dlen > 768) {
    dexp += dlen - 768;
    dlen = 768;
  }
  if (dlen + dexp <= -324) return 0.0;
  if (dlen + dexp > 309) return dfrombits(DP_EXPONENT_MASK);
  if (strtoddiyfp(decimals, dlen, dexp, &result)) return result;
  return strtodbig(result, decimals, dlen, dexp);
}

enum { S_FINISH, S_ERROR, S_START, S_OBJECTINITIAL, S_MEMBERKEY, S_MEMBERVALUE, S_OBJECTFINISH, S_ARRAYINITIAL, S_ELEMENT, S_ARRAYFINISH, S_VALUE, S_ELEMENTDELIMITER, S_MEMBERDELIMITER, S_KEYVALUEDELIMITER, S_COUNT };

enum { T_LEFTBRACKET, T_RIGHTBRACKET, T_LEFTCURLY, T_RIGHTCURLY, T_COMMA, T_COLON, T_STRING, T_FALSE, T_TRUE, T_NULL, T_NUMBER, T_COUNT };

static const unsigned char predict[S_COUNT][T_COUNT] = {
  { S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR },
  { S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR },
  { S_ARRAYINITIAL, S_ERROR, S_OBJECTINITIAL, S_ERROR, S_ERROR, S_ERROR, S_VALUE, S_VALUE, S_VALUE, S_VALUE, S_VALUE },
  { S_ERROR, S_ERROR, S_ERROR, S_OBJECTFINISH, S_ERROR, S_ERROR, S_MEMBERKEY, S_ERROR, S_ERROR, S_ERROR, S_ERROR },
  { S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_KEYVALUEDELIMITER, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR },
  { S_ERROR, S_ERROR, S_ERROR, S_OBJECTFINISH, S_MEMBERDELIMITER, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR },
  { S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR },
  { S_ARRAYINITIAL, S_ARRAYFINISH, S_OBJECTINITIAL, S_ERROR, S_ERROR, S_ERROR, S_ELEMENT, S_ELEMENT, S_ELEMENT, S_ELEMENT, S_ELEMENT },
  { S_ERROR, S_ARRAYFINISH, S_ERROR, S_ERROR, S_ELEMENTDELIMITER, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR },
  { S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR },
  { S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR, S_ERROR },
  { S_ARRAYINITIAL, S_ARRAYFINISH, S_OBJECTINITIAL, S_ERROR, S_ERROR, S_ERROR, S_ELEMENT, S_ELEMENT, S_ELEMENT, S_ELEMENT, S_ELEMENT },
  { S_ERROR, S_ERROR, S_ERROR, S_OBJECTFINISH, S_ERROR, S_ERROR, S_MEMBERKEY, S_ERROR, S_ERROR, S_ERROR, S_ERROR },
  { S_ARRAYINITIAL, S_ERROR, S_OBJECTINITIAL, S_ERROR, S_ERROR, S_ERROR, S_MEMBERVALUE, S_MEMBERVALUE, S_MEMBERVALUE, S_MEMBERVALUE, S_MEMBERVALUE },
};

typedef struct Frame {
  int state;
  size_t count;
} Frame;

typedef struct Parser {
  const char *b, *p, *e;
  unsigned flags;
  int error;
  size_t erroffset;
  const ljson_Handler *h;
  void *ud;
  ljson_Alloc alloc;
  void *allocud;
  char *buf;
  size_t bufn, bufsize;
  Frame *frames;
  size_t depth, nframes, maxdepth;
  char initbuf[256];
  Frame initframes[32];
} Parser;

#define peek(P) ((P)->p < (P)->e ? *(P)->p : '\0')
#define tell(P) ((size_t)((P)->p - (P)->b))
#define isdigit_(c) ((c) >= '0' && (c) <= '9')

static int seterror (Parser *P, int error, size_t offset) {
  if (P->error == LJSON_OK) {
    P->error = error;
    P->erroffset = offset;
  }
  return 0;
}

static int consume (Parser *P, char c) {
  if (peek(P) == c) {
    P->p++;
    return 1;
  }
  return 0;
}

static int bufput (Parser *P, const char *s, size_t len) {
  if (P->bufsize - P->bufn < len) {
    void *block = P->buf;
    if (!growblock(P->alloc, P->allocud, &block, P->initbuf, &P->bufsize, P->bufn + len, 1)) return seterror(P, LJSON_ERROR_MEMORY, tell(P));
    P->buf = (char *)block;
  }
  memcpy(P->buf + P->bufn, s, len);
  P->bufn += len;
  return 1;
}

static void skipwhitespace (Parser *P) {
  while (P->p < P->e && (*P->p == ' ' || *P->p == '\n' || *P->p == '\r' || *P->p == '\t')) P->p++;
}

static int skipwhitespaceandcomments (Parser *P) {
  skipwhitespace(P);
  if (P->flags & LJSON_PARSE_COMMENTS) {
    while (consume(P, '/')) {
      if (consume(P, '*')) {
        for (;;) {
          if (peek(P) == '\0') return seterror(P, LJSON_ERROR_UNSPECIFIC_SYNTAX_ERROR, tell(P));
          else if (consume(P, '*')) {
            if (consume(P, '/')) break;
          }
          else P->p++;
        }
      }
      else if (consume(P, '/')) {
        while (peek(P) != '\0' && *P->p++ != '\n') {}
      }
      else return seterror(P, LJSON_ERROR_UNSPECIFIC_SYNTAX_ERROR, tell(P));
      skipwhitespace(P);
    }
  }
  return 1;
}

static int parseliteral (Parser *P, const char *rest, int what) {
  int ok;
  P->p++;
  for (; *rest; rest++) {
    if (!consume(P, *rest)) return seterror(P, LJSON_ERROR_VALUE_INVALID, tell(P));
  }
  switch (what) {
    case 'n': ok = P->h->null(P->ud); break;
    case 't': ok = P->h->boolean(P->ud, 1); break;
    default: ok = P->h->boolean(P->ud, 0); break;
  }
  if (!ok) return seterror(P, LJSON_ERROR_TERMINATION, tell(P));
  return 1;
}

static unsigned parsehex4 (Parser *P, size_t escoffset) {
  unsigned codepoint = 0;
  int i;
  for (i = 0; i < 4; i++) {
    char c = peek(P);
    codepoint <<= 4;
    if (c >= '0' && c <= '9') codepoint += (unsigned)(c - '0');
    else if (c >= 'A' && c <= 'F') codepoint += (unsigned)(c - 'A' + 10);
    else if (c >= 'a' && c <= 'f') codepoint += (unsigned)(c - 'a' + 10);
    else {
      seterror(P, LJSON_ERROR_STRING_UNICODE_ESCAPE_INVALID_HEX, escoffset);
      return 0;
    }
    P->p++;
  }
  return codepoint;
}

static int bufpututf8 (Parser *P, unsigned c) {
  char u[4];
  size_t n;
  if (c <= 0x7F) {
    u[0] = (char)c;
    n = 1;
  }
  else if (c <= 0x7FF) {
    u[0] = (char)(0xC0 | ((c >> 6) & 0xFF));
    u[1] = (char)(0x80 | (c & 0x3F));
    n = 2;
  }
  else if (c <= 0xFFFF) {
    u[0] = (char)(0xE0 | ((c >> 12) & 0xFF));
    u[1] = (char)(0x80 | ((c >> 6) & 0x3F));
    u[2] = (char)(0x80 | (c & 0x3F));
    n = 3;
  }
  else {
    u[0] = (char)(0xF0 | ((c >> 18) & 0xFF));
    u[1] = (char)(0x80 | ((c >> 12) & 0x3F));
    u[2] = (char)(0x80 | ((c >> 6) & 0x3F));
    u[3] = (char)(0x80 | (c & 0x3F));
    n = 4;
  }
  return bufput(P, u, n);
}

static const char unescape[256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, '"', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '/', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\\', 0, 0, 0,
  0, 0, '\b', 0, 0, 0, '\f', 0, 0, 0, 0, 0, 0, 0, '\n', 0, 0, 0, '\r', 0, '\t', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static int parsestring (Parser *P, int iskey) {
  const char *s;
  const char *str;
  size_t len;
  int ok;
  P->p++;
  s = P->p;
  for (;;) {
    unsigned char c = (P->p < P->e) ? (unsigned char)*P->p : 0;
    if (c == '"') {
      str = s;
      len = (size_t)(P->p - s);
      P->p++;
      goto submit;
    }
    if (c == '\\' || c < 0x20) break;
    P->p++;
  }
  P->bufn = 0;
  if (!bufput(P, s, (size_t)(P->p - s))) return 0;
  for (;;) {
    unsigned char c = (P->p < P->e) ? (unsigned char)*P->p : 0;
    if (c == '\\') {
      size_t escoffset = tell(P);
      unsigned char e;
      P->p++;
      e = (unsigned char)peek(P);
      if (unescape[e]) {
        P->p++;
        if (!bufput(P, &unescape[e], 1)) return 0;
      }
      else if (e == 'u') {
        unsigned codepoint;
        P->p++;
        codepoint = parsehex4(P, escoffset);
        if (P->error) return 0;
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
          if (codepoint <= 0xDBFF) {
            unsigned codepoint2;
            if (!consume(P, '\\') || !consume(P, 'u')) return seterror(P, LJSON_ERROR_STRING_UNICODE_SURROGATE_INVALID, escoffset);
            codepoint2 = parsehex4(P, escoffset);
            if (P->error) return 0;
            if (codepoint2 < 0xDC00 || codepoint2 > 0xDFFF) return seterror(P, LJSON_ERROR_STRING_UNICODE_SURROGATE_INVALID, escoffset);
            codepoint = (((codepoint - 0xD800) << 10) | (codepoint2 - 0xDC00)) + 0x10000;
          }
          else if (!(P->flags & LJSON_PARSE_LENIENT)) return seterror(P, LJSON_ERROR_STRING_UNICODE_SURROGATE_INVALID, escoffset);
        }
        if (!bufpututf8(P, codepoint)) return 0;
      }
      else return seterror(P, LJSON_ERROR_STRING_ESCAPE_INVALID, escoffset);
    }
    else if (c == '"') {
      P->p++;
      break;
    }
    else if (c < 0x20) {
      if (c == 0) return seterror(P, LJSON_ERROR_STRING_MISS_QUOTATION_MARK, tell(P));
      return seterror(P, LJSON_ERROR_STRING_INVALID_ENCODING, tell(P));
    }
    else {
      const char *run = P->p;
      while (P->p < P->e && *P->p != '"' && *P->p != '\\' && (unsigned char)*P->p >= 0x20) P->p++;
      if (!bufput(P, run, (size_t)(P->p - run))) return 0;
    }
  }
  str = P->buf;
  len = P->bufn;
submit:
  ok = iskey ? P->h->key(P->ud, str, len) : P->h->string(P->ud, str, len);
  if (!ok) return seterror(P, LJSON_ERROR_TERMINATION, tell(P));
  return 1;
}

static int takepush (Parser *P, int full) {
  char c = *P->p++;
  if (full) {
    if (!bufput(P, &c, 1)) return -1;
  }
  return c - '0';
}

static int parsenumber (Parser *P) {
  const int full = (P->flags & LJSON_PARSE_FULL_PRECISION) != 0;
  const int lenient = (P->flags & LJSON_PARSE_LENIENT) != 0;
  const size_t start = tell(P);
  double d = 0.0;
  int usenaninf = 0, minus, use64 = 0, usedouble = 0, sigdigits = 0, expfrac = 0, exp = 0, ok, t;
  unsigned i = 0;
  uint64_t i64 = 0;
  size_t decimalpos;
  P->bufn = 0;
  minus = consume(P, '-');
  if (peek(P) == '0') {
    if (takepush(P, full) < 0) return 0;
  }
  else if (peek(P) >= '1' && peek(P) <= '9') {
    if ((t = takepush(P, full)) < 0) return 0;
    i = (unsigned)t;
    if (minus) {
      while (isdigit_(peek(P))) {
        if (i >= 214748364) {
          if (i != 214748364 || peek(P) > '8') {
            i64 = i;
            use64 = 1;
            break;
          }
        }
        if ((t = takepush(P, full)) < 0) return 0;
        i = i * 10 + (unsigned)t;
        sigdigits++;
      }
    }
    else {
      while (isdigit_(peek(P))) {
        if (i >= 429496729) {
          if (i != 429496729 || peek(P) > '5') {
            i64 = i;
            use64 = 1;
            break;
          }
        }
        if ((t = takepush(P, full)) < 0) return 0;
        i = i * 10 + (unsigned)t;
        sigdigits++;
      }
    }
  }
  else if ((P->flags & LJSON_PARSE_NAN_INF) && (peek(P) == 'I' || peek(P) == 'N' || (lenient && peek(P) == 'i'))) {
    if (consume(P, 'N')) {
      if (consume(P, 'a') && consume(P, 'N')) {
        d = dfrombits(UINT64_C(0x7FF8000000000000));
        usenaninf = 1;
      }
    }
    else if (consume(P, 'I') || (lenient && consume(P, 'i'))) {
      if (consume(P, 'n') && consume(P, 'f')) {
        d = dfrombits(minus ? DP_SIGN_MASK | DP_EXPONENT_MASK : DP_EXPONENT_MASK);
        usenaninf = 1;
        if (peek(P) == 'i' && !(consume(P, 'i') && consume(P, 'n') && consume(P, 'i') && consume(P, 't') && consume(P, 'y'))) return seterror(P, LJSON_ERROR_VALUE_INVALID, tell(P));
      }
    }
    if (!usenaninf) return seterror(P, LJSON_ERROR_VALUE_INVALID, tell(P));
  }
  else return seterror(P, LJSON_ERROR_VALUE_INVALID, tell(P));
  if (use64) {
    if (minus) {
      while (isdigit_(peek(P))) {
        if (i64 >= UINT64_C(0x0CCCCCCCCCCCCCCC)) {
          if (i64 != UINT64_C(0x0CCCCCCCCCCCCCCC) || peek(P) > '8') {
            d = (double)i64;
            usedouble = 1;
            break;
          }
        }
        if ((t = takepush(P, full)) < 0) return 0;
        i64 = i64 * 10 + (unsigned)t;
        sigdigits++;
      }
    }
    else {
      while (isdigit_(peek(P))) {
        if (i64 >= UINT64_C(0x1999999999999999)) {
          if (i64 != UINT64_C(0x1999999999999999) || peek(P) > '5') {
            d = (double)i64;
            usedouble = 1;
            break;
          }
        }
        if ((t = takepush(P, full)) < 0) return 0;
        i64 = i64 * 10 + (unsigned)t;
        sigdigits++;
      }
    }
  }
  if (usedouble) {
    while (isdigit_(peek(P))) {
      if ((t = takepush(P, full)) < 0) return 0;
      d = d * 10 + t;
    }
  }
  if ((!usenaninf || lenient) && consume(P, '.')) {
    decimalpos = P->bufn;
    if (!isdigit_(peek(P))) return seterror(P, LJSON_ERROR_NUMBER_MISS_FRACTION, tell(P));
    if (!usedouble) {
      if (!use64) i64 = i;
      while (isdigit_(peek(P))) {
        if (i64 > UINT64_C(0x1FFFFFFFFFFFFF)) break;
        if ((t = takepush(P, full)) < 0) return 0;
        i64 = i64 * 10 + (unsigned)t;
        --expfrac;
        if (i64 != 0) sigdigits++;
      }
      d = (double)i64;
      usedouble = 1;
    }
    while (isdigit_(peek(P))) {
      if (sigdigits < 17) {
        if ((t = takepush(P, full)) < 0) return 0;
        d = d * 10.0 + t;
        --expfrac;
        if (d > 0.0) sigdigits++;
      }
      else if (takepush(P, full) < 0) return 0;
    }
  }
  else decimalpos = P->bufn;
  if ((!usenaninf || lenient) && (consume(P, 'e') || consume(P, 'E'))) {
    int expminus = 0;
    if (!usedouble) {
      d = (double)(use64 ? i64 : i);
      usedouble = 1;
    }
    if (consume(P, '+')) {}
    else if (consume(P, '-')) expminus = 1;
    if (isdigit_(peek(P))) {
      exp = *P->p++ - '0';
      if (expminus) {
        int maxexp = (expfrac + 2147483639) / 10;
        while (isdigit_(peek(P))) {
          exp = exp * 10 + (*P->p++ - '0');
          if (exp > maxexp) {
            while (isdigit_(peek(P))) P->p++;
          }
        }
      }
      else {
        int maxexp = 308 - expfrac;
        while (isdigit_(peek(P))) {
          exp = (int)((unsigned)exp * 10u + (unsigned)(*P->p++ - '0'));
          if (exp > maxexp) return seterror(P, LJSON_ERROR_NUMBER_TOO_BIG, start);
        }
      }
    }
    else return seterror(P, LJSON_ERROR_NUMBER_MISS_EXPONENT, tell(P));
    if (expminus) exp = -exp;
  }
  if (usedouble) {
    const int p = exp + expfrac;
    if (full) d = strtodfull(d, p, P->buf, P->bufn, decimalpos, exp);
    else d = strtodnormal(d, p);
    if (d > DBL_MAX) return seterror(P, LJSON_ERROR_NUMBER_TOO_BIG, start);
    ok = P->h->number(P->ud, minus ? -d : d);
  }
  else if (usenaninf) ok = P->h->number(P->ud, d);
  else if (use64) ok = minus ? P->h->integer(P->ud, (int64_t)(~i64 + 1)) : P->h->uinteger(P->ud, i64);
  else ok = minus ? P->h->integer(P->ud, (int32_t)(~i + 1)) : P->h->uinteger(P->ud, i);
  if (!ok) return seterror(P, LJSON_ERROR_TERMINATION, start);
  return 1;
}

static int parsevalue (Parser *P) {
  switch (peek(P)) {
    case 'n': return parseliteral(P, "ull", 'n');
    case 't': return parseliteral(P, "rue", 't');
    case 'f': return parseliteral(P, "alse", 'f');
    case '"': return parsestring(P, 0);
    default: return parsenumber(P);
  }
}

static int tokenize (char c) {
  switch (c) {
    case '[': return T_LEFTBRACKET;
    case ']': return T_RIGHTBRACKET;
    case '{': return T_LEFTCURLY;
    case '}': return T_RIGHTCURLY;
    case ',': return T_COMMA;
    case ':': return T_COLON;
    case '"': return T_STRING;
    case 'f': return T_FALSE;
    case 't': return T_TRUE;
    case 'n': return T_NULL;
    default: return T_NUMBER;
  }
}

static int transit (Parser *P, int src, int dst) {
  switch (dst) {
    case S_ERROR: return dst;
    case S_OBJECTINITIAL: case S_ARRAYINITIAL: {
      int n = src, ok;
      if (src == S_ARRAYINITIAL || src == S_ELEMENTDELIMITER) n = S_ELEMENT;
      else if (src == S_KEYVALUEDELIMITER) n = S_MEMBERVALUE;
      if (P->maxdepth > 0 && P->depth >= P->maxdepth) {
        seterror(P, LJSON_ERROR_DEPTH, tell(P));
        return S_ERROR;
      }
      if (P->depth == P->nframes) {
        void *block = P->frames;
        if (!growblock(P->alloc, P->allocud, &block, P->initframes, &P->nframes, P->depth + 1, sizeof(Frame))) {
          seterror(P, LJSON_ERROR_MEMORY, tell(P));
          return S_ERROR;
        }
        P->frames = (Frame *)block;
      }
      P->frames[P->depth].state = n;
      P->frames[P->depth].count = 0;
      P->depth++;
      ok = (dst == S_OBJECTINITIAL) ? P->h->start_object(P->ud) : P->h->start_array(P->ud);
      if (!ok) {
        seterror(P, LJSON_ERROR_TERMINATION, tell(P));
        return S_ERROR;
      }
      P->p++;
      return dst;
    }
    case S_MEMBERKEY:
      return parsestring(P, 1) ? dst : S_ERROR;
    case S_KEYVALUEDELIMITER:
      P->p++;
      return dst;
    case S_MEMBERVALUE: case S_ELEMENT:
      return parsevalue(P) ? dst : S_ERROR;
    case S_MEMBERDELIMITER: case S_ELEMENTDELIMITER:
      P->p++;
      P->frames[P->depth - 1].count++;
      return dst;
    case S_OBJECTFINISH: case S_ARRAYFINISH: {
      Frame *f;
      size_t c;
      int n, ok;
      if (!(P->flags & LJSON_PARSE_TRAILING_COMMAS) && src == (dst == S_OBJECTFINISH ? S_MEMBERDELIMITER : S_ELEMENTDELIMITER)) {
        seterror(P, dst == S_OBJECTFINISH ? LJSON_ERROR_OBJECT_MISS_NAME : LJSON_ERROR_VALUE_INVALID, tell(P));
        return S_ERROR;
      }
      f = &P->frames[--P->depth];
      c = f->count;
      if (src == S_MEMBERVALUE || src == S_ELEMENT) ++c;
      n = f->state;
      if (n == S_START) n = S_FINISH;
      ok = (dst == S_OBJECTFINISH) ? P->h->end_object(P->ud, c) : P->h->end_array(P->ud, c);
      if (!ok) {
        seterror(P, LJSON_ERROR_TERMINATION, tell(P));
        return S_ERROR;
      }
      P->p++;
      return n;
    }
    default:
      return parsevalue(P) ? S_FINISH : S_ERROR;
  }
}

static void handleerror (Parser *P, int src) {
  if (P->error) return;
  switch (src) {
    case S_START: seterror(P, LJSON_ERROR_DOCUMENT_EMPTY, tell(P)); return;
    case S_FINISH: seterror(P, LJSON_ERROR_DOCUMENT_ROOT_NOT_SINGULAR, tell(P)); return;
    case S_OBJECTINITIAL: case S_MEMBERDELIMITER: seterror(P, LJSON_ERROR_OBJECT_MISS_NAME, tell(P)); return;
    case S_MEMBERKEY: seterror(P, LJSON_ERROR_OBJECT_MISS_COLON, tell(P)); return;
    case S_MEMBERVALUE: seterror(P, LJSON_ERROR_OBJECT_MISS_COMMA_OR_CURLY_BRACKET, tell(P)); return;
    case S_KEYVALUEDELIMITER: case S_ARRAYINITIAL: case S_ELEMENTDELIMITER: seterror(P, LJSON_ERROR_VALUE_INVALID, tell(P)); return;
    default: seterror(P, LJSON_ERROR_ARRAY_MISS_COMMA_OR_SQUARE_BRACKET, tell(P)); return;
  }
}

int ljson_parse (const ljson_Reader *r, const char *s, size_t len, const ljson_Handler *h, void *ud, size_t *offset) {
  Parser P;
  int state = S_START;
  P.b = P.p = s;
  P.e = s + len;
  P.flags = r->flags;
  P.error = LJSON_OK;
  P.erroffset = 0;
  P.h = h;
  P.ud = ud;
  P.alloc = (r->alloc != NULL) ? r->alloc : defaultalloc;
  P.allocud = r->allocud;
  P.buf = P.initbuf;
  P.bufn = 0;
  P.bufsize = sizeof(P.initbuf);
  P.frames = P.initframes;
  P.depth = 0;
  P.nframes = sizeof(P.initframes) / sizeof(P.initframes[0]);
  P.maxdepth = r->maxdepth;
  if (skipwhitespaceandcomments(&P)) {
    while (peek(&P) != '\0') {
      int d = transit(&P, state, predict[state][tokenize(peek(&P))]);
      if (d == S_ERROR) {
        handleerror(&P, state);
        break;
      }
      state = d;
      if ((P.flags & LJSON_PARSE_STOP_WHEN_DONE) && state == S_FINISH) break;
      if (!skipwhitespaceandcomments(&P)) break;
    }
    if (state != S_FINISH) handleerror(&P, state);
  }
  freeblock(P.alloc, P.allocud, P.buf, P.initbuf, P.bufsize, 1);
  freeblock(P.alloc, P.allocud, P.frames, P.initframes, P.nframes, sizeof(Frame));
  *offset = (P.error != LJSON_OK) ? P.erroffset : tell(&P);
  return P.error;
}

const char *ljson_errormessage (int error) {
  switch (error) {
    case LJSON_OK: return "No error.";
    case LJSON_ERROR_DOCUMENT_EMPTY: return "The document is empty.";
    case LJSON_ERROR_DOCUMENT_ROOT_NOT_SINGULAR: return "The document root must not be followed by other values.";
    case LJSON_ERROR_VALUE_INVALID: return "Invalid value.";
    case LJSON_ERROR_OBJECT_MISS_NAME: return "Missing a name for object member.";
    case LJSON_ERROR_OBJECT_MISS_COLON: return "Missing a colon after a name of object member.";
    case LJSON_ERROR_OBJECT_MISS_COMMA_OR_CURLY_BRACKET: return "Missing a comma or '}' after an object member.";
    case LJSON_ERROR_ARRAY_MISS_COMMA_OR_SQUARE_BRACKET: return "Missing a comma or ']' after an array element.";
    case LJSON_ERROR_STRING_UNICODE_ESCAPE_INVALID_HEX: return "Incorrect hex digit after \\u escape in string.";
    case LJSON_ERROR_STRING_UNICODE_SURROGATE_INVALID: return "The surrogate pair in string is invalid.";
    case LJSON_ERROR_STRING_ESCAPE_INVALID: return "Invalid escape character in string.";
    case LJSON_ERROR_STRING_MISS_QUOTATION_MARK: return "Missing a closing quotation mark in string.";
    case LJSON_ERROR_STRING_INVALID_ENCODING: return "Invalid encoding in string.";
    case LJSON_ERROR_NUMBER_TOO_BIG: return "Number too big to be stored in double.";
    case LJSON_ERROR_NUMBER_MISS_FRACTION: return "Miss fraction part in number.";
    case LJSON_ERROR_NUMBER_MISS_EXPONENT: return "Miss exponent in number.";
    case LJSON_ERROR_TERMINATION: return "Terminate parsing due to Handler error.";
    case LJSON_ERROR_UNSPECIFIC_SYNTAX_ERROR: return "Unspecific syntax error.";
    case LJSON_ERROR_DEPTH: return "Nesting too deep.";
    case LJSON_ERROR_MEMORY: return "Not enough memory.";
    default: return "Unknown error.";
  }
}

void ljson_writerinit (ljson_Writer *w, ljson_Alloc alloc, void *allocud) {
  w->flags = 0;
  w->maxdecimals = LJSON_MAXDECIMALS;
  w->indentchar = ' ';
  w->indentcount = 4;
  w->b = w->initb;
  w->n = 0;
  w->size = sizeof(w->initb);
  w->levels = w->initlevels;
  w->depth = 0;
  w->nlevels = sizeof(w->initlevels) / sizeof(w->initlevels[0]);
  w->hasroot = 0;
  w->nomem = 0;
  w->alloc = (alloc != NULL) ? alloc : defaultalloc;
  w->allocud = allocud;
}

void ljson_writerfree (ljson_Writer *w) {
  freeblock(w->alloc, w->allocud, w->b, w->initb, w->size, 1);
  freeblock(w->alloc, w->allocud, w->levels, w->initlevels, w->nlevels, sizeof(ljson_Level));
  w->b = w->initb;
  w->size = sizeof(w->initb);
  w->levels = w->initlevels;
  w->nlevels = sizeof(w->initlevels) / sizeof(w->initlevels[0]);
  w->n = w->depth = 0;
}

const char *ljson_result (const ljson_Writer *w, size_t *len) {
  *len = w->nomem ? 0 : w->n;
  return w->nomem ? NULL : w->b;
}

static int reserve (ljson_Writer *w, size_t k) {
  if (w->nomem) return 0;
  if (w->size - w->n < k) {
    void *block = w->b;
    if (k > (size_t)-1 - w->n || !growblock(w->alloc, w->allocud, &block, w->initb, &w->size, w->n + k, 1)) {
      w->nomem = 1;
      return 0;
    }
    w->b = (char *)block;
  }
  return 1;
}

static void put (ljson_Writer *w, char c) {
  if (reserve(w, 1)) w->b[w->n++] = c;
}

static void putn (ljson_Writer *w, const char *s, size_t len) {
  if (reserve(w, len)) {
    memcpy(w->b + w->n, s, len);
    w->n += len;
  }
}

static void writeindent (ljson_Writer *w) {
  size_t count = w->depth * w->indentcount;
  if (reserve(w, count)) {
    memset(w->b + w->n, w->indentchar, count);
    w->n += count;
  }
}

static void prefix (ljson_Writer *w) {
  ljson_Level *level;
  if (w->depth == 0) {
    w->hasroot = 1;
    return;
  }
  level = &w->levels[w->depth - 1];
  if (!(w->flags & LJSON_WRITE_PRETTY)) {
    if (level->count > 0) put(w, (level->inarray || level->count % 2 == 0) ? ',' : ':');
  }
  else if (level->inarray) {
    if (level->count > 0) {
      put(w, ',');
      if (w->flags & LJSON_WRITE_SINGLE_LINE_ARRAY) put(w, ' ');
    }
    if (!(w->flags & LJSON_WRITE_SINGLE_LINE_ARRAY)) {
      put(w, '\n');
      writeindent(w);
    }
  }
  else {
    if (level->count > 0) {
      if (level->count % 2 == 0) putn(w, ",\n", 2);
      else putn(w, ": ", 2);
    }
    else put(w, '\n');
    if (level->count % 2 == 0) writeindent(w);
  }
  level->count++;
}

static int done (ljson_Writer *w) { return !w->nomem; }

int ljson_null (ljson_Writer *w) {
  prefix(w);
  putn(w, "null", 4);
  return done(w);
}

int ljson_bool (ljson_Writer *w, int b) {
  prefix(w);
  if (b) putn(w, "true", 4);
  else putn(w, "false", 5);
  return done(w);
}

int ljson_int (ljson_Writer *w, int64_t i) {
  char buff[21];
  prefix(w);
  putn(w, buff, (size_t)(ljson_i64toa(i, buff) - buff));
  return done(w);
}

int ljson_uint (ljson_Writer *w, uint64_t u) {
  char buff[20];
  prefix(w);
  putn(w, buff, (size_t)(ljson_u64toa(u, buff) - buff));
  return done(w);
}

int ljson_double (ljson_Writer *w, double d) {
  uint64_t u = dbits(d);
  prefix(w);
  if ((u & DP_EXPONENT_MASK) == DP_EXPONENT_MASK) {
    if (!(w->flags & LJSON_WRITE_NAN_INF)) return 0;
    if (u & DP_SIGNIFICAND_MASK) putn(w, "NaN", 3);
    else if (u & DP_SIGN_MASK) putn(w, "-Infinity", 9);
    else putn(w, "Infinity", 8);
  }
  else {
    char buff[32];
    putn(w, buff, (size_t)(ljson_dtoa(d, buff, w->maxdecimals) - buff));
  }
  return done(w);
}

int ljson_string (ljson_Writer *w, const char *s, size_t len) {
  static const char hexdigits[] = "0123456789ABCDEF";
  static const char escape[256] = {
    'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'b', 't', 'n', 'u', 'f', 'r', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u',
    0, 0, '"', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\\', 0, 0, 0,
  };
  size_t i = 0;
  prefix(w);
  if (len > ((size_t)-1 - 2) / 6 || !reserve(w, 2 + len * 6)) {
    w->nomem = 1;
    return 0;
  }
  w->b[w->n++] = '"';
  while (i < len) {
    size_t run = i;
    unsigned char c;
    while (i < len && !escape[(unsigned char)s[i]]) i++;
    memcpy(w->b + w->n, s + run, i - run);
    w->n += i - run;
    if (i == len) break;
    c = (unsigned char)s[i++];
    w->b[w->n++] = '\\';
    w->b[w->n++] = escape[c];
    if (escape[c] == 'u') {
      w->b[w->n++] = '0';
      w->b[w->n++] = '0';
      w->b[w->n++] = hexdigits[c >> 4];
      w->b[w->n++] = hexdigits[c & 0xF];
    }
  }
  w->b[w->n++] = '"';
  return 1;
}

int ljson_key (ljson_Writer *w, const char *s, size_t len) { return ljson_string(w, s, len); }

static int startlevel (ljson_Writer *w, int inarray, char c) {
  prefix(w);
  if (w->depth == w->nlevels) {
    void *block = w->levels;
    if (!growblock(w->alloc, w->allocud, &block, w->initlevels, &w->nlevels, w->depth + 1, sizeof(ljson_Level))) {
      w->nomem = 1;
      return 0;
    }
    w->levels = (ljson_Level *)block;
  }
  w->levels[w->depth].count = 0;
  w->levels[w->depth].inarray = inarray;
  w->depth++;
  put(w, c);
  return done(w);
}

static int endlevel (ljson_Writer *w, int inarray, char c) {
  int empty;
  if (w->depth == 0) return 0;
  empty = w->levels[--w->depth].count == 0;
  if ((w->flags & LJSON_WRITE_PRETTY) && !empty && (!inarray || !(w->flags & LJSON_WRITE_SINGLE_LINE_ARRAY))) {
    put(w, '\n');
    writeindent(w);
  }
  put(w, c);
  return done(w);
}

int ljson_startobject (ljson_Writer *w) { return startlevel(w, 0, '{'); }

int ljson_endobject (ljson_Writer *w) { return endlevel(w, 0, '}'); }

int ljson_startarray (ljson_Writer *w) { return startlevel(w, 1, '['); }

int ljson_endarray (ljson_Writer *w) { return endlevel(w, 1, ']'); }

int ljson_raw (ljson_Writer *w, const char *s, size_t len) {
  prefix(w);
  putn(w, s, len);
  return done(w);
}

