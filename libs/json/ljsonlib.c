#define ljsonlib_c
#define LUA_LIB

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(__cplusplus) && !defined(LUA_COMPILED_AS_HPP)
extern "C" {
#endif
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <lgrit_lib.h>
#if defined(__cplusplus) && !defined(LUA_COMPILED_AS_HPP)
}
#endif

#include "ljson.h"
#include "ljsonlib.h"

#if !defined(LUA_JSON_TABLE_CUTOFF)
  #define LUA_JSON_TABLE_CUTOFF 11
#endif

#if !defined(LUA_JSON_DEFAULT_DEPTH)
  #define LUA_JSON_DEFAULT_DEPTH 32
#endif

#if !defined(LUA_JSON_MAX_DEPTH)
  #define LUA_JSON_MAX_DEPTH 200
#endif

#if !defined(LUA_JSON_MAX_REPLACEMENTS)
  #define LUA_JSON_MAX_REPLACEMENTS 200
#endif

#if LUA_FLOAT_TYPE == LUA_FLOAT_FLOAT
  #define LUA_NUMBER_FMT_LEN 7
  #define LUA_NUMBER_FMT_POW 1E7
#elif LUA_FLOAT_TYPE == LUA_FLOAT_LONGDOUBLE
  #define LUA_NUMBER_FMT_LEN 19
  #define LUA_NUMBER_FMT_POW 1E19
#else
  #define LUA_NUMBER_FMT_LEN 14
  #define LUA_NUMBER_FMT_POW 1E14
#endif

#define MAXNUMBER2STR 44

#if !defined(l_sprintf)
  #if defined(LUA_USE_WINDOWS) && defined(__STDC_WANT_SECURE_LIB__)
    #define l_sprintf(s, sz, f, i) sprintf_s((s), (sz), (f), (i))
  #else
    #define l_sprintf(s, sz, f, i) snprintf((s), (sz), (f), (i))
  #endif
#endif

#define JSON_MAX_LUAINDEX (sizeof(size_t) < sizeof(lua_Integer) ? (size_t)-1 : (size_t)LUA_MAXINTEGER)

#define JSON_OPTION_RESERVED 0x0
#define JSON_PRETTY_PRINT 0x01
#define JSON_SORT_KEYS 0x02
#define JSON_LUA_NULL 0x04
#define JSON_NESTING_NULL 0x08
#define JSON_UNSIGNED_INTEGERS 0x10
#define JSON_NAN_AND_INF 0x20
#define JSON_ENCODE_INT32 0x40
#define JSON_ENCODER_ARRAY_VECTOR 0x80
#define JSON_LUA_DTOA 0x100
#define JSON_LUA_GRISU 0x200
#define JSON_ARRAY_SINGLE_LINE 0x10000
#define JSON_ARRAY_EMPTY 0x20000
#define JSON_ARRAY_WITH_HOLES 0x40000
#define JSON_ENCODER_HANDLER 0x2000000
#define JSON_DECODER_PRESET 0x4000000
#define JSON_ENCODER_DECIMALS 0x8000000
#define JSON_ENCODER_INDENT 0x10000000
#define JSON_ENCODER_INDENT_AMT 0x20000000
#define JSON_ENCODER_MAX_DEPTH 0x40000000
#define JSON_TABLE_KEY_ORDER 0x80000000

#define JSON_DEFAULT (JSON_LUA_NULL | JSON_ARRAY_EMPTY | JSON_ARRAY_WITH_HOLES | JSON_NAN_AND_INF)

#define JSON_DECODE_DEFAULT 0x0
#define JSON_DECODE_EXTENDED 0x1

#define LUA_JSON_REG_FLAGS 1
#define LUA_JSON_REG_DEPTH 2
#define LUA_JSON_REG_INDENT 3
#define LUA_JSON_REG_INDENT_AMT 4
#define LUA_JSON_REG_MAXDEC 5
#define LUA_JSON_REG_PRESET 6

#define json_checkstack(L, sz) do { if (!lua_checkstack((L), (sz))) throwmsg((L), "Lua Stack Overflow"); } while (0)

static int throwmsg (lua_State *L, const char *msg) {
  lua_pushstring(L, msg);
  return lua_error(L);
}

static int throwtype (lua_State *L, const char *fmt, int type) {
  lua_pushfstring(L, fmt, lua_typename(L, type));
  return lua_error(L);
}

#define ERROR_UNSUPPORTED_TYPE "type '%s' is not supported by JSON"
#define ERROR_UNSUPPORTED_KEY "type '%s' is not supported as a keyorder by JSON"

#define LUA_JSON_GUARD "lua_json_guard"
#define GUARD_BLOCKS 4

typedef struct Guard {
  lua_Alloc f;
  void *ud;
  void *block[GUARD_BLOCKS];
  size_t size[GUARD_BLOCKS];
} Guard;

typedef struct Anchor {
  lua_State *L;
  int slot;
  Guard *g;
} Anchor;

static void freeguard (Guard *g) {
  int i;
  for (i = 0; i < GUARD_BLOCKS; i++) {
    if (g->block[i] != NULL) g->f(g->ud, g->block[i], g->size[i], 0);
    g->block[i] = NULL;
    g->size[i] = 0;
  }
}

static int guard_gc (lua_State *L) {
  freeguard((Guard *)luaL_checkudata(L, 1, LUA_JSON_GUARD));
  return 0;
}

static void initanchor (lua_State *L, Anchor *a) {
  a->L = L;
  a->g = NULL;
  lua_pushnil(L);
  a->slot = lua_gettop(L);
}

static void freeanchor (Anchor *a) {
  if (a->g != NULL) freeguard(a->g);
}

static void *anchoralloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  Anchor *a = (Anchor *)ud;
  Guard *g = a->g;
  void *nb;
  int i;
  if (g == NULL) {
    lua_State *L = a->L;
    if (nsize == 0) return NULL;
    luaL_checkstack(L, 2, NULL);
    g = (Guard *)lua_newuserdatauv(L, sizeof(Guard), 0);
    g->f = lua_getallocf(L, &g->ud);
    for (i = 0; i < GUARD_BLOCKS; i++) {
      g->block[i] = NULL;
      g->size[i] = 0;
    }
    luaL_setmetatable(L, LUA_JSON_GUARD);
    lua_replace(L, a->slot);
    a->g = g;
  }
  for (i = 0; i < GUARD_BLOCKS && g->block[i] != ptr; i++) {}
  if (i == GUARD_BLOCKS) luaL_error(a->L, "too many json buffers");
  nb = g->f(g->ud, ptr, osize, nsize);
  if (nb == NULL && nsize > 0) luaL_error(a->L, "not enough memory");
  g->block[i] = nb;
  g->size[i] = (nb != NULL) ? nsize : 0;
  return nb;
}

