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

typedef void *(*lmsgpack_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);

enum {
  LMSGPACK_OK = 0,
  LMSGPACK_ERROR_INCOMPLETE,
  LMSGPACK_ERROR_INVALID,
  LMSGPACK_ERROR_DEPTH,
  LMSGPACK_ERROR_MEMORY,
  LMSGPACK_ERROR_TOO_LARGE
};

const char *lmsgpack_errormessage (int error);

typedef enum lmsgpack_Type {
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
} lmsgpack_Type;

typedef struct lmsgpack_Value lmsgpack_Value;
typedef struct lmsgpack_Pair lmsgpack_Pair;

struct lmsgpack_Value {
  lmsgpack_Type type;
  union {
    int boolean;
    uint64_t u64;
    int64_t i64;
    double f64;
    struct { uint32_t size; const char *ptr; } str;
    struct { uint32_t size; const char *ptr; } bin;
    struct { int8_t type; uint32_t size; const char *ptr; } ext;
    struct { uint32_t size; lmsgpack_Value *ptr; } array;
    struct { uint32_t size; lmsgpack_Pair *ptr; } map;
  } via;
};

struct lmsgpack_Pair {
  lmsgpack_Value key;
  lmsgpack_Value val;
};

typedef struct lmsgpack_Writer lmsgpack_Writer;

typedef int (*lmsgpack_Grow) (lmsgpack_Writer *w, size_t need);

struct lmsgpack_Writer {
  char *buf;
  size_t len;
  size_t cap;
  lmsgpack_Grow grow;
  void *ud;
  lmsgpack_Alloc alloc;
  void *allocud;
  int status;
};

void lmsgpack_writer_init (lmsgpack_Writer *w, lmsgpack_Alloc alloc, void *allocud);
void lmsgpack_writer_initbuffer (lmsgpack_Writer *w, char *buf, size_t cap, lmsgpack_Grow grow, void *ud);
void lmsgpack_writer_free (lmsgpack_Writer *w);

const char *lmsgpack_writer_result (const lmsgpack_Writer *w, size_t *len);
char *lmsgpack_writer_reserve (lmsgpack_Writer *w, size_t n);

int lmsgpack_write_nil (lmsgpack_Writer *w);
int lmsgpack_write_true (lmsgpack_Writer *w);
int lmsgpack_write_false (lmsgpack_Writer *w);
int lmsgpack_write_bool (lmsgpack_Writer *w, int b);
int lmsgpack_write_int (lmsgpack_Writer *w, int64_t v);
int lmsgpack_write_uint (lmsgpack_Writer *w, uint64_t v);
int lmsgpack_write_sint (lmsgpack_Writer *w, int64_t v);
int lmsgpack_write_fixint (lmsgpack_Writer *w, int size, int issigned, uint64_t v);
int lmsgpack_write_float (lmsgpack_Writer *w, float v);
int lmsgpack_write_double (lmsgpack_Writer *w, double v);
int lmsgpack_write_array (lmsgpack_Writer *w, size_t n);
int lmsgpack_write_map (lmsgpack_Writer *w, size_t n);
int lmsgpack_write_str (lmsgpack_Writer *w, size_t n);
int lmsgpack_write_str_compat (lmsgpack_Writer *w, size_t n);
int lmsgpack_write_bin (lmsgpack_Writer *w, size_t n);
int lmsgpack_write_ext (lmsgpack_Writer *w, size_t n, int8_t type);
int lmsgpack_write_bytes (lmsgpack_Writer *w, const void *p, size_t n);
int lmsgpack_write_value (lmsgpack_Writer *w, const lmsgpack_Value *v);

int lmsgpack_check (const char *s, size_t len, size_t *offset);
int lmsgpack_read (const char *s, size_t len, size_t *offset, lmsgpack_Value *v);

typedef struct lmsgpack_Document {
  lmsgpack_Value root;
  lmsgpack_Value *mem;
  size_t memsize;
  lmsgpack_Alloc alloc;
  void *allocud;
} lmsgpack_Document;

void lmsgpack_document_init (lmsgpack_Document *d, lmsgpack_Alloc alloc, void *allocud);
void lmsgpack_document_free (lmsgpack_Document *d);

int lmsgpack_decode (lmsgpack_Document *d, const char *s, size_t len, size_t *offset);

#if defined(__cplusplus)
}
#endif

#endif
