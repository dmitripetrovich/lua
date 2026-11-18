#ifndef ljson_h
#define ljson_h

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef void *(*ljson_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);

enum {
  LJSON_OK = 0,
  LJSON_ERROR_DOCUMENT_EMPTY,
  LJSON_ERROR_DOCUMENT_ROOT_NOT_SINGULAR,
  LJSON_ERROR_VALUE_INVALID,
  LJSON_ERROR_OBJECT_MISS_NAME,
  LJSON_ERROR_OBJECT_MISS_COLON,
  LJSON_ERROR_OBJECT_MISS_COMMA_OR_CURLY_BRACKET,
  LJSON_ERROR_ARRAY_MISS_COMMA_OR_SQUARE_BRACKET,
  LJSON_ERROR_STRING_UNICODE_ESCAPE_INVALID_HEX,
  LJSON_ERROR_STRING_UNICODE_SURROGATE_INVALID,
  LJSON_ERROR_STRING_ESCAPE_INVALID,
  LJSON_ERROR_STRING_MISS_QUOTATION_MARK,
  LJSON_ERROR_STRING_INVALID_ENCODING,
  LJSON_ERROR_NUMBER_TOO_BIG,
  LJSON_ERROR_NUMBER_MISS_FRACTION,
  LJSON_ERROR_NUMBER_MISS_EXPONENT,
  LJSON_ERROR_TERMINATION,
  LJSON_ERROR_UNSPECIFIC_SYNTAX_ERROR,
  LJSON_ERROR_DEPTH,
  LJSON_ERROR_MEMORY
};

#define LJSON_PARSE_COMMENTS 0x01
#define LJSON_PARSE_TRAILING_COMMAS 0x02
#define LJSON_PARSE_NAN_INF 0x04
#define LJSON_PARSE_FULL_PRECISION 0x08
#define LJSON_PARSE_STOP_WHEN_DONE 0x10
#define LJSON_PARSE_LENIENT 0x20

typedef struct ljson_Handler {
  int (*null) (void *ud);
  int (*boolean) (void *ud, int b);
  int (*integer) (void *ud, int64_t i);
  int (*uinteger) (void *ud, uint64_t u);
  int (*number) (void *ud, double d);
  int (*string) (void *ud, const char *s, size_t len);
  int (*key) (void *ud, const char *s, size_t len);
  int (*start_object) (void *ud);
  int (*end_object) (void *ud, size_t count);
  int (*start_array) (void *ud);
  int (*end_array) (void *ud, size_t count);
} ljson_Handler;

typedef struct ljson_Reader {
  unsigned flags;
  size_t maxdepth;
  ljson_Alloc alloc;
  void *allocud;
} ljson_Reader;

int ljson_parse (const ljson_Reader *r, const char *s, size_t len, const ljson_Handler *h, void *ud, size_t *offset);

const char *ljson_errormessage (int error);

#define LJSON_WRITE_PRETTY 0x01
#define LJSON_WRITE_NAN_INF 0x02
#define LJSON_WRITE_SINGLE_LINE_ARRAY 0x04

#define LJSON_MAXDECIMALS 324

typedef struct ljson_Level {
  size_t count;
  int inarray;
} ljson_Level;

typedef struct ljson_Writer {
  unsigned flags;
  int maxdecimals;
  char indentchar;
  unsigned indentcount;
  char *b;
  size_t n, size;
  ljson_Level *levels;
  size_t depth, nlevels;
  int hasroot;
  int nomem;
  ljson_Alloc alloc;
  void *allocud;
  char initb[256];
  ljson_Level initlevels[32];
} ljson_Writer;

void ljson_writerinit (ljson_Writer *w, ljson_Alloc alloc, void *allocud);
void ljson_writerfree (ljson_Writer *w);

const char *ljson_result (const ljson_Writer *w, size_t *len);

int ljson_null (ljson_Writer *w);
int ljson_bool (ljson_Writer *w, int b);
int ljson_int (ljson_Writer *w, int64_t i);
int ljson_uint (ljson_Writer *w, uint64_t u);
int ljson_double (ljson_Writer *w, double d);
int ljson_string (ljson_Writer *w, const char *s, size_t len);
int ljson_key (ljson_Writer *w, const char *s, size_t len);
int ljson_startobject (ljson_Writer *w);
int ljson_endobject (ljson_Writer *w);
int ljson_startarray (ljson_Writer *w);
int ljson_endarray (ljson_Writer *w);
int ljson_raw (ljson_Writer *w, const char *s, size_t len);

char *ljson_dtoa (double d, char *buff, int maxdecimals);
char *ljson_i64toa (int64_t i, char *buff);
char *ljson_u64toa (uint64_t u, char *buff);

#if defined(__cplusplus)
}
#endif

#endif