static void *growarray (Anchor *a, void *block, void *init, size_t *size, size_t need, size_t elem) {
  size_t nsize = (*size > 0) ? *size * 2 : 8;
  void *nb;
  if (nsize < need) nsize = need;
  nb = anchoralloc(a, (block == init) ? NULL : block, *size * elem, nsize * elem);
  if (block == init) memcpy(nb, init, *size * elem);
  *size = nsize;
  return nb;
}

static size_t luaL_optsizet (lua_State *L, int arg, size_t def) {
  if (lua_isnoneornil(L, arg)) return def;
  else if (lua_isinteger(L, arg)) {
    const lua_Integer i = lua_tointeger(L, arg);
    if (i >= 0 && (size_t)i <= JSON_MAX_LUAINDEX) return (size_t)i;
    luaL_argerror(L, arg, "invalid integer argument");
    return 0;
  }
  luaL_argerror(L, arg, lua_pushfstring(L, "integer expected"));
  return 0;
}

static int luaL_optcheckoption (lua_State *L, int arg, const char *def, const char *const lst[], int ldef) {
  const char *name = (def) ? luaL_optstring(L, arg, def) : luaL_checkstring(L, arg);
  int i;
  for (i = 0; lst[i]; i++) {
    if (strcmp(lst[i], name) == 0) return i;
  }
  return ldef;
}

static int lua_json_getsubtable (lua_State *L, int idx, const char *key) {
  if (lua_getfield(L, idx, key) == LUA_TTABLE) return 1;
  lua_pop(L, 1);
  idx = lua_absindex(L, idx);
  lua_createtable(L, LUA_JSON_REG_PRESET + 1, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, idx, key);
  return 0;
}

static lua_Integer geti (lua_State *L, int idx, lua_Integer key, lua_Integer opt) {
  lua_Integer result;
  lua_rawgeti(L, idx, key);
  result = luaL_optinteger(L, -1, opt);
  lua_pop(L, 1);
  return result;
}

static void seti (lua_State *L, int idx, lua_Integer key, lua_Integer value) {
  idx = lua_absindex(L, idx);
  lua_pushinteger(L, value);
  lua_rawseti(L, idx, key);
}

static int is_json_null (lua_State *L, int idx) { return lua_type(L, idx) == LUA_TLIGHTUSERDATA && lua_touserdata(L, idx) == NULL; }

static int has_json_type (lua_State *L, int idx, int *is_array) {
  int result = 0;
  if (luaL_getmetafield(L, idx, "__jsontype") != LUA_TNIL) {
    if (lua_type(L, -1) == LUA_TSTRING) {
      *is_array = strcmp(lua_tostring(L, -1), "array") == 0;
      result = 1;
    }
    lua_pop(L, 1);
  }
  return result;
}

static int isnaninf (double d) {
  uint64_t u;
  memcpy(&u, &d, sizeof(u));
  return (u & UINT64_C(0x7FF0000000000000)) == UINT64_C(0x7FF0000000000000);
}

static const char *lua_dtoa (char *s, size_t n, double value) {
  int l = l_sprintf(s, n, LUA_NUMBER_FMT, (LUAI_UACNUMBER)value);
  char *end, *p;
  if (s[strspn(s, "-0123456789")] == '\0') {
    s[l++] = '.';
    s[l++] = '0';
    return s + l;
  }
  end = s + l;
  for (p = s; p != end; ++p) {
    if (*p == ',') *p = '.';
  }
  return end;
}

static double lua_grisuRound (double d) {
  if ((DBL_MAX / LUA_NUMBER_FMT_POW) <= fabs(d)) return d;
  return round(d * LUA_NUMBER_FMT_POW) / LUA_NUMBER_FMT_POW;
}

static int parseVector (lua_State *L, int idx, lua_Float4 *f) {
  switch (lua_tofloat4(L, idx, V_PARSETABLE, f)) {
    case LUA_VVECTOR1: return 1;
    case LUA_VVECTOR2: return 2;
    case LUA_VVECTOR3: return 3;
    case LUA_VQUAT: case LUA_VVECTOR4: return 4;
    default: luaL_typeerror(L, idx, "number or vector type"); return 0;
  }
}

static int table_is_json_array (lua_State *L, int idx, lua_Integer flags, size_t *array_length) {
  const int stacktop = lua_gettop(L);
  int has_type, is_array = 0;
  size_t count = 0, max = 0, arraylen = 0;
  json_checkstack(L, 3);
  has_type = has_json_type(L, idx, &is_array);
  lua_pushnil(L);
  while (lua_next(L, idx)) {
    lua_Integer n;
#if defined(LUA_JSON_COMPAT)
    size_t slen = 0;
    const char *key = NULL;
#endif
    if (lua_isinteger(L, -2) && (n = lua_tointeger(L, -2), (n >= 1 && (size_t)n <= JSON_MAX_LUAINDEX))) {
      const size_t nst = (size_t)n;
      max = nst > max ? nst : max;
      count++;
    }
#if defined(LUA_JSON_COMPAT)
    else if (lua_type(L, -2) == LUA_TSTRING && lua_isinteger(L, -1) && (n = lua_tointeger(L, -1), (n >= 1 && (size_t)n <= JSON_MAX_LUAINDEX)) && (key = lua_tolstring(L, -2, &slen)) != NULL && slen == 1 && key[0] == 'n') {
      arraylen = (size_t)n;
      max = arraylen > max ? arraylen : max;
    }
#endif
    else {
      lua_settop(L, stacktop);
      return 0;
    }
    lua_pop(L, 1);
  }
  *array_length = max;
  lua_settop(L, stacktop);
#if defined(LUA_JSON_COMPAT)
  if (arraylen > (count << 1) + LUA_JSON_TABLE_CUTOFF) return 0;
#endif
  if (has_type && !is_array) return 0;
  else if (max == count) return max > 0 || has_type || (flags & JSON_ARRAY_EMPTY);
  else if (has_type || (flags & JSON_ARRAY_WITH_HOLES)) return (max < LUA_JSON_TABLE_CUTOFF) || max <= arraylen || (count >= (max >> 1));
  return 0;
}

