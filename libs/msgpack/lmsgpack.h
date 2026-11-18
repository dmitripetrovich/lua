#ifndef lmsgpack_h
#define lmsgpack_h

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(LMSGPACK_MAX_DEPTH)
  #define LMSGPACK_MAX_DEPTH 32
#endif

typedef enum lmsgpack_status {
  LMSGPACK_OK = 0,
  LMSGPACK_INCOMPLETE,
  LMSGPACK_INVALID,
  LMSGPACK_TOO_DEEP,
  LMSGPACK_NOMEM,
  LMSGPACK_TOO_LARGE
} lmsgpack_status;

typedef enum lmsgpack_type {
  LMSGPACK_NIL,
  LMSGPACK_BOOLEAN,
  LMSGPACK_POSITIVE_INTEGER,
  LMSGPACK_NEGATIVE_INTEGER,
  LMSGPACK_FLOAT32,
  LMSGPACK_FLOAT64,
  LMSGPACK_STR,
  LMSGPACK_BIN,
  LMSGPACK_EXT,
  LMSGPACK_ARRAY,
  LMSGPACK_MAP
} lmsgpack_type;

typedef struct lmsgpack_object lmsgpack_object;
typedef struct lmsgpack_pair lmsgpack_pair;

struct lmsgpack_object {
  lmsgpack_type type;
  union {
    int boolean;
    uint64_t u64;
    int64_t i64;
    double f64;
    struct { uint32_t size; const char *ptr; } str;
    struct { uint32_t size; const char *ptr; } bin;
    struct { int8_t type; uint32_t size; const char *ptr; } ext;
    struct { uint32_t size; lmsgpack_object *ptr; } array;
    struct { uint32_t size; lmsgpack_pair *ptr; } map;
  } via;
};

struct lmsgpack_pair {
  lmsgpack_object key;
  lmsgpack_object val;
};

typedef void *(*lmsgpack_alloc) (void *ud, void *ptr, size_t osize, size_t nsize);

typedef struct lmsgpack_writer lmsgpack_writer;

typedef int (*lmsgpack_reserve) (lmsgpack_writer *w, size_t need);

struct lmsgpack_writer {
  char *buf;
  size_t len;
  size_t cap;
  lmsgpack_reserve reserve;
  void *ud;
  lmsgpack_status status;
};

void lmsgpack_writer_init (lmsgpack_writer *w, char *buf, size_t cap, lmsgpack_reserve reserve, void *ud);

void lmsgpack_buffer_init (lmsgpack_writer *w);
void lmsgpack_buffer_free (lmsgpack_writer *w);

char *lmsgpack_writer_reserve (lmsgpack_writer *w, size_t n);

int lmsgpack_write_nil (lmsgpack_writer *w);
int lmsgpack_write_true (lmsgpack_writer *w);
int lmsgpack_write_false (lmsgpack_writer *w);
int lmsgpack_write_bool (lmsgpack_writer *w, int b);

int lmsgpack_write_int (lmsgpack_writer *w, int64_t v);
int lmsgpack_write_uint (lmsgpack_writer *w, uint64_t v);

int lmsgpack_write_sint (lmsgpack_writer *w, int64_t v);

int lmsgpack_write_fixint (lmsgpack_writer *w, int size, int issigned, uint64_t v);

int lmsgpack_write_float (lmsgpack_writer *w, float v);
int lmsgpack_write_double (lmsgpack_writer *w, double v);

int lmsgpack_write_array (lmsgpack_writer *w, size_t n);
int lmsgpack_write_map (lmsgpack_writer *w, size_t n);
int lmsgpack_write_str (lmsgpack_writer *w, size_t n);
int lmsgpack_write_str_compat (lmsgpack_writer *w, size_t n);
int lmsgpack_write_bin (lmsgpack_writer *w, size_t n);
int lmsgpack_write_ext (lmsgpack_writer *w, size_t n, int8_t type);

int lmsgpack_write_bytes (lmsgpack_writer *w, const void *p, size_t n);

int lmsgpack_write_object (lmsgpack_writer *w, const lmsgpack_object *o);

lmsgpack_status lmsgpack_check (const char *data, size_t len, size_t *off);

lmsgpack_status lmsgpack_read (const char *data, size_t len, size_t *off, lmsgpack_object *item);

typedef struct lmsgpack_unpacked {
  lmsgpack_object data;
  lmsgpack_object *mem;
  size_t memsize;
  lmsgpack_alloc alloc;
  void *ud;
} lmsgpack_unpacked;

void lmsgpack_unpacked_init (lmsgpack_unpacked *u, lmsgpack_alloc alloc, void *ud);
void lmsgpack_unpacked_free (lmsgpack_unpacked *u);

lmsgpack_status lmsgpack_unpack (lmsgpack_unpacked *u, const char *data, size_t len, size_t *off);

#if defined(__cplusplus)
}
#endif

#endif
