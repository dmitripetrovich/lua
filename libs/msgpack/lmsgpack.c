#include <stdlib.h>
#include <string.h>

#include "lmsgpack.h"

static void store16 (unsigned char *p, uint64_t v) {
  p[0] = (unsigned char)(v >> 8);
  p[1] = (unsigned char)v;
}

static void store32 (unsigned char *p, uint64_t v) {
  p[0] = (unsigned char)(v >> 24);
  p[1] = (unsigned char)(v >> 16);
  p[2] = (unsigned char)(v >> 8);
  p[3] = (unsigned char)v;
}

static void store64 (unsigned char *p, uint64_t v) {
  store32(p, v >> 32);
  store32(p + 4, v);
}

static uint32_t load16 (const unsigned char *p) {
  return ((uint32_t)p[0] << 8) | p[1];
}

static uint32_t load32 (const unsigned char *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint64_t load64 (const unsigned char *p) {
  return ((uint64_t)load32(p) << 32) | load32(p + 4);
}

static int64_t tosigned (uint64_t u, int bits) {
  uint64_t sign = (uint64_t)1 << (bits - 1);
  u &= (sign << 1) - 1;
  return (u & sign) ? -(int64_t)((sign << 1) - 1 - u) - 1 : (int64_t)u;
}

#if SIZE_MAX > 0xffffffffu
  #define toolarge(n) ((n) > 0xffffffffu)
#else
  #define toolarge(n) 0
#endif

static int fail (lmsgpack_writer *w, lmsgpack_status s) {
  if (w->status == LMSGPACK_OK)
    w->status = s;
  return -1;
}

void lmsgpack_writer_init (lmsgpack_writer *w, char *buf, size_t cap, lmsgpack_reserve reserve, void *ud) {
  w->buf = buf;
  w->len = 0;
  w->cap = (buf != NULL) ? cap : 0;
  w->reserve = reserve;
  w->ud = ud;
  w->status = LMSGPACK_OK;
}

static int bufreserve (lmsgpack_writer *w, size_t need) {
  size_t size = (w->cap == 0) ? 256 : (w->cap <= SIZE_MAX / 2) ? w->cap * 2 : SIZE_MAX;
  char *b;
  if (need > SIZE_MAX - w->len)
    return -1;
  if (size < w->len + need)
    size = w->len + need;
  if ((b = (char *)realloc(w->buf, size)) == NULL)
    return -1;
  w->buf = b;
  w->cap = size;
  return 0;
}

void lmsgpack_buffer_init (lmsgpack_writer *w) {
  lmsgpack_writer_init(w, NULL, 0, bufreserve, NULL);
}

void lmsgpack_buffer_free (lmsgpack_writer *w) {
  free(w->buf);
  w->buf = NULL;
  w->len = w->cap = 0;
}

char *lmsgpack_writer_reserve (lmsgpack_writer *w, size_t n) {
  if (w->cap - w->len >= n)
    return w->buf + w->len;
  if (w->reserve == NULL || w->reserve(w, n) != 0 || w->cap - w->len < n) {
    fail(w, LMSGPACK_NOMEM);
    return NULL;
  }
  return w->buf + w->len;
}

#define room(w, n) (((w)->cap - (w)->len >= (n)) ? (unsigned char *)(w)->buf + (w)->len : (unsigned char *)lmsgpack_writer_reserve((w), (n)))

static int put (lmsgpack_writer *w, unsigned int c, size_t size, uint64_t v) {
  unsigned char *p = room(w, 1 + size);
  if (p == NULL)
    return -1;
  p[0] = (unsigned char)c;
  switch (size) {
    case 1: p[1] = (unsigned char)v; break;
    case 2: store16(p + 1, v); break;
    case 4: store32(p + 1, v); break;
    case 8: store64(p + 1, v); break;
    default: break;
  }
  w->len += 1 + size;
  return 0;
}

int lmsgpack_write_nil (lmsgpack_writer *w) {
  return put(w, 0xc0, 0, 0);
}

int lmsgpack_write_true (lmsgpack_writer *w) {
  return put(w, 0xc3, 0, 0);
}

int lmsgpack_write_false (lmsgpack_writer *w) {
  return put(w, 0xc2, 0, 0);
}

int lmsgpack_write_bool (lmsgpack_writer *w, int b) {
  return put(w, b ? 0xc3 : 0xc2, 0, 0);
}

int lmsgpack_write_uint (lmsgpack_writer *w, uint64_t v) {
  if (v < 0x80)
    return put(w, (unsigned int)v, 0, 0);
  else if (v < 0x100)
    return put(w, 0xcc, 1, v);
  else if (v < 0x10000)
    return put(w, 0xcd, 2, v);
  else if (v < 0x100000000)
    return put(w, 0xce, 4, v);
  return put(w, 0xcf, 8, v);
}

static int putnegative (lmsgpack_writer *w, int64_t v) {
  if (v < -0x80000000LL)
    return put(w, 0xd3, 8, (uint64_t)v);
  else if (v < -0x8000)
    return put(w, 0xd2, 4, (uint64_t)v);
  else if (v < -0x80)
    return put(w, 0xd1, 2, (uint64_t)v);
  return put(w, 0xd0, 1, (uint64_t)v);
}

int lmsgpack_write_int (lmsgpack_writer *w, int64_t v) {
  if (v < -32)
    return putnegative(w, v);
  else if (v < 0x80)
    return put(w, (unsigned int)(v & 0xff), 0, 0);
  return lmsgpack_write_uint(w, (uint64_t)v);
}

int lmsgpack_write_sint (lmsgpack_writer *w, int64_t v) {
  if (v < -32)
    return putnegative(w, v);
  else if (v < 0x80)
    return put(w, (unsigned int)(v & 0xff), 0, 0);
  else if (v < 0x8000)
    return put(w, 0xd1, 2, (uint64_t)v);
  else if (v < 0x80000000LL)
    return put(w, 0xd2, 4, (uint64_t)v);
  return put(w, 0xd3, 8, (uint64_t)v);
}

int lmsgpack_write_fixint (lmsgpack_writer *w, int size, int issigned, uint64_t v) {
  switch (size) {
    case 1: return put(w, issigned ? 0xd0 : 0xcc, 1, v);
    case 2: return put(w, issigned ? 0xd1 : 0xcd, 2, v);
    case 4: return put(w, issigned ? 0xd2 : 0xce, 4, v);
    case 8: return put(w, issigned ? 0xd3 : 0xcf, 8, v);
    default: return fail(w, LMSGPACK_INVALID);
  }
}

int lmsgpack_write_float (lmsgpack_writer *w, float v) {
  uint32_t i;
  memcpy(&i, &v, sizeof(i));
  return put(w, 0xca, 4, i);
}

int lmsgpack_write_double (lmsgpack_writer *w, double v) {
  uint64_t i;
  memcpy(&i, &v, sizeof(i));
  return put(w, 0xcb, 8, i);
}

static int putlength (lmsgpack_writer *w, size_t n, unsigned int fix, size_t fixmax, unsigned int tag8, unsigned int tag16, unsigned int tag32) {
  if (toolarge(n))
    return fail(w, LMSGPACK_TOO_LARGE);
  else if (n < fixmax)
    return put(w, fix | (unsigned int)n, 0, 0);
  else if (n < 0x100 && tag8 != 0)
    return put(w, tag8, 1, n);
  else if (n < 0x10000)
    return put(w, tag16, 2, n);
  return put(w, tag32, 4, n);
}

int lmsgpack_write_array (lmsgpack_writer *w, size_t n) {
  return putlength(w, n, 0x90, 16, 0, 0xdc, 0xdd);
}

int lmsgpack_write_map (lmsgpack_writer *w, size_t n) {
  return putlength(w, n, 0x80, 16, 0, 0xde, 0xdf);
}

int lmsgpack_write_str (lmsgpack_writer *w, size_t n) {
  return putlength(w, n, 0xa0, 32, 0xd9, 0xda, 0xdb);
}

int lmsgpack_write_str_compat (lmsgpack_writer *w, size_t n) {
  return putlength(w, n, 0xa0, 32, 0, 0xda, 0xdb);
}

int lmsgpack_write_bin (lmsgpack_writer *w, size_t n) {
  return putlength(w, n, 0, 0, 0xc4, 0xc5, 0xc6);
}

int lmsgpack_write_ext (lmsgpack_writer *w, size_t n, int8_t type) {
  unsigned char *p;
  size_t size;
  unsigned int tag;
  if (toolarge(n))
    return fail(w, LMSGPACK_TOO_LARGE);
  switch (n) {
    case 1: tag = 0xd4; size = 0; break;
    case 2: tag = 0xd5; size = 0; break;
    case 4: tag = 0xd6; size = 0; break;
    case 8: tag = 0xd7; size = 0; break;
    case 16: tag = 0xd8; size = 0; break;
    default: tag = (n < 0x100) ? 0xc7 : (n < 0x10000) ? 0xc8 : 0xc9; size = (n < 0x100) ? 1 : (n < 0x10000) ? 2 : 4; break;
  }
  if ((p = room(w, 2 + size)) == NULL)
    return -1;
  p[0] = (unsigned char)tag;
  switch (size) {
    case 1: p[1] = (unsigned char)n; break;
    case 2: store16(p + 1, n); break;
    case 4: store32(p + 1, n); break;
    default: break;
  }
  p[1 + size] = (unsigned char)type;
  w->len += 2 + size;
  return 0;
}

int lmsgpack_write_bytes (lmsgpack_writer *w, const void *s, size_t n) {
  unsigned char *p;
  if (n == 0)
    return 0;
  if ((p = room(w, n)) == NULL)
    return -1;
  memcpy(p, s, n);
  w->len += n;
  return 0;
}

int lmsgpack_write_object (lmsgpack_writer *w, const lmsgpack_object *o) {
  uint32_t i;
  switch (o->type) {
    case LMSGPACK_NIL: return lmsgpack_write_nil(w);
    case LMSGPACK_BOOLEAN: return lmsgpack_write_bool(w, o->via.boolean);
    case LMSGPACK_POSITIVE_INTEGER: return lmsgpack_write_uint(w, o->via.u64);
    case LMSGPACK_NEGATIVE_INTEGER: return lmsgpack_write_int(w, o->via.i64);
    case LMSGPACK_FLOAT32: return lmsgpack_write_float(w, (float)o->via.f64);
    case LMSGPACK_FLOAT64: return lmsgpack_write_double(w, o->via.f64);
    case LMSGPACK_STR: return (lmsgpack_write_str(w, o->via.str.size) != 0) ? -1 : lmsgpack_write_bytes(w, o->via.str.ptr, o->via.str.size);
    case LMSGPACK_BIN: return (lmsgpack_write_bin(w, o->via.bin.size) != 0) ? -1 : lmsgpack_write_bytes(w, o->via.bin.ptr, o->via.bin.size);
    case LMSGPACK_EXT: return (lmsgpack_write_ext(w, o->via.ext.size, o->via.ext.type) != 0) ? -1 : lmsgpack_write_bytes(w, o->via.ext.ptr, o->via.ext.size);
    case LMSGPACK_ARRAY: {
      if (lmsgpack_write_array(w, o->via.array.size) != 0)
        return -1;
      for (i = 0; i < o->via.array.size; i++) {
        if (lmsgpack_write_object(w, &o->via.array.ptr[i]) != 0)
          return -1;
      }
      return 0;
    }
    case LMSGPACK_MAP: {
      if (lmsgpack_write_map(w, o->via.map.size) != 0)
        return -1;
      for (i = 0; i < o->via.map.size; i++) {
        if (lmsgpack_write_object(w, &o->via.map.ptr[i].key) != 0 || lmsgpack_write_object(w, &o->via.map.ptr[i].val) != 0)
          return -1;
      }
      return 0;
    }
    default: return fail(w, LMSGPACK_INVALID);
  }
}

static lmsgpack_status readbody (const unsigned char *s, size_t avail, size_t p, uint64_t n, lmsgpack_type type, lmsgpack_object *o, size_t *end) {
  *end = p;
  if (avail < n)
    return LMSGPACK_INCOMPLETE;
  o->type = type;
  if (type == LMSGPACK_EXT) {
    o->via.ext.type = (int8_t)tosigned(s[p], 8);
    o->via.ext.size = (uint32_t)(n - 1);
    o->via.ext.ptr = (const char *)s + p + 1;
  }
  else if (type == LMSGPACK_BIN) {
    o->via.bin.size = (uint32_t)n;
    o->via.bin.ptr = (const char *)s + p;
  }
  else {
    o->via.str.size = (uint32_t)n;
    o->via.str.ptr = (const char *)s + p;
  }
  *end = p + (size_t)n;
  return LMSGPACK_OK;
}

static void setint (lmsgpack_object *o, int64_t i) {
  if (i >= 0) {
    o->type = LMSGPACK_POSITIVE_INTEGER;
    o->via.u64 = (uint64_t)i;
  }
  else {
    o->type = LMSGPACK_NEGATIVE_INTEGER;
    o->via.i64 = i;
  }
}

static void setcontainer (lmsgpack_object *o, lmsgpack_type type, uint32_t n) {
  o->type = type;
  o->via.array.size = n;
  o->via.array.ptr = NULL;
}

static lmsgpack_status readitem (const unsigned char *s, size_t len, size_t p, size_t *end, lmsgpack_object *o) {
  unsigned int c;
  size_t avail;
  *end = p;
  if (p >= len)
    return LMSGPACK_INCOMPLETE;
  c = s[p++];
  *end = p;
  avail = len - p;
  if (c <= 0x7f) {
    o->type = LMSGPACK_POSITIVE_INTEGER;
    o->via.u64 = c;
    return LMSGPACK_OK;
  }
  else if (c >= 0xe0) {
    o->type = LMSGPACK_NEGATIVE_INTEGER;
    o->via.i64 = (int64_t)c - 0x100;
    return LMSGPACK_OK;
  }
  else if (c <= 0x8f) {
    setcontainer(o, LMSGPACK_MAP, c & 0x0f);
    return LMSGPACK_OK;
  }
  else if (c <= 0x9f) {
    setcontainer(o, LMSGPACK_ARRAY, c & 0x0f);
    return LMSGPACK_OK;
  }
  else if (c <= 0xbf)
    return readbody(s, avail, p, c & 0x1f, LMSGPACK_STR, o, end);
  #define need(n) if (avail < (n)) return LMSGPACK_INCOMPLETE; *end = p + (n)
  switch (c) {
    case 0xc0: o->type = LMSGPACK_NIL; return LMSGPACK_OK;
    case 0xc2: case 0xc3: o->type = LMSGPACK_BOOLEAN; o->via.boolean = (c == 0xc3); return LMSGPACK_OK;
    case 0xc4: need(1); return readbody(s, avail - 1, p + 1, s[p], LMSGPACK_BIN, o, end);
    case 0xc5: need(2); return readbody(s, avail - 2, p + 2, load16(s + p), LMSGPACK_BIN, o, end);
    case 0xc6: need(4); return readbody(s, avail - 4, p + 4, load32(s + p), LMSGPACK_BIN, o, end);
    case 0xc7: need(1); return readbody(s, avail - 1, p + 1, (uint64_t)s[p] + 1, LMSGPACK_EXT, o, end);
    case 0xc8: need(2); return readbody(s, avail - 2, p + 2, (uint64_t)load16(s + p) + 1, LMSGPACK_EXT, o, end);
    case 0xc9: need(4); return readbody(s, avail - 4, p + 4, (uint64_t)load32(s + p) + 1, LMSGPACK_EXT, o, end);
    case 0xca: {
      uint32_t i;
      float f;
      need(4);
      i = load32(s + p);
      memcpy(&f, &i, sizeof(f));
      o->type = LMSGPACK_FLOAT32;
      o->via.f64 = f;
      return LMSGPACK_OK;
    }
    case 0xcb: {
      uint64_t i;
      double d;
      need(8);
      i = load64(s + p);
      memcpy(&d, &i, sizeof(d));
      o->type = LMSGPACK_FLOAT64;
      o->via.f64 = d;
      return LMSGPACK_OK;
    }
    case 0xcc: need(1); o->type = LMSGPACK_POSITIVE_INTEGER; o->via.u64 = s[p]; return LMSGPACK_OK;
    case 0xcd: need(2); o->type = LMSGPACK_POSITIVE_INTEGER; o->via.u64 = load16(s + p); return LMSGPACK_OK;
    case 0xce: need(4); o->type = LMSGPACK_POSITIVE_INTEGER; o->via.u64 = load32(s + p); return LMSGPACK_OK;
    case 0xcf: need(8); o->type = LMSGPACK_POSITIVE_INTEGER; o->via.u64 = load64(s + p); return LMSGPACK_OK;
    case 0xd0: need(1); setint(o, tosigned(s[p], 8)); return LMSGPACK_OK;
    case 0xd1: need(2); setint(o, tosigned(load16(s + p), 16)); return LMSGPACK_OK;
    case 0xd2: need(4); setint(o, tosigned(load32(s + p), 32)); return LMSGPACK_OK;
    case 0xd3: need(8); setint(o, tosigned(load64(s + p), 64)); return LMSGPACK_OK;
    case 0xd4: return readbody(s, avail, p, 1 + 1, LMSGPACK_EXT, o, end);
    case 0xd5: return readbody(s, avail, p, 2 + 1, LMSGPACK_EXT, o, end);
    case 0xd6: return readbody(s, avail, p, 4 + 1, LMSGPACK_EXT, o, end);
    case 0xd7: return readbody(s, avail, p, 8 + 1, LMSGPACK_EXT, o, end);
    case 0xd8: return readbody(s, avail, p, 16 + 1, LMSGPACK_EXT, o, end);
    case 0xd9: need(1); return readbody(s, avail - 1, p + 1, s[p], LMSGPACK_STR, o, end);
    case 0xda: need(2); return readbody(s, avail - 2, p + 2, load16(s + p), LMSGPACK_STR, o, end);
    case 0xdb: need(4); return readbody(s, avail - 4, p + 4, load32(s + p), LMSGPACK_STR, o, end);
    case 0xdc: need(2); setcontainer(o, LMSGPACK_ARRAY, load16(s + p)); return LMSGPACK_OK;
    case 0xdd: need(4); setcontainer(o, LMSGPACK_ARRAY, load32(s + p)); return LMSGPACK_OK;
    case 0xde: need(2); setcontainer(o, LMSGPACK_MAP, load16(s + p)); return LMSGPACK_OK;
    case 0xdf: need(4); setcontainer(o, LMSGPACK_MAP, load32(s + p)); return LMSGPACK_OK;
    default: return LMSGPACK_INVALID;
  }
  #undef need
}

lmsgpack_status lmsgpack_read (const char *data, size_t len, size_t *off, lmsgpack_object *item) {
  size_t end;
  lmsgpack_status status = readitem((const unsigned char *)data, len, *off, &end, item);
  if (status == LMSGPACK_OK)
    *off = end;
  return status;
}

static const unsigned char fixedsize[0x20] = {
  1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 5, 9, 2, 3, 5, 9,
  2, 3, 5, 9, 3, 4, 6, 10, 18, 0, 0, 0, 0, 0, 0, 0
};

static lmsgpack_status scan (const unsigned char *s, size_t len, size_t *off, uint64_t *items) {
  uint64_t remain[LMSGPACK_MAX_DEPTH];
  int top = 0;
  size_t p = *off;
  *items = 0;
  for (;;) {
    lmsgpack_object o;
    size_t end;
    lmsgpack_status status;
    if (p < len) {
      unsigned int c = s[p];
      size_t size = (c <= 0x7f || c >= 0xe0) ? 1 : (c >= 0xc0) ? fixedsize[c - 0xc0] : 0;
      if (size > 0 && len - p >= size) {
        p += size;
        goto value;
      }
    }
    status = readitem(s, len, p, &end, &o);
    if (status != LMSGPACK_OK) {
      if (status == LMSGPACK_INCOMPLETE)
        *off = end;
      return status;
    }
    p = end;
    if (o.type == LMSGPACK_ARRAY || o.type == LMSGPACK_MAP) {
      uint64_t n = (o.type == LMSGPACK_MAP) ? 2 * (uint64_t)o.via.map.size : o.via.array.size;
      if (top >= LMSGPACK_MAX_DEPTH)
        return LMSGPACK_TOO_DEEP;
      if (n > 0) {
        *items += n;
        remain[top++] = n;
        continue;
      }
    }
  value:
    while (top > 0 && --remain[top - 1] == 0)
      top--;
    if (top == 0) {
      *off = p;
      return LMSGPACK_OK;
    }
  }
}

lmsgpack_status lmsgpack_check (const char *data, size_t len, size_t *off) {
  uint64_t items;
  return scan((const unsigned char *)data, len, off, &items);
}

static void *defaultalloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
  (void)osize;
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nsize);
}