enum { KEY_STRING, KEY_INTEGER, KEY_NUMBER };

typedef struct Key {
  int kind;
  lua_Integer i;
  lua_Number n;
  const char *s;
  size_t len;
} Key;

typedef struct KeyList {
  Key *k;
  size_t n, size;
  int slot;
  int anchor;
} KeyList;

typedef struct Encoder {
  lua_Integer flags;
  int max_depth;
  int error_handler_idx;
  int replacements;
  KeyList *order;
  ljson_Writer *w;
} Encoder;

static void initkeys (lua_State *L, KeyList *l) {
  json_checkstack(L, 2);
  l->k = NULL;
  l->n = l->size = 0;
  lua_pushnil(L);
  l->slot = lua_gettop(L);
  lua_pushnil(L);
  l->anchor = lua_gettop(L);
}

static void pushkey (lua_State *L, KeyList *l, Key k, int keyidx) {
  keyidx = lua_absindex(L, keyidx);
  json_checkstack(L, 2);
  if (k.kind == KEY_STRING) {
    if (lua_isnil(L, l->anchor)) {
      lua_createtable(L, 8, 0);
      lua_replace(L, l->anchor);
    }
    lua_pushvalue(L, keyidx);
    lua_rawseti(L, l->anchor, (lua_Integer)l->n + 1);
  }
  if (l->n == l->size) {
    size_t nsize = (l->size > 0) ? l->size * 2 : 8;
    Key *nk = (Key *)lua_newuserdatauv(L, nsize * sizeof(Key), 0);
    if (l->n > 0) memcpy(nk, l->k, l->n * sizeof(Key));
    lua_replace(L, l->slot);
    l->k = nk;
    l->size = nsize;
  }
  l->k[l->n++] = k;
}

static Key intkey (lua_Integer i) {
  Key k;
  k.kind = KEY_INTEGER;
  k.i = i;
  k.n = 0;
  k.s = NULL;
  k.len = 0;
  return k;
}

static Key numkey (lua_Number n) {
  Key k = intkey(0);
  k.kind = KEY_NUMBER;
  k.n = n;
  return k;
}

static Key strkey (const char *s, size_t len) {
  Key k = intkey(0);
  k.kind = KEY_STRING;
  k.s = s;
  k.len = len;
  return k;
}

static lua_Number keynumber (const Key *k) { return (k->kind == KEY_INTEGER) ? (lua_Number)k->i : k->n; }

static int keystrcmp (const Key *a, const Key *b) {
  int c = memcmp(a->s, b->s, (a->len < b->len) ? a->len : b->len);
  return (c != 0) ? c : (a->len < b->len) ? -1 : (a->len > b->len);
}

static int keyless (const Key *a, const Key *b) {
  if (a->kind != KEY_STRING) return (b->kind != KEY_STRING) ? (keynumber(a) < keynumber(b)) : 1;
  return (b->kind != KEY_STRING) ? 0 : (keystrcmp(a, b) < 0);
}

static int keyequal (const Key *a, const Key *b) {
  if (a->kind != KEY_STRING) return (b->kind != KEY_STRING) ? (keynumber(a) == keynumber(b)) : 0;
  return (b->kind != KEY_STRING) ? 0 : (a->len == b->len && memcmp(a->s, b->s, a->len) == 0);
}

static int findkey (const KeyList *l, const Key *k) {
  size_t i;
  for (i = 0; i < l->n; i++) {
    if (keyequal(k, &l->k[i])) return 1;
  }
  return 0;
}

static void sortkeys (lua_State *L, KeyList *l) {
  Key *tmp, *a = l->k;
  size_t width, i;
  if (l->n < 2) return;
  json_checkstack(L, 1);
  tmp = (Key *)lua_newuserdatauv(L, l->n * sizeof(Key), 0);
  for (width = 1; width < l->n; width *= 2) {
    for (i = 0; i < l->n; i += 2 * width) {
      size_t lo = i, mid = (i + width < l->n) ? i + width : l->n, hi = (i + 2 * width < l->n) ? i + 2 * width : l->n;
      size_t x = lo, y = mid, o = lo;
      while (x < mid && y < hi) tmp[o++] = keyless(&a[y], &a[x]) ? a[y++] : a[x++];
      while (x < mid) tmp[o++] = a[x++];
      while (y < hi) tmp[o++] = a[y++];
    }
    memcpy(a, tmp, l->n * sizeof(Key));
  }
  lua_pop(L, 1);
}

static void populate_key_vector (lua_State *L, int idx, KeyList *sink) {
  const size_t length = (size_t)lua_rawlen(L, idx);
  size_t i;
  for (i = 1; i <= length; ++i) {
    const int type = lua_rawgeti(L, idx, (lua_Integer)i);
    json_checkstack(L, 1);
    switch (type) {
      case LUA_TNUMBER: {
        if (lua_isinteger(L, -1)) pushkey(L, sink, intkey(lua_tointeger(L, -1)), -1);
        else pushkey(L, sink, numkey(lua_tonumber(L, -1)), -1);
        break;
      }
      case LUA_TSTRING: {
        size_t len = 0;
        const char *s = lua_tolstring(L, -1, &len);
        pushkey(L, sink, strkey(s, len), -1);
        break;
      }
      default: {
#if defined(LUA_JSON_SANITIZE_KEYS)
        throwtype(L, ERROR_UNSUPPORTED_KEY, type);
#endif
        break;
      }
    }
    lua_pop(L, 1);
  }
}

static void populate_unordered_vector (lua_State *L, int idx, const KeyList *key_order, KeyList *sink) {
  json_checkstack(L, 3);
  lua_pushnil(L);
  while (lua_next(L, idx)) {
    switch (lua_type(L, -2)) {
      case LUA_TNUMBER: {
        Key k = lua_isinteger(L, -2) ? intkey(lua_tointeger(L, -2)) : numkey(lua_tonumber(L, -2));
        if (!findkey(key_order, &k)) pushkey(L, sink, k, -2);
        break;
      }
      case LUA_TSTRING: {
        size_t len = 0;
        const char *s = lua_tolstring(L, -2, &len);
        Key k = strkey(s, len);
        if (!findkey(key_order, &k)) pushkey(L, sink, k, -2);
        break;
      }
      default:
#if defined(LUA_JSON_SANITIZE_KEYS)
        throwtype(L, ERROR_UNSUPPORTED_KEY, lua_type(L, -2));
#endif
        break;
    }
    lua_pop(L, 1);
  }
}

static void encodeValue (lua_State *L, Encoder *E, int idx, int depth);

static void encodeComponent (lua_State *L, Encoder *E, const char *key, lua_VecF f) {
  if (key != NULL) ljson_write_key(E->w, key, 1);
  if (ljson_write_double(E->w, (double)f) != LJSON_OK) throwmsg(L, "error encoding: kWriteNanAndInfFlag");
}

static int handle_exception (lua_State *L, Encoder *E, int idx, int depth, const char *reason) {
  int result = 0;
  if (E->error_handler_idx > 0 && E->replacements < LUA_JSON_MAX_REPLACEMENTS) {
    json_checkstack(L, 3);
    lua_pushvalue(L, E->error_handler_idx);
    lua_pushstring(L, reason);
    lua_pushvalue(L, idx);
    lua_call(L, 2, 2);
    if (lua_isnil(L, -2)) {
      if (luaL_optstring(L, -1, NULL) != NULL) {
        lua_remove(L, -2);
        return 2;
      }
    }
    else {
      E->replacements++;
      encodeValue(L, E, lua_absindex(L, -2), depth + 1);
      E->replacements--;
      result = 1;
    }
    lua_pop(L, 2);
  }
  return result;
}

static int encodeMetafield (lua_State *L, Encoder *E, int idx) {
  const int type = luaL_getmetafield(L, idx, "__tojson");
  if (type == LUA_TNIL) return 0;
  if (type == LUA_TFUNCTION) {
    lua_pushvalue(L, idx);
    lua_call(L, 1, 1);
    if (lua_type(L, -1) == LUA_TSTRING) {
      size_t len;
      const char *s = lua_tolstring(L, -1, &len);
      ljson_write_raw(E->w, s, len);
    }
    else throwmsg(L, "Invalid __tojson result");
    lua_pop(L, 1);
  }
  else throwmsg(L, "Invalid __tojson function");
  return 1;
}

static int OrderedKey (Encoder *E, const Key *key) {
  if (key->kind == KEY_INTEGER) {
    char buffer[MAXNUMBER2STR];
    const char *end = ljson_i64toa((int64_t)key->i, buffer);
    return ljson_write_key(E->w, buffer, (size_t)(end - buffer)) == LJSON_OK;
  }
  else if (key->kind == KEY_NUMBER) {
    const double d = (double)key->n;
    char buffer[MAXNUMBER2STR + 2] = { 0 };
    const char *end;
    if (isnaninf(d)) {
      const char *lit;
      if (!(E->flags & JSON_NAN_AND_INF)) return 0;
      if (d != d) lit = "NaN";
      else if (d < 0) lit = "-Infinity";
      else lit = "Infinity";
      return ljson_write_key(E->w, lit, strlen(lit)) == LJSON_OK;
    }
    if (E->flags & JSON_LUA_DTOA) end = lua_dtoa(buffer, MAXNUMBER2STR, d);
    else end = ljson_dtoa((E->flags & JSON_LUA_GRISU) ? lua_grisuRound(d) : d, buffer, E->w->maxdecimals);
    return ljson_write_key(E->w, buffer, (size_t)(end - buffer)) == LJSON_OK;
  }
  return ljson_write_key(E->w, key->s, key->len) == LJSON_OK;
}

static void encode_array (lua_State *L, Encoder *E, int idx, size_t array_length, int depth) {
  size_t i;
  ljson_write_start_array(E->w);
  for (i = 1; i <= array_length; ++i) {
    lua_rawgeti(L, idx, (lua_Integer)i);
    encodeValue(L, E, lua_gettop(L), depth);
    lua_pop(L, 1);
  }
  ljson_write_end_array(E->w);
}

static void encodeObject (lua_State *L, Encoder *E, int idx, int depth) {
  json_checkstack(L, 3);
  ljson_write_start_object(E->w);
  lua_pushnil(L);
  while (lua_next(L, idx)) {
    Key key;
    switch (lua_type(L, -2)) {
      case LUA_TSTRING: {
        size_t len = 0;
        const char *s = lua_tolstring(L, -2, &len);
        key = strkey(s, len);
        break;
      }
      case LUA_TNUMBER: {
        if (lua_isinteger(L, -2)) key = intkey(lua_tointeger(L, -2));
        else key = numkey(lua_tonumber(L, -2));
        break;
      }
      default: {
#if defined(LUA_JSON_SANITIZE_KEYS)
        throwtype(L, ERROR_UNSUPPORTED_KEY, lua_type(L, -2));
#endif
        lua_pop(L, 1);
        continue;
      }
    }
    if (OrderedKey(E, &key)) encodeValue(L, E, lua_gettop(L), depth);
    lua_pop(L, 1);
  }
  ljson_write_end_object(E->w);
}

static void pushkeyvalue (lua_State *L, const Key *k) {
  if (k->kind == KEY_INTEGER) lua_pushinteger(L, k->i);
  else if (k->kind == KEY_NUMBER) lua_pushnumber(L, k->n);
  else lua_pushlstring(L, k->s, k->len);
}

static void encodeOrderedObject (lua_State *L, Encoder *E, int idx, int depth, const KeyList *keyorder, const KeyList *unordered) {
  size_t i;
  json_checkstack(L, 2);
  ljson_write_start_object(E->w);
  for (i = 0; i < keyorder->n; i++) {
    pushkeyvalue(L, &keyorder->k[i]);
    if (lua_gettable(L, idx) != LUA_TNIL && OrderedKey(E, &keyorder->k[i])) encodeValue(L, E, lua_gettop(L), depth);
    lua_pop(L, 1);
  }
  for (i = 0; i < unordered->n; i++) {
    pushkeyvalue(L, &unordered->k[i]);
    if (lua_gettable(L, idx) != LUA_TNIL && OrderedKey(E, &unordered->k[i])) encodeValue(L, E, lua_gettop(L), depth);
    lua_pop(L, 1);
  }
  ljson_write_end_object(E->w);
}