void lmsgpack_unpacked_init (lmsgpack_unpacked *u, lmsgpack_alloc alloc, void *ud) {
  u->data.type = LMSGPACK_NIL;
  u->mem = NULL;
  u->memsize = 0;
  u->alloc = (alloc != NULL) ? alloc : defaultalloc;
  u->ud = ud;
}

void lmsgpack_unpacked_free (lmsgpack_unpacked *u) {
  if (u->mem != NULL)
    u->alloc(u->ud, u->mem, u->memsize, 0);
  u->mem = NULL;
  u->memsize = 0;
  u->data.type = LMSGPACK_NIL;
}

static void build (const unsigned char *s, size_t len, size_t *p, lmsgpack_object *o, lmsgpack_object **next) {
  uint32_t i, n;
  readitem(s, len, *p, p, o);
  if (o->type == LMSGPACK_ARRAY && (n = o->via.array.size) > 0) {
    o->via.array.ptr = *next;
    *next += n;
    for (i = 0; i < n; i++)
      build(s, len, p, &o->via.array.ptr[i], next);
  }
  else if (o->type == LMSGPACK_MAP && (n = o->via.map.size) > 0) {
    o->via.map.ptr = (lmsgpack_pair *)(void *)*next;
    *next += 2 * (size_t)n;
    for (i = 0; i < n; i++) {
      build(s, len, p, &o->via.map.ptr[i].key, next);
      build(s, len, p, &o->via.map.ptr[i].val, next);
    }
  }
}

lmsgpack_status lmsgpack_unpack (lmsgpack_unpacked *u, const char *data, size_t len, size_t *off) {
  const unsigned char *s = (const unsigned char *)data;
  size_t start = *off, end = *off;
  uint64_t items;
  lmsgpack_object *next;
  lmsgpack_status status;
  lmsgpack_unpacked_free(u);
  if ((status = scan(s, len, &end, &items)) != LMSGPACK_OK) {
    if (status == LMSGPACK_INCOMPLETE)
      *off = end;
    return status;
  }
  if (items > 0) {
    if (items > SIZE_MAX / sizeof(lmsgpack_object))
      return LMSGPACK_NOMEM;
    u->memsize = (size_t)items * sizeof(lmsgpack_object);
    if ((u->mem = (lmsgpack_object *)u->alloc(u->ud, NULL, 0, u->memsize)) == NULL) {
      u->memsize = 0;
      return LMSGPACK_NOMEM;
    }
  }
  next = u->mem;
  build(s, len, &start, &u->data, &next);
  *off = end;
  return LMSGPACK_OK;
}