static void encodeTable (lua_State *L, Encoder *E, int idx, int depth) {
  const int top = lua_gettop(L);
  size_t array_length;
  if (depth > E->max_depth) {
    int r = handle_exception(L, E, idx, depth, "reference cycle");
    if (r != 1) {
      if (E->flags & JSON_NESTING_NULL) {
        lua_settop(L, top);
        ljson_write_null(E->w);
      }
      else if (r == 2) lua_error(L);
      else throwmsg(L, "maximum table nesting depth exceeded");
    }
    return;
  }
  if (encodeMetafield(L, E, idx)) {}
  else if (table_is_json_array(L, idx, E->flags, &array_length)) encode_array(L, E, idx, array_length, depth);
  else if (luaL_getmetafield(L, idx, "__jsonorder") != LUA_TNIL) {
    if (lua_type(L, -1) == LUA_TFUNCTION) {
      lua_pushvalue(L, idx);
      lua_call(L, 1, 1);
    }
    if (lua_type(L, -1) == LUA_TTABLE) {
      const int order = lua_gettop(L);
      KeyList meta_order, unorder;
      initkeys(L, &meta_order);
      initkeys(L, &unorder);
      populate_key_vector(L, order, &meta_order);
      populate_unordered_vector(L, idx, &meta_order, &unorder);
      encodeOrderedObject(L, E, idx, depth, &meta_order, &unorder);
    }
    else throwmsg(L, "Invalid __jsonorder result");
  }
  else if ((E->flags & JSON_SORT_KEYS) != 0 || E->order->n != 0) {
    KeyList unorder;
    initkeys(L, &unorder);
    populate_unordered_vector(L, idx, E->order, &unorder);
    if (E->flags & JSON_SORT_KEYS) sortkeys(L, &unorder);
    encodeOrderedObject(L, E, idx, depth, E->order, &unorder);
  }
  else encodeObject(L, E, idx, depth);
  lua_settop(L, top);
}

static void encodeValue (lua_State *L, Encoder *E, int idx, int depth) {
  ljson_Writer *w = E->w;
  if (is_json_null(L, idx)) {
    ljson_write_null(w);
    return;
  }
  switch (lua_type(L, idx)) {
    case LUA_TNIL:
      ljson_write_null(w);
      break;
    case LUA_TBOOLEAN:
      ljson_write_bool(w, lua_toboolean(L, idx));
      break;
    case LUA_TNUMBER: {
      if (lua_isinteger(L, idx)) {
        const lua_Integer i = lua_tointeger(L, idx);
        if (E->flags & JSON_ENCODE_INT32) {
          if (E->flags & JSON_UNSIGNED_INTEGERS) ljson_write_uint(w, (unsigned)i);
          else ljson_write_int(w, (int)i);
        }
        else {
          if (E->flags & JSON_UNSIGNED_INTEGERS) ljson_write_uint(w, (uint64_t)i);
          else ljson_write_int(w, (int64_t)i);
        }
      }
      else {
        const double d = (double)lua_tonumber(L, idx);
        const int is_inf = isnaninf(d);
#if defined(LUA_JSON_COMPAT)
        if (is_inf) ljson_write_null(w);
        else
#endif
        if ((E->flags & JSON_LUA_DTOA) && !is_inf) {
          char buffer[MAXNUMBER2STR + 2] = { 0 };
          const char *end = lua_dtoa(buffer, MAXNUMBER2STR, d);
          ljson_write_raw(w, buffer, (size_t)(end - buffer));
        }
        else if (ljson_write_double(w, ((E->flags & JSON_LUA_GRISU) && !is_inf) ? lua_grisuRound(d) : d) != LJSON_OK) {
          int r = handle_exception(L, E, idx, depth, "error encoding number");
          if (r == 2) lua_error(L);
          else if (r == 0) throwmsg(L, "error encoding: kWriteNanAndInfFlag");
        }
      }
      break;
    }
    case LUA_TVECTOR: {
      lua_Float4 v;
      int args = parseVector(L, idx, &v);
      const int asarray = (E->flags & JSON_ENCODER_ARRAY_VECTOR) != 0;
      if (asarray) ljson_write_start_array(w);
      else ljson_write_start_object(w);
      encodeComponent(L, E, asarray ? NULL : "x", v.x);
      if (args > 1) encodeComponent(L, E, asarray ? NULL : "y", v.y);
      if (args > 2) encodeComponent(L, E, asarray ? NULL : "z", v.z);
      if (args > 3) encodeComponent(L, E, asarray ? NULL : "w", v.w);
      if (asarray) ljson_write_end_array(w);
      else ljson_write_end_object(w);
      break;
    }
    case LUA_TSTRING: {
      size_t len;
      const char *s = lua_tolstring(L, idx, &len);
      ljson_write_string(w, s, len);
      break;
    }
    case LUA_TTABLE:
      encodeTable(L, E, idx, depth + 1);
      break;
    default: {
      if (!encodeMetafield(L, E, idx)) {
        int r = handle_exception(L, E, idx, depth, "unsupported type");
        if (r == 2) lua_error(L);
        else if (r == 0) throwtype(L, ERROR_UNSUPPORTED_TYPE, lua_type(L, idx));
      }
      break;
    }
  }
}

typedef struct Ctx {
  int inarray;
  lua_Integer index;
} Ctx;

typedef struct Decoder {
  lua_State *L;
  lua_Integer flags;
  int nullarg, objectarg, arrayarg;
  Anchor *anchor;
  Ctx *ctx;
  size_t depth, nctx;
  Ctx initctx[32];
} Decoder;

static int submit (Decoder *D) {
  if (D->depth > 0) {
    Ctx *c = &D->ctx[D->depth - 1];
    if (c->inarray) lua_rawseti(D->L, -2, ++c->index);
    else lua_rawset(D->L, -3);
  }
  return 1;
}

static int d_null (void *ud) {
  Decoder *D = (Decoder *)ud;
  if (D->nullarg > 0) lua_pushvalue(D->L, D->nullarg);
  else if (D->flags & JSON_LUA_NULL) lua_pushnil(D->L);
  else lua_pushlightuserdata(D->L, NULL);
  return submit(D);
}

static int d_boolean (void *ud, int b) {
  Decoder *D = (Decoder *)ud;
  lua_pushboolean(D->L, b);
  return submit(D);
}

static int d_integer (void *ud, int64_t i) {
  Decoder *D = (Decoder *)ud;
  lua_pushinteger(D->L, (lua_Integer)i);
  return submit(D);
}

static int d_uinteger (void *ud, uint64_t u) {
  Decoder *D = (Decoder *)ud;
  if (u <= (uint64_t)LUA_MAXINTEGER) lua_pushinteger(D->L, (lua_Integer)u);
  else lua_pushnumber(D->L, (lua_Number)u);
  return submit(D);
}

static int d_number (void *ud, double d) {
  Decoder *D = (Decoder *)ud;
  lua_pushnumber(D->L, (lua_Number)d);
  return submit(D);
}

static int d_string (void *ud, const char *s, size_t len) {
  Decoder *D = (Decoder *)ud;
  lua_pushlstring(D->L, s, len);
  return submit(D);
}

static int d_key (void *ud, const char *s, size_t len) {
  Decoder *D = (Decoder *)ud;
  lua_pushlstring(D->L, s, len);
  return 1;
}

static int d_start (Decoder *D, int inarray) {
  lua_State *L = D->L;
  if (!lua_checkstack(L, 2)) return 0;
  lua_createtable(L, 0, 0);
  lua_pushvalue(L, inarray ? D->arrayarg : D->objectarg);
  lua_setmetatable(L, -2);
  if (D->depth == D->nctx) D->ctx = (Ctx *)growarray(D->anchor, D->ctx, D->initctx, &D->nctx, D->depth + 1, sizeof(Ctx));
  D->ctx[D->depth].inarray = inarray;
  D->ctx[D->depth].index = 0;
  D->depth++;
  return 1;
}

static int d_end (Decoder *D) {
  D->depth--;
  return submit(D);
}

static int d_startobject (void *ud) { return d_start((Decoder *)ud, 0); }

static int d_endobject (void *ud, size_t count) {
  (void)count;
  return d_end((Decoder *)ud);
}

static int d_startarray (void *ud) { return d_start((Decoder *)ud, 1); }

static int d_endarray (void *ud, size_t count) {
  (void)count;
  return d_end((Decoder *)ud);
}

static const ljson_Handler decode_handler = { d_null, d_boolean, d_integer, d_uinteger, d_number, d_string, d_key, d_startobject, d_endobject, d_startarray, d_endarray };

static const char pretty_indent[] = { ' ', '\t', '\n', '\r' };

static const char *const option_keys[] = {
  "",
  "indent", "pretty",
  "sort_keys",
  "null",
  "nesting",
  "unsigned",
  "nan", "inf",
  "bit32",
  "lua_format_float",
  "lua_round_float",
  "vectorarray",
  "single_line",
  "empty_table_as_array",
  "with_hole",
  "decoder_preset",
  "max_depth",
  "indent_char",
  "indent_count", "level",
  "decimal_count",
  "keyorder",
  "exception",
  NULL
};

static const lua_Integer option_keys_num[] = {
  JSON_OPTION_RESERVED,
  JSON_PRETTY_PRINT, JSON_PRETTY_PRINT,
  JSON_SORT_KEYS,
  JSON_LUA_NULL,
  JSON_NESTING_NULL,
  JSON_UNSIGNED_INTEGERS,
  JSON_NAN_AND_INF, JSON_NAN_AND_INF,
  JSON_ENCODE_INT32,
  JSON_LUA_DTOA,
  JSON_LUA_GRISU,
  JSON_ENCODER_ARRAY_VECTOR,
  JSON_ARRAY_SINGLE_LINE,
  JSON_ARRAY_EMPTY,
  JSON_ARRAY_WITH_HOLES,
  JSON_DECODER_PRESET,
  JSON_ENCODER_MAX_DEPTH,
  JSON_ENCODER_INDENT,
  JSON_ENCODER_INDENT_AMT, JSON_ENCODER_INDENT_AMT,
  JSON_ENCODER_DECIMALS,
  JSON_TABLE_KEY_ORDER,
  JSON_ENCODER_HANDLER,
};

static const char *const decode_presets[] = { "default", "extended", NULL };

static const lua_Integer decode_presets_num[] = { JSON_DECODE_DEFAULT, JSON_DECODE_EXTENDED };

static void create_shared_meta (lua_State *L, const char *meta, const char *type) {
  luaL_getsubtable(L, LUA_REGISTRYINDEX, meta);
  lua_pushstring(L, type);
  lua_setfield(L, -2, "__jsontype");
  lua_pop(L, 1);
}

static int make_table_type (lua_State *L, int idx, const char *meta, const char *type) {
  if (lua_isnoneornil(L, idx)) lua_createtable(L, 0, 0);
  else if (lua_istable(L, idx)) {
    lua_pushvalue(L, idx);
    if (lua_getmetatable(L, -1)) {
      lua_pushstring(L, type);
      lua_setfield(L, -2, "__jsontype");
      lua_pop(L, 1);
      return 1;
    }
  }
  else return luaL_argerror(L, idx, "optional table excepted");
  luaL_getmetatable(L, meta);
  lua_setmetatable(L, -2);
  return 1;
}

static int json_encode (lua_State *L) {
  int key_order_idx = 0, error_handler_idx = 0, depth, decimals;
  lua_Integer flags, indent, indent_amt;
  Encoder E;
  KeyList order;
  Anchor anchor;
  ljson_Writer w;
  size_t len;
  const char *result;
  lua_settop(L, 2);
  lua_json_getsubtable(L, LUA_REGISTRYINDEX, "lua_json");
  flags = geti(L, -1, LUA_JSON_REG_FLAGS, JSON_DEFAULT);
  indent = geti(L, -1, LUA_JSON_REG_INDENT, 0);
  indent_amt = geti(L, -1, LUA_JSON_REG_INDENT_AMT, -1);
  depth = (int)geti(L, -1, LUA_JSON_REG_DEPTH, LUA_JSON_DEFAULT_DEPTH);
  decimals = (int)geti(L, -1, LUA_JSON_REG_MAXDEC, LUA_NUMBER_FMT_LEN);
  lua_pop(L, 1);
  if (lua_istable(L, 2)) {
    int has_key_order = 0, has_exception_handler = 0;
    lua_pushnil(L);
    while (lua_next(L, 2)) {
      const lua_Integer opt = (lua_type(L, -2) == LUA_TSTRING) ? option_keys_num[luaL_optcheckoption(L, -2, NULL, option_keys, 0)] : JSON_OPTION_RESERVED;
      lua_Integer v = lua_tointeger(L, -1);
      switch (opt) {
        case JSON_PRETTY_PRINT: case JSON_SORT_KEYS: case JSON_LUA_NULL: case JSON_UNSIGNED_INTEGERS: case JSON_NAN_AND_INF: case JSON_ENCODE_INT32: case JSON_LUA_DTOA: case JSON_LUA_GRISU: case JSON_ENCODER_ARRAY_VECTOR: case JSON_ARRAY_SINGLE_LINE: case JSON_ARRAY_EMPTY: case JSON_ARRAY_WITH_HOLES:
          flags = lua_toboolean(L, -1) ? (flags | opt) : (flags & ~opt);
          break;
        case JSON_ENCODER_MAX_DEPTH:
          if (v <= 0 || v > LUA_JSON_MAX_DEPTH) return luaL_error(L, "invalid encoder depth");
          depth = (int)v;
          break;
        case JSON_ENCODER_DECIMALS:
          if (v <= 0 || v > LJSON_MAXDECIMALS) return luaL_error(L, "invalid decimal count");
          decimals = (int)v;
          break;
        case JSON_ENCODER_INDENT:
          if (v < 0 || v >= 4) return luaL_error(L, "invalid indentation index");
          indent = v;
          break;
        case JSON_ENCODER_INDENT_AMT:
          if (v < 0 || v > 255) return luaL_error(L, "invalid indentation amount");
          indent_amt = v;
          break;
        case JSON_ENCODER_HANDLER:
          has_exception_handler = lua_isfunction(L, -1);
          break;
        case JSON_TABLE_KEY_ORDER:
          has_key_order = lua_istable(L, -1);
          break;
        default:
          break;
      }
      lua_pop(L, 1);
    }
    if (has_exception_handler) {
      lua_getfield(L, 2, "exception");
      error_handler_idx = lua_gettop(L);
    }
    if (has_key_order) {
      lua_getfield(L, 2, "keyorder");
      key_order_idx = lua_gettop(L);
    }
  }
  else if (!lua_isnoneornil(L, 2)) return luaL_error(L, "Argument 2: table or nothing expected");
  if (indent < 0 || indent >= 4 || depth <= 0 || depth > LUA_JSON_MAX_DEPTH) return luaL_error(L, "invalid encoder parameters");
  if (indent_amt < 0) indent_amt = (indent == 0) ? 4 : 1;
  initkeys(L, &order);
  if (key_order_idx > 0) populate_key_vector(L, key_order_idx, &order);
  initanchor(L, &anchor);
  ljson_writer_init(&w, anchoralloc, &anchor);
  w.flags = 0;
  if (flags & JSON_PRETTY_PRINT) {
    w.flags |= LJSON_WRITE_PRETTY;
    if (flags & JSON_ARRAY_SINGLE_LINE) w.flags |= LJSON_WRITE_SINGLE_LINE_ARRAY;
    w.indentchar = pretty_indent[indent];
    w.indentcount = (unsigned)indent_amt;
  }
  if (flags & JSON_NAN_AND_INF) w.flags |= LJSON_WRITE_NAN_INF;
  w.maxdecimals = decimals;
  E.flags = flags;
  E.max_depth = depth;
  E.error_handler_idx = error_handler_idx;
  E.replacements = 0;
  E.order = &order;
  E.w = &w;
  encodeValue(L, &E, 1, 0);
  result = ljson_writer_result(&w, &len);
  if (result == NULL) return luaL_error(L, "not enough memory");
  lua_pushlstring(L, result, len);
  ljson_writer_free(&w);
  freeanchor(&anchor);
  luaL_checkhook(L, lua_rawlen(L, -1));
  return 1;
}

static int json_decode (lua_State *L) {
  Decoder D;
  Anchor anchor;
  ljson_Reader r;
  lua_Integer flags, parsemode;
  const char *contents = NULL;
  size_t len = 0, position, offset = 0;
  int top, error;
  lua_json_getsubtable(L, LUA_REGISTRYINDEX, "lua_json");
  flags = geti(L, -1, LUA_JSON_REG_FLAGS, JSON_DEFAULT);
  parsemode = geti(L, -1, LUA_JSON_REG_PRESET, JSON_DECODE_DEFAULT);
  lua_pop(L, 1);
  if (lua_type(L, 1) != LUA_TNIL) contents = luaL_checklstring(L, 1, &len);
  position = luaL_optsizet(L, 2, 1);
  D.L = L;
  D.flags = flags;
  D.nullarg = (lua_gettop(L) >= 3) ? 3 : -1;
  D.objectarg = lua_istable(L, 4) ? 4 : -1;
  D.arrayarg = lua_istable(L, 5) ? 5 : -1;
  if (len == 0) {
    lua_pushnil(L);
    lua_pushinteger(L, 0);
    lua_pushfstring(L, "%s (%d)", ljson_errormessage(LJSON_ERROR_DOCUMENT_EMPTY), 0);
    return 3;
  }
  else if (position == 0 || position > len) return luaL_error(L, "invalid position");
  top = lua_gettop(L);
  if (D.objectarg < 0) {
    luaL_getmetatable(L, "lua_json_object");
    D.objectarg = lua_gettop(L);
  }
  if (D.arrayarg < 0) {
    luaL_getmetatable(L, "lua_json_array");
    D.arrayarg = lua_gettop(L);
  }
  initanchor(L, &anchor);
  D.anchor = &anchor;
  D.ctx = D.initctx;
  D.depth = 0;
  D.nctx = sizeof(D.initctx) / sizeof(D.initctx[0]);
  r.flags = LJSON_PARSE_TRAILING_COMMAS | LJSON_PARSE_NAN_INF;
  if (parsemode == JSON_DECODE_EXTENDED) r.flags |= LJSON_PARSE_FULL_PRECISION | LJSON_PARSE_COMMENTS;
#if defined(LUA_JSON_COMPAT)
  r.flags |= LJSON_PARSE_STOP_WHEN_DONE | LJSON_PARSE_LENIENT;
#endif
  r.maxdepth = 0;
  r.alloc = anchoralloc;
  r.allocud = &anchor;
  error = ljson_parse(&r, contents + (position - 1), len - (position - 1), &decode_handler, &D, &offset);
  freeanchor(&anchor);

  offset += position - 1;
  if (error != LJSON_OK) {
    lua_settop(L, top);
#if defined(LUA_JSON_EXPLICIT)
    lua_pushfstring(L, "%s (%I)", ljson_errormessage(error), (LUAI_UACINT)offset);
    return lua_error(L);
#else
    lua_pushnil(L);
    lua_pushinteger(L, (lua_Integer)offset);
    lua_pushfstring(L, "%s (%I)", ljson_errormessage(error), (LUAI_UACINT)offset);
    return 3;
#endif
  }
  lua_pushinteger(L, 1 + (lua_Integer)offset);
  luaL_checkhook(L, len);
  return 2;
}

static int json_setoption (lua_State *L) {
  lua_Integer v = 0;
  const lua_Integer opt = option_keys_num[luaL_checkoption(L, 1, NULL, option_keys)];
  lua_settop(L, 2);
  lua_json_getsubtable(L, LUA_REGISTRYINDEX, "lua_json");
  switch (opt) {
    case JSON_PRETTY_PRINT: case JSON_SORT_KEYS: case JSON_LUA_NULL: case JSON_UNSIGNED_INTEGERS: case JSON_NAN_AND_INF: case JSON_ENCODE_INT32: case JSON_LUA_DTOA: case JSON_LUA_GRISU: case JSON_ENCODER_ARRAY_VECTOR: case JSON_ARRAY_SINGLE_LINE: case JSON_ARRAY_EMPTY: case JSON_ARRAY_WITH_HOLES: {
      v = geti(L, -1, LUA_JSON_REG_FLAGS, JSON_DEFAULT);
      luaL_checktype(L, 2, LUA_TBOOLEAN);
      seti(L, -1, LUA_JSON_REG_FLAGS, lua_toboolean(L, 2) ? (v | opt) : (v & ~opt));
      break;
    }
    case JSON_ENCODER_MAX_DEPTH:
      v = luaL_checkinteger(L, 2);
      luaL_argcheck(L, v > 0 && v <= LUA_JSON_MAX_DEPTH, 2, "invalid encoder depth");
      seti(L, -1, LUA_JSON_REG_DEPTH, v);
      break;
    case JSON_ENCODER_INDENT:
      v = luaL_checkinteger(L, 2);
      luaL_argcheck(L, v >= 0 && v < 4, 2, "invalid indentation index");
      seti(L, -1, LUA_JSON_REG_INDENT, v);
      break;
    case JSON_ENCODER_INDENT_AMT:
      v = luaL_checkinteger(L, 2);
      luaL_argcheck(L, v >= 0 && v <= 255, 2, "invalid indentation amount");
      seti(L, -1, LUA_JSON_REG_INDENT_AMT, v);
      break;
    case JSON_ENCODER_DECIMALS:
      v = luaL_checkinteger(L, 2);
      luaL_argcheck(L, v > 0 && v <= LJSON_MAXDECIMALS, 2, "invalid decimal count");
      seti(L, -1, LUA_JSON_REG_MAXDEC, v);
      break;
    case JSON_DECODER_PRESET:
      v = decode_presets_num[luaL_optcheckoption(L, 2, NULL, decode_presets, 0)];
      seti(L, -1, LUA_JSON_REG_PRESET, v);
      break;
    default:
      break;
  }
  lua_pop(L, 1);
  return 0;
}

static int json_getoption (lua_State *L) {
  lua_Integer v = 0;
  const lua_Integer opt = option_keys_num[luaL_checkoption(L, 1, NULL, option_keys)];
  lua_json_getsubtable(L, LUA_REGISTRYINDEX, "lua_json");
  switch (opt) {
    case JSON_PRETTY_PRINT: case JSON_SORT_KEYS: case JSON_LUA_NULL: case JSON_UNSIGNED_INTEGERS: case JSON_NAN_AND_INF: case JSON_ENCODE_INT32: case JSON_LUA_DTOA: case JSON_LUA_GRISU: case JSON_ENCODER_ARRAY_VECTOR: case JSON_ARRAY_SINGLE_LINE: case JSON_ARRAY_EMPTY: case JSON_ARRAY_WITH_HOLES:
      v = geti(L, -1, LUA_JSON_REG_FLAGS, JSON_DEFAULT);
      lua_pushboolean(L, (v & opt) != 0);
      break;
    case JSON_ENCODER_MAX_DEPTH:
      lua_pushinteger(L, geti(L, -1, LUA_JSON_REG_DEPTH, LUA_JSON_DEFAULT_DEPTH));
      break;
    case JSON_ENCODER_INDENT:
      lua_pushinteger(L, geti(L, -1, LUA_JSON_REG_INDENT, 0));
      break;
    case JSON_ENCODER_INDENT_AMT:
      v = geti(L, -1, LUA_JSON_REG_INDENT_AMT, -1);
      lua_pushinteger(L, (v >= 0) ? v : (geti(L, -1, LUA_JSON_REG_INDENT, 0) == 0) ? 4 : 1);
      break;
    case JSON_ENCODER_DECIMALS:
      lua_pushinteger(L, geti(L, -1, LUA_JSON_REG_MAXDEC, LUA_NUMBER_FMT_LEN));
      break;
    case JSON_DECODER_PRESET: {
      v = geti(L, -1, LUA_JSON_REG_PRESET, JSON_DECODE_DEFAULT);
      if (JSON_DECODE_DEFAULT <= v && v <= JSON_DECODE_EXTENDED) lua_pushstring(L, decode_presets[v]);
      else lua_pushnil(L);
      break;
    }
    default:
      lua_pop(L, 1);
      return 0;
  }
  return 1;
}

static int json_object (lua_State *L) {
  make_table_type(L, 1, "lua_json_object", "object");
  return 1;
}

static int json_array (lua_State *L) {
  make_table_type(L, 1, "lua_json_array", "array");
  return 1;
}

static int json_isobject (lua_State *L) {
  int is_array = 0;
  lua_pushboolean(L, has_json_type(L, 1, &is_array) && !is_array);
  return 1;
}

static int json_isarray (lua_State *L) {
  int is_array = 0;
  lua_pushboolean(L, has_json_type(L, 1, &is_array) && is_array);
  return 1;
}

LUAMOD_API int luaopen_json (lua_State *L) {
  static const luaL_Reg luajson_lib[] = {
    { "decode", json_decode },
    { "encode", json_encode },
    { "setoption", json_setoption },
    { "getoption", json_getoption },
    { "null", NULL },
    { "object", json_object },
    { "array", json_array },
    { "isobject", json_isobject },
    { "isarray", json_isarray },
    { "_COMPAT", NULL },
    { NULL, NULL }
  };
  if (luaL_newmetatable(L, LUA_JSON_GUARD)) {
    lua_pushcfunction(L, guard_gc);
    lua_setfield(L, -2, "__gc");
  }
  lua_pop(L, 1);
  create_shared_meta(L, "lua_json_array", "array");
  create_shared_meta(L, "lua_json_object", "object");
  luaL_newlib(L, luajson_lib);
  lua_pushlightuserdata(L, NULL);
  lua_setfield(L, -2, "null");
#if defined(LUA_JSON_COMPAT)
  lua_pushboolean(L, 1);
#else
  lua_pushboolean(L, 0);
#endif
  lua_setfield(L, -2, "_COMPAT");
  return 1;
}
