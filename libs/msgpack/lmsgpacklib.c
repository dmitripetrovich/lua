#define lmsgpacklib_c
#define LUA_LIB

#include <stdint.h>
#include <string.h>
#include <limits.h>

#if defined(LUA_COMPILED_AS_HPP)
extern "C++" {
#endif

#include <lua.h>
#include <luaconf.h>
#include <lauxlib.h>
#include <lgrit_lib.h>

#if defined(LUA_COMPILED_AS_HPP)
}
#endif

#include "lmsgpack.h"

#if LUA_VERSION_NUM < 504
  #error unsupported Lua version
#endif

#if !defined(MP_MAX_NESTING)
  #define MP_MAX_NESTING 16
#endif

#if !defined(MP_TABLE_CUTOFF)
  #define MP_TABLE_CUTOFF 16
#endif

#if !defined(MP_BUFFER_INITSIZE)
  #define MP_BUFFER_INITSIZE 32
#endif

#if !defined(MP_STACK_BUFFERSIZE)
  #define MP_STACK_BUFFERSIZE 512
#endif

#if defined(__cplusplus)
  #define mp_nullptr nullptr
  #define mp_cast(t, exp) static_cast<t>(exp)
  #define mp_pcast(t, exp) reinterpret_cast<t>(exp)
#else
  #define mp_nullptr NULL
  #define mp_cast(t, exp) ((t)(exp))
  #define mp_pcast(t, exp) ((t)(exp))
#endif

#define mp_checkstack(L, sz) luaL_checkstack((L), (sz), "too many (nested) values in encoded msgpack")

#define MP_MAX_SIZET (~mp_cast(size_t, 0))
#define MP_MAX_LUAINDEX (sizeof(size_t) < sizeof(lua_Integer) ? MP_MAX_SIZET : mp_cast(size_t, LUA_MAXINTEGER))

#define LUA_MSGPACK_META_MTYPE "__ext"
#define LUA_MSGPACK_META_ENCODE "__pack"
#define LUA_MSGPACK_META_DECODE "__unpack"

#define LUA_MSGPACK_USERDATA "LUA_MSGPACK"

#define MP_PACKING             0x01
#define MP_UNPACKING           0x02

#define MP_UNSIGNED_INTEGERS   0x10

#define MP_NUMBER_AS_INTEGER   0x20
#define MP_NUMBER_AS_FLOAT     0x40
#define MP_NUMBER_AS_DOUBLE    0x80

#define MP_STRING_COMPAT       0x100
#define MP_STRING_BINARY       0x200

#define MP_EMPTY_AS_ARRAY      0x400
#define MP_ARRAY_AS_MAP        0x800
#define MP_ARRAY_WITH_HOLES    0x1000
#define MP_ARRAY_WITHOUT_HOLES 0x2000

#define MP_SMALL_LUA           0x4000
#define MP_FULL_64_BITS        0x8000
#define MP_LONG_DOUBLE         0x10000

#define MP_USE_SENTINEL        0x20000
#define MP_IGNORE_INVALID      0x40000

#define MP_MODE (MP_PACKING | MP_UNPACKING)
#define MP_MASK_RUNTIME (MP_MODE)
#define MP_MASK_ARRAY (MP_ARRAY_AS_MAP | MP_ARRAY_WITH_HOLES | MP_ARRAY_WITHOUT_HOLES)
#define MP_MASK_STRING (MP_STRING_COMPAT | MP_STRING_BINARY)
#define MP_MASK_NUMBER (MP_NUMBER_AS_INTEGER | MP_NUMBER_AS_FLOAT | MP_NUMBER_AS_DOUBLE)

#if LUA_FLOAT_TYPE == LUA_FLOAT_FLOAT
  #define MP_DEFAULT (MP_EMPTY_AS_ARRAY | MP_UNSIGNED_INTEGERS | MP_ARRAY_WITHOUT_HOLES | MP_NUMBER_AS_FLOAT)
#else
  #define MP_DEFAULT (MP_EMPTY_AS_ARRAY | MP_UNSIGNED_INTEGERS | MP_ARRAY_WITHOUT_HOLES | MP_NUMBER_AS_DOUBLE)
#endif

#define MSGPACK_EXT_VECTOR2       20
#define MSGPACK_EXT_VECTOR3       21
#define MSGPACK_EXT_VECTOR4       22
#define MSGPACK_EXT_QUATERNION    23

#define LUA_MSGPACK_EXT_VALID(i) (INT8_MIN <= (i) && (i) <= INT8_MAX)
#define LUA_MSGPACK_EXT_RESERVED(i) ((i) == MSGPACK_EXT_VECTOR2 || (i) == MSGPACK_EXT_VECTOR3 || (i) == MSGPACK_EXT_VECTOR4 || (i) == MSGPACK_EXT_QUATERNION)

#define LUA_MSGPACK_LUA_TYPE(x) ((x) + 1000)

#define EXT_INVALID -1024

typedef struct lua_msgpack {
  lua_Integer flags;
  lmsgpack_Writer w;
} lua_msgpack;

#include "lmsgpacklib.h"

#define LUA_MSGPACK_REG_OPTIONS "lua_msgpack_flags"
#define LUA_MSGPACK_REG_EXT "lua_msgpack_meta"

#define MP_STRINGIFY_(x) #x
#define MP_STRINGIFY(x) MP_STRINGIFY_(x)

#define mp_is_packing(ud) (((ud)->flags & MP_MODE) == MP_PACKING)

static lua_Integer mp_getregi (lua_State *L, const char *key, lua_Integer opt) {
  lua_Integer result;
  lua_getfield(L, LUA_REGISTRYINDEX, key);
  result = luaL_optinteger(L, -1, opt);
  lua_pop(L, 1);
  return result;
}

static void mp_setregi (lua_State *L, const char *key, lua_Integer value) {
  lua_pushinteger(L, value);
  lua_setfield(L, LUA_REGISTRYINDEX, key);
}

static void mp_getregt (lua_State *L, const char *name) {
  if (lua_getfield(L, LUA_REGISTRYINDEX, name) != LUA_TTABLE) {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, name);
  }
}

static int typetoindex (lua_State *L, const char *name) {
  int i, found = -1;
  if (name == mp_nullptr)
    return -1;
#if defined(LUA_NUMTAGS)
  for (i = 0; i < LUA_NUMTAGS; ++i) {
#else
  for (i = 0; i < (LUA_TTHREAD + 1); ++i) {
#endif
    if (i != LUA_TLIGHTUSERDATA && strcmp(lua_typename(L, i), name) == 0) {
      found = i;
      break;
    }
  }
  if (found == -1 && strcmp("lightuserdata", name) == 0)
    found = LUA_TLIGHTUSERDATA;
  return found;
}

static size_t mp_optsizet (lua_State *L, int arg, size_t def) {
  if (lua_isnoneornil(L, arg))
    return def;
  else if (lua_isinteger(L, arg)) {
    const lua_Integer i = lua_tointeger(L, arg);
    if (i >= 0 && mp_cast(size_t, i) <= MP_MAX_LUAINDEX)
      return mp_cast(size_t, i);
    luaL_argerror(L, arg, "invalid integer argument");
    return 0;
  }
  luaL_argerror(L, arg, lua_pushfstring(L, "integer expected"));
  return 0;
}

#if LUA_VEC_TYPE == LUA_FLOAT_FLOAT
  typedef uint32_t lua_VecI;
#elif LUA_VEC_TYPE == LUA_FLOAT_DOUBLE
  typedef uint64_t lua_VecI;
#else
  #error "unsupported vector type"
#endif

static void mp_storevec (unsigned char *p, lua_VecF f) {
  lua_VecI i;
  size_t k;
  memcpy(&i, &f, sizeof(i));
  for (k = 0; k < sizeof(i); k++) {
#if defined(LUA_MSGPACK_NATIVE_VECTOR)
    p[sizeof(i) - 1 - k] = mp_cast(unsigned char, i >> (8 * k));
#else
    p[k] = mp_cast(unsigned char, i >> (8 * k));
#endif
  }
}

static lua_VecF mp_loadvec (const unsigned char *p) {
  lua_VecI i = 0;
  lua_VecF f;
  size_t k;
  for (k = 0; k < sizeof(i); k++) {
#if defined(LUA_MSGPACK_NATIVE_VECTOR)
    i |= mp_cast(lua_VecI, p[sizeof(i) - 1 - k]) << (8 * k);
#else
    i |= mp_cast(lua_VecI, p[k]) << (8 * k);
#endif
  }
  memcpy(&f, &i, sizeof(f));
  return f;
}

static void mp_pushnull (lua_State *L) {
  lua_pushlightuserdata(L, mp_nullptr);
}

static int mp_is_null (lua_State *L, int idx) {
  return lua_type(L, idx) == LUA_TLIGHTUSERDATA && lua_touserdata(L, idx) == mp_nullptr;
}

static void mp_replace_null (lua_State *L) {
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    mp_pushnull(L);
  }
}

typedef struct mp_encoder {
  lua_State *L;
  lmsgpack_Writer *w;
  lua_Integer flags;
  int box;
  int ext;
  int fetched;
} mp_encoder;

static int mp_boxreserve (lmsgpack_Writer *w, size_t need) {
  mp_encoder *e = mp_pcast(mp_encoder *, w->ud);
  lua_State *L = e->L;
  size_t size = w->cap * 2;
  char *b;
  if (MP_MAX_SIZET - need < w->len)
    luaL_error(L, "buffer too large");
  if (size < w->len + need)
    size = w->len + need;
  b = mp_pcast(char *, lua_newuserdatauv(L, size, 0));
  memcpy(b, w->buf, w->len);
  lua_replace(L, e->box);
  w->buf = b;
  w->cap = size;
  return LMSGPACK_OK;
}

static int mp_heapreserve (lmsgpack_Writer *w, size_t need) {
  lua_State *L = mp_pcast(lua_State *, w->ud);
  void *ud;
  lua_Alloc f = lua_getallocf(L, &ud);
  size_t size = w->cap * 2;
  char *b;
  if (MP_MAX_SIZET - need < w->len)
    luaL_error(L, "buffer too large");
  if (size < w->len + need)
    size = w->len + need;
  if ((b = mp_pcast(char *, f(ud, w->buf, w->cap, size))) == mp_nullptr)
    luaL_error(L, "unable to allocate buffer");
  w->buf = b;
  w->cap = size;
  return LMSGPACK_OK;
}

static void mp_encoder_init (lua_State *L, mp_encoder *e, lmsgpack_Writer *w, char *init, size_t size) {
  e->L = L;
  e->w = w;
  e->flags = mp_getregi(L, LUA_MSGPACK_REG_OPTIONS, MP_DEFAULT) & ~MP_MASK_RUNTIME;
  e->fetched = 0;
  lua_pushnil(L);
  e->box = lua_gettop(L);
  lua_pushnil(L);
  e->ext = lua_gettop(L);
  lmsgpack_writer_initbuffer(w, init, size, mp_boxreserve, e);
}

static void mp_toolarge (mp_encoder *e) {
  e->w->status = LMSGPACK_OK;
  luaL_error(e->L, "object too large to msgpack");
}

static void mp_fetch (mp_encoder *e) {
  if (!e->fetched) {
    mp_getregt(e->L, LUA_MSGPACK_REG_EXT);
    lua_replace(e->L, e->ext);
    e->fetched = 1;
  }
}

static void mp_encode_any (mp_encoder *e, int idx, int level);

static void mp_write_ext (mp_encoder *e, const char *s, size_t len, int8_t type) {
  if (lmsgpack_write_ext(e->w, len, type) != 0)
    mp_toolarge(e);
  lmsgpack_write_bytes(e->w, s, len);
}

static void mp_encode_integer (mp_encoder *e, int idx) {
  int64_t i = mp_cast(int64_t, lua_tointeger(e->L, idx));
  if (e->flags & MP_UNSIGNED_INTEGERS)
    lmsgpack_write_int(e->w, i);
  else
    lmsgpack_write_sint(e->w, i);
}

static void mp_encode_number (mp_encoder *e, int idx) {
  int isint = 0;
  if (lua_isinteger(e->L, idx) || ((e->flags & MP_NUMBER_AS_INTEGER) && (lua_tointegerx(e->L, idx, &isint), isint)))
    mp_encode_integer(e, idx);
  else if (e->flags & MP_NUMBER_AS_FLOAT)
    lmsgpack_write_float(e->w, mp_cast(float, lua_tonumber(e->L, idx)));
  else
    lmsgpack_write_double(e->w, mp_cast(double, lua_tonumber(e->L, idx)));
}

static void mp_encode_string (mp_encoder *e, int idx) {
  size_t len = 0;
  const char *s = lua_tolstring(e->L, idx, &len);
  int r;
  if (s == mp_nullptr) {
    len = 0;
    s = "";
  }
  if ((e->flags & MP_STRING_COMPAT) == MP_STRING_COMPAT)
    r = lmsgpack_write_str_compat(e->w, len);
  else if ((e->flags & MP_STRING_BINARY) == MP_STRING_BINARY)
    r = lmsgpack_write_bin(e->w, len);
  else
    r = lmsgpack_write_str(e->w, len);
  if (r != 0)
    mp_toolarge(e);
  lmsgpack_write_bytes(e->w, s, len);
}

static void mp_encode_vector (mp_encoder *e, int idx) {
  unsigned char buf[4 * sizeof(lua_VecF)];
  lua_Float4 v;
  int type, size;
  switch (lua_tofloat4(e->L, idx, V_NOTABLE, &v)) {
    case LUA_VVECTOR2: type = MSGPACK_EXT_VECTOR2; size = 2; break;
    case LUA_VVECTOR3: type = MSGPACK_EXT_VECTOR3; size = 3; break;
    case LUA_VVECTOR4: type = MSGPACK_EXT_VECTOR4; size = 4; break;
    case LUA_VQUAT: type = MSGPACK_EXT_QUATERNION; size = 4; break;
    default: luaL_error(e->L, "invalid vector type"); return;
  }
  mp_storevec(&buf[0 * sizeof(lua_VecF)], v.x);
  mp_storevec(&buf[1 * sizeof(lua_VecF)], v.y);
  if (size > 2)
    mp_storevec(&buf[2 * sizeof(lua_VecF)], v.z);
  if (size > 3)
    mp_storevec(&buf[3 * sizeof(lua_VecF)], v.w);
  mp_write_ext(e, mp_pcast(const char *, buf), mp_cast(size_t, size) * sizeof(lua_VecF), mp_cast(int8_t, type));
}

static int mp_encode_ext_metatable (mp_encoder *e, int idx, int8_t ext_id, int8_t has_ext) {
  lua_State *L = e->L;
  const int type = luaL_getmetafield(L, idx, LUA_MSGPACK_META_ENCODE);
  if (type == LUA_TNIL)
    return 0;
  else if (type != LUA_TFUNCTION) {
    lua_pop(L, 1);
    return 0;
  }
  lua_pushvalue(L, idx);
  lua_pushinteger(L, mp_cast(lua_Integer, ext_id));
  lua_call(L, 2, 2);
  if (lua_type(L, -2) == LUA_TSTRING) {
    size_t len = 0;
    const char *s = lua_tolstring(L, -2, &len);
    if (lua_toboolean(L, -1))
      lmsgpack_write_bytes(e->w, s, len);
    else if (has_ext)
      mp_write_ext(e, s, len, ext_id);
    else {
      lua_pop(L, 2);
      return luaL_error(L, "unregistered extension identifier must return <encoding, true>");
    }
    lua_pop(L, 2);
    return 1;
  }
  lua_pop(L, 2);
  return luaL_error(L, "invalid encoder result from encoder <%d>", mp_cast(int, ext_id));
}

static lua_Integer mp_ext_type (lua_State *L, int idx) {
  lua_Integer type = EXT_INVALID;
  if (luaL_getmetafield(L, idx, LUA_MSGPACK_META_MTYPE) != LUA_TNIL) {
    if (lua_isinteger(L, -1)) {
      type = lua_tointeger(L, -1);
      type = LUA_MSGPACK_EXT_VALID(type) ? type : EXT_INVALID;
    }
    lua_pop(L, 1);
  }
  return type;
}

#define EXT_INDIRECT_MAX 5

static int mp_encode_registered_ext_type (mp_encoder *e, int idx, int8_t ext_id) {
  lua_State *L = e->L;
  int i;
  mp_checkstack(L, 5);
  if (mp_encode_ext_metatable(e, idx, ext_id, 1))
    return 1;
  mp_fetch(e);
  for (i = 0; i < EXT_INDIRECT_MAX; ++i) {
    const int type = lua_rawgeti(L, e->ext, ext_id);
    if (type == LUA_TTABLE) {
      if (lua_getfield(L, -1, LUA_MSGPACK_META_ENCODE) == LUA_TFUNCTION) {
        lua_remove(L, -2);
        lua_pushvalue(L, idx);
        lua_pushinteger(L, mp_cast(lua_Integer, ext_id));
        lua_call(L, 2, 2);
        if (lua_type(L, -2) == LUA_TSTRING) {
          size_t len = 0;
          const char *s = lua_tolstring(L, -2, &len);
          if (lua_toboolean(L, -1))
            lmsgpack_write_bytes(e->w, s, len);
          else
            mp_write_ext(e, s, len, ext_id);
          lua_pop(L, 2);
          return 1;
        }
        lua_pop(L, 2);
        return luaL_error(L, "invalid encoder result from encoder <%d>", ext_id);
      }
      lua_pop(L, 2);
      return luaL_error(L, "msgpack extension type: invalid encoder");
    }
    else if (type == LUA_TNUMBER) {
      lua_Integer ext = lua_tointeger(L, -1);
      lua_pop(L, 1);
      if (ext == ext_id)
        return luaL_error(L, "msgpack extension type: invalid encoder");
      else if (!LUA_MSGPACK_EXT_VALID(ext))
        return luaL_error(L, "msgpack extension type: invalid identifier");
      else if (i == (EXT_INDIRECT_MAX - 1))
        return luaL_error(L, "msgpack extension type: invalid identifier associations");
      ext_id = mp_cast(int8_t, ext);
    }
    else {
      lua_pop(L, 1);
      break;
    }
  }
  return 0;
}

static int mp_encode_registered_lua_type (mp_encoder *e, int idx, int type) {
  lua_State *L = e->L;
  mp_checkstack(L, 2);
  mp_fetch(e);
  if (lua_rawgeti(L, e->ext, LUA_MSGPACK_LUA_TYPE(type)) == LUA_TNUMBER) {
    const int8_t ext_id = mp_cast(int8_t, lua_tonumber(L, -1));
    lua_pop(L, 1);
    return mp_encode_registered_ext_type(e, idx, ext_id);
  }
  lua_pop(L, 1);
  return 0;
}

static int mp_table_is_an_array (lua_State *L, int idx, lua_Integer flags, size_t *array_length, size_t *pairs) {
  size_t count = 0, max = 0, arraylen = 0, entries = 0;
  int isarray = 1;
  const int stacktop = lua_gettop(L);
  mp_checkstack(L, 2);
  lua_pushnil(L);
  while (lua_next(L, idx)) {
    lua_Integer n;
#if !defined(LUA_MSGPACK_COMPAT)
    size_t keylen = 0;
    const char *key = mp_nullptr;
#endif
    lua_checkhook(L, 1);
    entries++;
    if (isarray && lua_isinteger(L, -2) && ((n = lua_tointeger(L, -2)) >= 1 && mp_cast(size_t, n) <= MP_MAX_LUAINDEX)) {
      count++;
      max = mp_cast(size_t, n) > max ? mp_cast(size_t, n) : max;
    }
#if !defined(LUA_MSGPACK_COMPAT)
    else if (isarray && lua_type(L, -2) == LUA_TSTRING && lua_isinteger(L, -1) && (n = lua_tointeger(L, -1), (n >= 1 && mp_cast(size_t, n) <= MP_MAX_LUAINDEX)) && (key = lua_tolstring(L, -2, &keylen), key != mp_nullptr) && keylen == 1 && key[0] == 'n') {
      arraylen = mp_cast(size_t, n);
    }
#endif
    else
      isarray = 0;
    lua_pop(L, 1);
  }
  lua_settop(L, stacktop);
  *pairs = entries;
  if (!isarray)
    return 0;
  if (arraylen > 0 && arraylen <= 2 * count + MP_TABLE_CUTOFF)
    max = arraylen > max ? arraylen : max;
  else if (arraylen > 0)
    return 0;
  *array_length = max;
  if (max == count)
    return max > 0 || (flags & MP_EMPTY_AS_ARRAY);
  else if (flags & MP_ARRAY_WITH_HOLES)
    return ((max <= MP_TABLE_CUTOFF) || max <= arraylen || (count >= (max >> 1)));
  return 0;
}

static void mp_encode_array (mp_encoder *e, int idx, int level, size_t array_length) {
  lua_State *L = e->L;
  size_t j;
  luaL_checktype(L, idx, LUA_TTABLE);
  if (lmsgpack_write_array(e->w, array_length) != 0)
    mp_toolarge(e);
  mp_checkstack(L, 1);
  for (j = 1; j <= array_length; j++) {
    lua_checkhook(L, 1);
    lua_rawgeti(L, idx, mp_cast(lua_Integer, j));
    mp_encode_any(e, lua_gettop(L), level + 1);
    lua_pop(L, 1);
  }
}

static size_t mp_count_pairs (lua_State *L, int idx) {
  size_t len = 0;
  mp_checkstack(L, 2);
  lua_pushnil(L);
  while (lua_next(L, idx)) {
    len++;
    lua_checkhook(L, 1);
    lua_pop(L, 1);
  }
  return len;
}

static void mp_encode_map (mp_encoder *e, int idx, int level, size_t len) {
  lua_State *L = e->L;
  size_t written = 0;
  luaL_checktype(L, idx, LUA_TTABLE);
  if (lmsgpack_write_map(e->w, len) != 0)
    mp_toolarge(e);
  mp_checkstack(L, 3);
  lua_pushnil(L);
  while (lua_next(L, idx)) {
    int top = lua_gettop(L);
    lua_checkhook(L, 1);
    if (++written > len)
      luaL_error(L, "table changed while being msgpack'd");
    mp_encode_any(e, top - 1, level + 1);
    mp_encode_any(e, top, level + 1);
    lua_pop(L, 1);
  }
  if (written != len)
    luaL_error(L, "table changed while being msgpack'd");
}

static void mp_encode_table (mp_encoder *e, int idx, int level) {
  size_t array_length = 0, pairs = 0;
  luaL_checktype(e->L, idx, LUA_TTABLE);
  if ((e->flags & MP_ARRAY_AS_MAP) == MP_ARRAY_AS_MAP)
    mp_encode_map(e, idx, level, mp_count_pairs(e->L, idx));
  else if (mp_table_is_an_array(e->L, idx, e->flags, &array_length, &pairs))
    mp_encode_array(e, idx, level, array_length);
  else
    mp_encode_map(e, idx, level, pairs);
}

static void mp_encode_extended_table (mp_encoder *e, int idx, int level) {
  lua_Integer type = mp_ext_type(e->L, idx);
  if (type != EXT_INVALID) {
    if (!mp_encode_registered_ext_type(e, idx, mp_cast(int8_t, type)))
      luaL_error(e->L, "msgpack extension type: not registered!");
  }
  else if (mp_encode_ext_metatable(e, idx, 0, 0)) {
  }
  else if (mp_encode_registered_lua_type(e, idx, LUA_TTABLE)) {
  }
  else
    mp_encode_table(e, idx, level);
}

static void mp_encode_extended (mp_encoder *e, int idx) {
  lua_State *L = e->L;
  const int t = lua_type(L, idx);
  lua_Integer type = mp_ext_type(L, idx);
  if (type != EXT_INVALID) {
    if (!mp_encode_registered_ext_type(e, idx, mp_cast(int8_t, type))) {
      if ((e->flags & MP_IGNORE_INVALID))
        lmsgpack_write_nil(e->w);
      else
        luaL_error(L, "msgpack extension type: not registered!");
    }
  }
  else if (mp_encode_ext_metatable(e, idx, 0, 0)) {
  }
  else if (mp_encode_registered_lua_type(e, idx, t)) {
  }
  else if ((e->flags & MP_IGNORE_INVALID))
    lmsgpack_write_nil(e->w);
  else
    luaL_error(L, "type <%s> cannot be msgpack'd", lua_typename(L, t));
}

static void mp_encode_any (mp_encoder *e, int idx, int level) {
  int t = lua_type(e->L, idx);
  if (t == LUA_TTABLE && level >= MP_MAX_NESTING) {
#if defined(LUA_MSGPACK_ERROR_NESTING)
    luaL_error(e->L, "maximum table nesting depth (%d) exceeded", MP_MAX_NESTING);
    return;
#else
    t = LUA_TNIL;
#endif
  }
  switch (t) {
    case LUA_TNIL: lmsgpack_write_nil(e->w); break;
    case LUA_TBOOLEAN: lmsgpack_write_bool(e->w, lua_toboolean(e->L, idx)); break;
    case LUA_TNUMBER: mp_encode_number(e, idx); break;
    case LUA_TSTRING: mp_encode_string(e, idx); break;
    case LUA_TTABLE: mp_encode_extended_table(e, idx, level); break;
    case LUA_TLIGHTUSERDATA: {
      if (mp_is_null(e->L, idx))
        lmsgpack_write_nil(e->w);
      else
        mp_encode_extended(e, idx);
      break;
    }
    case LUA_TVECTOR: mp_encode_vector(e, idx); break;
    default: mp_encode_extended(e, idx); break;
  }
}

typedef struct mp_decoder {
  lua_State *L;
  const char *s;
  size_t len;
  size_t off;
  lua_Integer flags;
} mp_decoder;

static void mp_decode (mp_decoder *d);

static void mp_decode_vector (lua_State *L, const lmsgpack_Value *o) {
  const unsigned char *p = mp_pcast(const unsigned char *, o->via.ext.ptr);
  const int count = (o->via.ext.type == MSGPACK_EXT_VECTOR2) ? 2 : (o->via.ext.type == MSGPACK_EXT_VECTOR3) ? 3 : 4;
  lua_Float4 v = { 0, 0, 0, 0 };
  int variant;
  const size_t expected = mp_cast(size_t, count) * sizeof(lua_VecF);
  if (o->via.ext.size != expected)
    luaL_error(L, "msgpack vector: expected <%d> actual <%d>", mp_cast(int, expected), mp_cast(int, o->via.ext.size));
  v.x = mp_loadvec(p + 0 * sizeof(lua_VecF));
  v.y = mp_loadvec(p + 1 * sizeof(lua_VecF));
  if (count > 2)
    v.z = mp_loadvec(p + 2 * sizeof(lua_VecF));
  if (count > 3)
    v.w = mp_loadvec(p + 3 * sizeof(lua_VecF));
  switch (o->via.ext.type) {
    case MSGPACK_EXT_VECTOR2: variant = LUA_VVECTOR2; break;
    case MSGPACK_EXT_VECTOR3: variant = LUA_VVECTOR3; break;
    case MSGPACK_EXT_VECTOR4: variant = LUA_VVECTOR4; break;
    default: variant = LUA_VQUAT; break;
  }
  lua_pushfloat4(L, v, variant);
}

static void mp_decode_ext (lua_State *L, const lmsgpack_Value *o) {
  if (o->via.ext.type >= MSGPACK_EXT_VECTOR2 && o->via.ext.type <= MSGPACK_EXT_QUATERNION) {
    mp_decode_vector(L, o);
    return;
  }
  mp_checkstack(L, 4);
  mp_getregt(L, LUA_MSGPACK_REG_EXT);
  if (lua_rawgeti(L, -1, o->via.ext.type) == LUA_TTABLE) {
    if (lua_getfield(L, -1, LUA_MSGPACK_META_DECODE) == LUA_TFUNCTION) {
      lua_insert(L, -3);
      lua_pop(L, 2);
      lua_pushlstring(L, o->via.ext.ptr, o->via.ext.size);
      lua_pushinteger(L, mp_cast(lua_Integer, o->via.ext.type));
      lua_call(L, 2, 1);
    }
    else {
      lua_pop(L, 3);
      luaL_error(L, "msgpack extension type: invalid decoder!");
    }
  }
  else {
    lua_pop(L, 2);
    lua_pushlstring(L, o->via.ext.ptr, o->via.ext.size);
  }
}

static void mp_decode_array (mp_decoder *d, uint32_t size) {
  lua_State *L = d->L;
  uint32_t i;
  lua_createtable(L, (size <= INT_MAX) ? mp_cast(int, size) : 0, 0);
  mp_checkstack(L, 2);
  for (i = 0; i < size; ++i) {
    lua_checkhook(L, 1);
    mp_decode(d);
    if (d->flags & MP_USE_SENTINEL)
      mp_replace_null(L);
    lua_rawseti(L, -2, mp_cast(lua_Integer, i) + 1);
  }
}

static int mp_iskey (lua_State *L) {
  lua_Number n;
  if (lua_isnil(L, -1))
    return 0;
  else if (lua_type(L, -1) != LUA_TNUMBER || lua_isinteger(L, -1))
    return 1;
  n = lua_tonumber(L, -1);
  return n == n;
}

static void mp_decode_map (mp_decoder *d, uint32_t size) {
  lua_State *L = d->L;
  uint32_t i;
  lua_createtable(L, 0, (size <= INT_MAX) ? mp_cast(int, size) : 0);
  mp_checkstack(L, 5);
  for (i = 0; i < size; ++i) {
    lua_checkhook(L, 1);
    mp_decode(d);
    if (d->flags & MP_USE_SENTINEL)
      mp_replace_null(L);
    if (mp_iskey(L)) {
      mp_decode(d);
      lua_rawset(L, -3);
    }
    else {
      lua_pop(L, 1);
      lmsgpack_check(d->s, d->len, &d->off);
    }
  }
}

static void mp_decode (mp_decoder *d) {
  lua_State *L = d->L;
  lmsgpack_Value o;
  lmsgpack_read(d->s, d->len, &d->off, &o);
  switch (o.type) {
    case LMSGPACK_NIL: lua_pushnil(L); break;
    case LMSGPACK_BOOLEAN: lua_pushboolean(L, o.via.boolean); break;
    case LMSGPACK_POSITIVE_INTEGER: {
      if (o.via.u64 > mp_cast(lua_Unsigned, LUA_MAXINTEGER))
        lua_pushnumber(L, mp_cast(lua_Number, o.via.u64));
      else
        lua_pushinteger(L, mp_cast(lua_Integer, o.via.u64));
      break;
    }
    case LMSGPACK_NEGATIVE_INTEGER: {
#if LUA_MAXINTEGER < INT64_MAX
      if (o.via.i64 < LUA_MININTEGER) {
        lua_pushnumber(L, mp_cast(lua_Number, o.via.i64));
        break;
      }
#endif
      lua_pushinteger(L, mp_cast(lua_Integer, o.via.i64));
      break;
    }
    case LMSGPACK_FLOAT32: lua_pushnumber(L, mp_cast(lua_Number, mp_cast(float, o.via.f64))); break;
    case LMSGPACK_FLOAT64: lua_pushnumber(L, mp_cast(lua_Number, o.via.f64)); break;
    case LMSGPACK_STR: lua_pushlstring(L, o.via.str.ptr, o.via.str.size); break;
    case LMSGPACK_BIN: lua_pushlstring(L, o.via.bin.ptr, o.via.bin.size); break;
    case LMSGPACK_ARRAY: mp_decode_array(d, o.via.array.size); break;
    case LMSGPACK_MAP: mp_decode_map(d, o.via.map.size); break;
    case LMSGPACK_EXT: mp_decode_ext(L, &o); break;
    default: lua_pushnil(L); break;
  }
}

static int mp_decode_objects (lua_State *L, lua_Integer flags, const char *s, size_t len, size_t *offset, int limit, const char **error, int *err_code) {
  int done = 0;
  int object_count = 0;
  int err_res = LMSGPACK_OK;
  const char *err_msg = mp_nullptr;
  while (err_msg == mp_nullptr && !done) {
    mp_decoder d;
    d.L = L;
    d.s = s;
    d.len = len;
    d.off = *offset;
    d.flags = flags;
    switch ((err_res = lmsgpack_check(s, len, offset))) {
      case LMSGPACK_OK:
        mp_checkstack(L, 1);
        mp_decode(&d);
        ++object_count;
        done = (*offset == len) || (object_count >= limit && limit > 0);
        break;
      case LMSGPACK_ERROR_INCOMPLETE:
        err_msg = "msgpack format data is incomplete";
        break;
      case LMSGPACK_ERROR_INVALID:
        err_msg = "msgpack format data is invalid";
        break;
      case LMSGPACK_ERROR_DEPTH:
        err_msg = "msgpack data nests too deeply (over " MP_STRINGIFY(LMSGPACK_MAX_DEPTH) " levels) or needs too much memory";
        break;
      default:
        err_msg = "msgpack unknown decoder result";
        break;
    }
  }
  *error = err_msg;
  *err_code = err_res;
  return (err_msg == mp_nullptr) ? object_count : 0;
}

static lua_msgpack *mp_packer_create (lua_State *L, lua_Integer flags) {
  lua_msgpack *ud = mp_pcast(lua_msgpack *, lua_newuserdatauv(L, sizeof(lua_msgpack), 0));
  ud->flags = 0;
  lmsgpack_writer_initbuffer(&ud->w, mp_nullptr, 0, mp_heapreserve, L);
  mp_heapreserve(&ud->w, MP_BUFFER_INITSIZE);
  ud->flags = MP_PACKING | (flags & ~MP_MASK_RUNTIME);
  luaL_setmetatable(L, LUA_MSGPACK_USERDATA);
  return ud;
}

static int mp_packer_destroy (lua_State *L, int idx) {
  lua_msgpack *ud = mp_pcast(lua_msgpack *, luaL_checkudata(L, idx, LUA_MSGPACK_USERDATA));
  if (ud->flags & MP_MODE) {
    if (ud->w.buf != mp_nullptr) {
      void *aud;
      lua_Alloc f = lua_getallocf(L, &aud);
      f(aud, ud->w.buf, ud->w.cap, 0);
    }
    ud->w.buf = mp_nullptr;
    ud->w.len = ud->w.cap = 0;
    ud->flags = 0;
    if (lua_checkstack(L, 2)) {
      lua_pushnil(L);
      lua_setmetatable(L, idx);
    }
    return 1;
  }
  return 0;
}

static void mp_packer_begin (lua_State *L, mp_encoder *e, int input, int *nargs) {
  lua_msgpack *ud = mp_pcast(lua_msgpack *, luaL_checkudata(L, 1, LUA_MSGPACK_USERDATA));
  if ((*nargs = lua_gettop(L)) < input)
    luaL_argerror(L, 0, "MessagePack pack needs input");
  else if (!mp_is_packing(ud))
    luaL_error(L, "invalid packer UD");
  ud->w.ud = L;
  e->L = L;
  e->w = &ud->w;
  e->flags = ud->flags;
  e->box = 0;
  e->fetched = 0;
  lua_pushnil(L);
  e->ext = lua_gettop(L);
}

#define mp_int_packer(NAME, TYPE) static void NAME (mp_encoder *e, int idx) { lmsgpack_write_int(e->w, mp_cast(int64_t, mp_cast(TYPE, luaL_checkinteger(e->L, idx)))); }
#define mp_uint_packer(NAME, TYPE) static void NAME (mp_encoder *e, int idx) { lmsgpack_write_uint(e->w, mp_cast(uint64_t, mp_cast(TYPE, luaL_checkinteger(e->L, idx)))); }
#define mp_sint_packer(NAME, TYPE) static void NAME (mp_encoder *e, int idx) { lmsgpack_write_sint(e->w, mp_cast(int64_t, mp_cast(TYPE, luaL_checkinteger(e->L, idx)))); }
#define mp_fix_packer(NAME, TYPE, SIZE, SIGNED) static void NAME (mp_encoder *e, int idx) { lmsgpack_write_fixint(e->w, (SIZE), (SIGNED), mp_cast(uint64_t, mp_cast(TYPE, luaL_checkinteger(e->L, idx)))); }

mp_int_packer(mp_pack_char, char)
mp_int_packer(mp_pack_signed_char, signed char)
mp_int_packer(mp_pack_short, short)
mp_int_packer(mp_pack_int, int)
mp_int_packer(mp_pack_long, long)
mp_int_packer(mp_pack_long_long, long long)
mp_uint_packer(mp_pack_unsigned_char, unsigned char)
mp_uint_packer(mp_pack_unsigned_short, unsigned short)
mp_uint_packer(mp_pack_unsigned_int, unsigned int)
mp_uint_packer(mp_pack_unsigned_long, unsigned long)
mp_uint_packer(mp_pack_unsigned_long_long, unsigned long long)

mp_uint_packer(mp_pack_uint8, uint8_t)
mp_uint_packer(mp_pack_uint16, uint16_t)
mp_uint_packer(mp_pack_uint32, uint32_t)
mp_uint_packer(mp_pack_uint64, uint64_t)
mp_int_packer(mp_pack_int8, int8_t)
mp_int_packer(mp_pack_int16, int16_t)
mp_int_packer(mp_pack_int32, int32_t)
mp_int_packer(mp_pack_int64, int64_t)

mp_fix_packer(mp_pack_fix_uint8, uint8_t, 1, 0)
mp_fix_packer(mp_pack_fix_uint16, uint16_t, 2, 0)
mp_fix_packer(mp_pack_fix_uint32, uint32_t, 4, 0)
mp_fix_packer(mp_pack_fix_uint64, uint64_t, 8, 0)
mp_fix_packer(mp_pack_fix_int8, int8_t, 1, 1)
mp_fix_packer(mp_pack_fix_int16, int16_t, 2, 1)
mp_fix_packer(mp_pack_fix_int32, int32_t, 4, 1)
mp_fix_packer(mp_pack_fix_int64, int64_t, 8, 1)

mp_sint_packer(mp_pack_signed_int16, int16_t)
mp_sint_packer(mp_pack_signed_int32, int32_t)
mp_sint_packer(mp_pack_signed_int64, int64_t)

static void mp_pack_float (mp_encoder *e, int idx) {
  lmsgpack_write_float(e->w, mp_cast(float, luaL_checknumber(e->L, idx)));
}

static void mp_pack_double (mp_encoder *e, int idx) {
  lmsgpack_write_double(e->w, mp_cast(double, luaL_checknumber(e->L, idx)));
}

static void mp_pack_boolean (mp_encoder *e, int idx) {
  lmsgpack_write_bool(e->w, lua_toboolean(e->L, idx));
}

static void mp_pack_bytes (mp_encoder *e, int idx, int (*header) (lmsgpack_Writer *w, size_t n)) {
  size_t len = 0;
  const char *s = luaL_checklstring(e->L, idx, &len);
  if (header(e->w, len) != 0)
    mp_toolarge(e);
  lmsgpack_write_bytes(e->w, s, len);
}

#define mp_function(NAME, CALL) static int NAME (lua_State *L) { mp_encoder e; int i, nargs; mp_packer_begin(L, &e, 2, &nargs); for (i = 2; i <= nargs; i++) { CALL; } lua_pushvalue(L, 1); return 1; }
#define mp_function_op(NAME, CALL) static int NAME (lua_State *L) { mp_encoder e; int nargs; mp_packer_begin(L, &e, 1, &nargs); CALL; lua_pushvalue(L, 1); return 1; }

mp_function(luaL_pack_char, mp_pack_char(&e, i))
mp_function(luaL_pack_signed_char, mp_pack_signed_char(&e, i))
mp_function(luaL_pack_short, mp_pack_short(&e, i))
mp_function(luaL_pack_int, mp_pack_int(&e, i))
mp_function(luaL_pack_long, mp_pack_long(&e, i))
mp_function(luaL_pack_long_long, mp_pack_long_long(&e, i))
mp_function(luaL_pack_unsigned_char, mp_pack_unsigned_char(&e, i))
mp_function(luaL_pack_unsigned_short, mp_pack_unsigned_short(&e, i))
mp_function(luaL_pack_unsigned_int, mp_pack_unsigned_int(&e, i))
mp_function(luaL_pack_unsigned_long, mp_pack_unsigned_long(&e, i))
mp_function(luaL_pack_unsigned_long_long, mp_pack_unsigned_long_long(&e, i))

mp_function(luaL_pack_uint8, mp_pack_uint8(&e, i))
mp_function(luaL_pack_uint16, mp_pack_uint16(&e, i))
mp_function(luaL_pack_uint32, mp_pack_uint32(&e, i))
mp_function(luaL_pack_uint64, mp_pack_uint64(&e, i))
mp_function(luaL_pack_int8, mp_pack_int8(&e, i))
mp_function(luaL_pack_int16, mp_pack_int16(&e, i))
mp_function(luaL_pack_int32, mp_pack_int32(&e, i))
mp_function(luaL_pack_int64, mp_pack_int64(&e, i))

mp_function(luaL_pack_fix_uint8, mp_pack_fix_uint8(&e, i))
mp_function(luaL_pack_fix_uint16, mp_pack_fix_uint16(&e, i))
mp_function(luaL_pack_fix_uint32, mp_pack_fix_uint32(&e, i))
mp_function(luaL_pack_fix_uint64, mp_pack_fix_uint64(&e, i))
mp_function(luaL_pack_fix_int8, mp_pack_fix_int8(&e, i))
mp_function(luaL_pack_fix_int16, mp_pack_fix_int16(&e, i))
mp_function(luaL_pack_fix_int32, mp_pack_fix_int32(&e, i))
mp_function(luaL_pack_fix_int64, mp_pack_fix_int64(&e, i))

mp_function(luaL_pack_signed_int16, mp_pack_signed_int16(&e, i))
mp_function(luaL_pack_signed_int32, mp_pack_signed_int32(&e, i))
mp_function(luaL_pack_signed_int64, mp_pack_signed_int64(&e, i))

mp_function(luaL_pack_float, mp_pack_float(&e, i))
mp_function(luaL_pack_double, mp_pack_double(&e, i))
mp_function(luaL_pack_integer, { luaL_checkinteger(L, i); mp_encode_integer(&e, i); })
mp_function(luaL_pack_number, { luaL_checknumber(L, i); mp_encode_number(&e, i); })
mp_function(luaL_pack_vector, mp_encode_vector(&e, i))

mp_function_op(luaL_pack_nil, lmsgpack_write_nil(e.w))
mp_function_op(luaL_pack_true, lmsgpack_write_true(e.w))
mp_function_op(luaL_pack_false, lmsgpack_write_false(e.w))
mp_function(luaL_pack_boolean, mp_pack_boolean(&e, i))

mp_function(luaL_pack_string, mp_pack_bytes(&e, i, lmsgpack_write_str))
mp_function(luaL_pack_v4, mp_pack_bytes(&e, i, lmsgpack_write_str_compat))
mp_function(luaL_pack_bin, mp_pack_bytes(&e, i, lmsgpack_write_bin))
mp_function(luaL_pack_parse_string, { luaL_checklstring(L, i, mp_nullptr); mp_encode_string(&e, i); })

mp_function(luaL_pack_array, mp_encode_array(&e, i, 1, mp_cast(size_t, lua_rawlen(L, i))))
mp_function(luaL_pack_map, { luaL_checktype(L, i, LUA_TTABLE); mp_encode_map(&e, i, 1, mp_count_pairs(L, i)); })
mp_function(luaL_pack_parsed_table, mp_encode_extended_table(&e, i, 1))
mp_function(luaL_pack_unparsed_table, mp_encode_table(&e, i, 1))
mp_function(luaL_pack_any, mp_encode_any(&e, i, 1))

static const luaL_Reg packers[] = {
  { "nil", luaL_pack_nil, }, { "null", luaL_pack_nil, },
  { "boolean", luaL_pack_boolean, },
  { "string_compat", luaL_pack_v4, },
  { "_string", luaL_pack_string, },
  { "string", luaL_pack_parse_string },
  { "binary", luaL_pack_bin, },
  { "map", luaL_pack_map, },
  { "array", luaL_pack_array, },
  { "table", luaL_pack_parsed_table },
  { "_table", luaL_pack_unparsed_table },
  { "unsigned", luaL_pack_unsigned_long_long },
  { "signed", luaL_pack_long_long },
  { "integer", luaL_pack_integer },
  { "float", luaL_pack_float },
  { "double", luaL_pack_double },
  { "number", luaL_pack_number },
  { "any", luaL_pack_any },
  { "true", luaL_pack_true, }, { "t", luaL_pack_true, },
  { "false", luaL_pack_false, }, { "f", luaL_pack_false, },
  { "fix_uint8", luaL_pack_fix_uint8, },
  { "fix_uint16", luaL_pack_fix_uint16, },
  { "fix_uint32", luaL_pack_fix_uint32, },
  { "fix_uint64", luaL_pack_fix_uint64, },
  { "fix_int8", luaL_pack_fix_int8, },
  { "fix_int16", luaL_pack_fix_int16, },
  { "fix_int32", luaL_pack_fix_int32, },
  { "fix_int64", luaL_pack_fix_int64, },
  { "uint8", luaL_pack_uint8, },
  { "uint16", luaL_pack_uint16, },
  { "uint32", luaL_pack_uint32, },
  { "uint64", luaL_pack_uint64, },
  { "int8", luaL_pack_int8, },
  { "int16", luaL_pack_int16, },
  { "int32", luaL_pack_int32, },
  { "int64", luaL_pack_int64, },
  { "char", luaL_pack_char, },
  { "signed_char", luaL_pack_signed_char, },
  { "unsigned_char", luaL_pack_unsigned_char, },
  { "short", luaL_pack_short, },
  { "int", luaL_pack_int, },
  { "long", luaL_pack_long, },
  { "long_long", luaL_pack_long_long, },
  { "unsigned_short", luaL_pack_unsigned_short, },
  { "unsigned_int", luaL_pack_unsigned_int, },
  { "unsigned_long", luaL_pack_unsigned_long, },
  { "unsigned_long_long", luaL_pack_unsigned_long_long, },
  { "signed_int16", luaL_pack_signed_int16, },
  { "signed_int32", luaL_pack_signed_int32, },
  { "signed_int64", luaL_pack_signed_int64, },
  { "vector", luaL_pack_vector, },
  { mp_nullptr, mp_nullptr },
};

static int packed_len (lua_State *L) {
  lua_msgpack *ud = mp_pcast(lua_msgpack *, luaL_checkudata(L, 1, LUA_MSGPACK_USERDATA));
  lua_pushinteger(L, mp_is_packing(ud) ? mp_cast(lua_Integer, ud->w.len) : 0);
  return 1;
}

static int packed_buffer_append (lua_State *L) {
  lua_msgpack *ud = mp_pcast(lua_msgpack *, luaL_checkudata(L, 1, LUA_MSGPACK_USERDATA));
  if (mp_is_packing(ud)) {
    int i;
    ud->w.ud = L;
    for (i = 2; i <= lua_gettop(L); ++i) {
      size_t len = 0;
      const char *s = luaL_checklstring(L, i, &len);
      lmsgpack_write_bytes(&ud->w, s, len);
    }
  }
  return 0;
}

static int packed_encode (lua_State *L) {
  lua_msgpack *ud = mp_pcast(lua_msgpack *, luaL_checkudata(L, 1, LUA_MSGPACK_USERDATA));
  if (mp_is_packing(ud)) {
    mp_encoder e;
    int i, nargs;
    mp_packer_begin(L, &e, 1, &nargs);
    for (i = 2; i <= nargs; ++i)
      mp_encode_any(&e, i, 0);
  }
  return 0;
}

static int packed_tostring (lua_State *L) {
  lua_msgpack *ud = mp_pcast(lua_msgpack *, luaL_checkudata(L, 1, LUA_MSGPACK_USERDATA));
  if (mp_is_packing(ud)) {
    lua_pushlstring(L, ud->w.buf, ud->w.len);
    return 1;
  }
  return 0;
}

static int packud_gc (lua_State *L) {
  mp_packer_destroy(L, 1);
  return 0;
}

static const luaL_Reg msgpack_metafuncs[] = {
  { "__gc", packud_gc },
  { "__len", packed_len },
  { "__concat", packed_buffer_append },
  { "__call", packed_encode },
  { "__add", packed_encode },
  { "__shl", packed_encode },
  { "__tostring", packed_tostring },
  { "__close", packud_gc },
  { mp_nullptr, mp_nullptr }
};

static const char *const opts[] = {
  "unsigned", "integer", "float", "double",
  "string_compat", "string_binary",
  "empty_table_as_array", "without_hole", "with_hole", "always_as_map",
  "small_lua", "full64bits", "long_double", "sentinel", "ignore_invalid",
  mp_nullptr
};

static const lua_Integer optsnum[] = {
  MP_UNSIGNED_INTEGERS, MP_NUMBER_AS_INTEGER, MP_NUMBER_AS_FLOAT, MP_NUMBER_AS_DOUBLE,
  MP_STRING_COMPAT, MP_STRING_BINARY,
  MP_EMPTY_AS_ARRAY, MP_ARRAY_WITHOUT_HOLES, MP_ARRAY_WITH_HOLES, MP_ARRAY_AS_MAP,
  MP_SMALL_LUA, MP_FULL_64_BITS, MP_LONG_DOUBLE, MP_USE_SENTINEL, MP_IGNORE_INVALID,
};

static int mp_unpacker (lua_State *L, int compat_api, int include_offset) {
  int top = 0, count = 0, limit = 0;
  size_t len = 0, position = 0, offset = 0, end_position = 0;
  int err_code = LMSGPACK_OK;
  const char *err_msg = mp_nullptr;
  const char *s = luaL_checklstring(L, 1, &len);
  if (compat_api) {
    position = 1;
    limit = include_offset ? 1 : 0;
    end_position = 0;
  }
  else {
    lua_Integer n;
    position = mp_optsizet(L, 2, 1);
    n = luaL_optinteger(L, 3, include_offset ? 1 : 0);
    luaL_argcheck(L, n >= 0, 3, "invalid limit");
    limit = (n > INT_MAX) ? INT_MAX : mp_cast(int, n);
    end_position = mp_optsizet(L, 4, 0);
  }
  offset = position - 1;

  if (lua_isinteger(L, 2) && lua_tointeger(L, 2) <= 0) {
    lua_pushvalue(L, 2);
    lua_pushnil(L);
    return 2;
  }

  if (len == 0)
    return 0;
  else if (position == 0)
    return luaL_error(L, "invalid string position: <0>");
  else if (offset > len)
    return luaL_error(L, "start offset greater than input length");
  else if (end_position > 0 && end_position < offset)
    return luaL_error(L, "end position less than offset");
  else if (end_position > 0 && end_position > len)
    return luaL_error(L, "ending offset greater than input ending position");

  top = lua_gettop(L);
  len = (end_position == 0) ? len : end_position;
  if ((count = mp_decode_objects(L, mp_getregi(L, LUA_MSGPACK_REG_OPTIONS, MP_DEFAULT), s, len, &offset, limit, &err_msg, &err_code)) == 0) {
    lua_settop(L, top);
    if (include_offset && err_code == LMSGPACK_ERROR_INCOMPLETE) {
      lua_pushinteger(L, -(mp_cast(lua_Integer, offset) + 1));
      lua_pushnil(L);
      return 2;
    }
    return luaL_error(L, "%s", err_msg);
  }

  if (include_offset) {
    mp_checkstack(L, 2);
    lua_pushinteger(L, (offset < len) ? (mp_cast(lua_Integer, offset) + 1) : 0);
    lua_insert(L, top + 1);
    count++;
  }
  return count;
}

static lua_Integer mp_checktype (lua_State *L, lua_Integer type, int arg) {
  if (!LUA_MSGPACK_EXT_VALID(type))
    return luaL_argerror(L, arg, "Invalid extension-type identifier");
  return type;
}

static lua_Integer mp_set_extension_helper (lua_State *L, int idx) {
  lua_Integer type = 0;
  luaL_checktype(L, idx, LUA_TTABLE);
  lua_getfield(L, idx, LUA_MSGPACK_META_MTYPE);
  lua_getfield(L, idx, LUA_MSGPACK_META_ENCODE);
  lua_getfield(L, idx, LUA_MSGPACK_META_DECODE);
  if (lua_type(L, -3) != LUA_TNUMBER)
    return luaL_argerror(L, idx, "invalid extension-type identifier");
  type = mp_checktype(L, lua_tointeger(L, -3), idx);
  if (LUA_MSGPACK_EXT_RESERVED(type))
    return luaL_argerror(L, idx, "Reserved extension-type identifier");
  else if (!lua_isfunction(L, -1) || !lua_isfunction(L, -2))
    return luaL_argerror(L, idx, "missing pack/unpack metamethods.");
  lua_pop(L, 3);
  mp_getregt(L, LUA_MSGPACK_REG_EXT);
  lua_pushvalue(L, idx);
  lua_rawseti(L, -2, type);
  lua_pop(L, 1);
  return type;
}

static int mp_packer_new (lua_State *L) {
  mp_packer_create(L, mp_getregi(L, LUA_MSGPACK_REG_OPTIONS, MP_DEFAULT));
  return 1;
}

static int mp_pack (lua_State *L) {
  char init[MP_STACK_BUFFERSIZE];
  lmsgpack_Writer w;
  mp_encoder e;
  int i, nargs;
  if ((nargs = lua_gettop(L)) == 0)
    return luaL_argerror(L, 0, "MessagePack pack needs input");
  else if (!lua_checkstack(L, nargs))
    return luaL_argerror(L, 0, "too many arguments for MessagePack pack");
  mp_encoder_init(L, &e, &w, init, sizeof(init));
  for (i = 1; i <= nargs; i++)
    mp_encode_any(&e, i, 0);
  lua_pushlstring(L, w.buf, w.len);
  return 1;
}

static int mp_pack_args (lua_State *L) {
  char init[MP_STACK_BUFFERSIZE];
  lmsgpack_Writer w;
  mp_encoder e;
  int i, top = lua_gettop(L);
  mp_encoder_init(L, &e, &w, init, sizeof(init));
  lmsgpack_write_array(&w, mp_cast(size_t, top));
  for (i = 1; i <= top; ++i)
    mp_encode_any(&e, i, 1);
  lua_pushlstring(L, w.buf, w.len);
  return 1;
}

static int mp_unpack (lua_State *L) { return mp_unpacker(L, 0, 0); }

#if defined(LUA_MSGPACK_COMPAT)
static int mp_unpack_compat (lua_State *L) { return mp_unpacker(L, 1, 0); }
#endif

static int mp_unpack_next (lua_State *L) { return mp_unpacker(L, 0, 1); }

static int mp_get_extension (lua_State *L) {
  mp_checktype(L, luaL_checkinteger(L, 1), 1);
  mp_getregt(L, LUA_MSGPACK_REG_EXT);
  lua_pushvalue(L, 1);
  lua_rawget(L, -2);
  return 1;
}

static int mp_set_extension (lua_State *L) {
  mp_set_extension_helper(L, 1);
  lua_pushvalue(L, 1);
  return 1;
}

static int mp_clear_extension (lua_State *L) {
  int i, nargs = lua_gettop(L);
  mp_getregt(L, LUA_MSGPACK_REG_EXT);
  for (i = 1; i <= nargs; i++) {
    lua_Integer type = mp_checktype(L, luaL_checkinteger(L, i), i);
    if (LUA_MSGPACK_EXT_RESERVED(type))
      return luaL_argerror(L, i, "Reserved extension-type identifier");
    lua_pushvalue(L, i);
    lua_pushnil(L);
    lua_rawset(L, -3);
  }
  lua_pop(L, 1);
  return 0;
}

static int mp_get_type_extension (lua_State *L) {
  int top = lua_gettop(L);
  lua_Integer ltype = typetoindex(L, lua_tostring(L, 1));
  luaL_argcheck(L, ltype != -1, 1, "Lua type");
  mp_getregt(L, LUA_MSGPACK_REG_EXT);
  if (lua_rawgeti(L, -1, LUA_MSGPACK_LUA_TYPE(ltype)) == LUA_TNUMBER) {
    lua_rawget(L, -2);
    lua_insert(L, top + 1);
    lua_pop(L, 1);
  }
  else {
    lua_pop(L, 2);
    lua_pushnil(L);
  }
  return 1;
}

static int mp_set_type_extension (lua_State *L) {
  lua_Integer ext = 0;
  lua_Integer ltype = typetoindex(L, lua_tostring(L, 1));
  int t = lua_type(L, 2);
  luaL_argcheck(L, ltype != -1, 1, "Lua type");
  if (t == LUA_TTABLE)
    ext = mp_set_extension_helper(L, 2);
  else if (t == LUA_TNUMBER) {
    ext = mp_checktype(L, luaL_checkinteger(L, 2), 2);
    mp_getregt(L, LUA_MSGPACK_REG_EXT);
    if (lua_rawgeti(L, -1, ext) == LUA_TNIL)
      return luaL_error(L, "attempting to associate to nil msgpack extension");
    lua_pop(L, 2);
  }
  else if (t == LUA_TNIL) {
    mp_getregt(L, LUA_MSGPACK_REG_EXT);
    lua_pushinteger(L, LUA_MSGPACK_LUA_TYPE(ltype));
    lua_pushnil(L);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    return 0;
  }
  else
    return luaL_argerror(L, 2, "extension identifier or extension table");
  if (LUA_MSGPACK_EXT_VALID(ext)) {
    mp_getregt(L, LUA_MSGPACK_REG_EXT);
    lua_pushinteger(L, LUA_MSGPACK_LUA_TYPE(ltype));
    lua_pushinteger(L, ext);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    lua_pushvalue(L, 2);
    return 1;
  }
  return luaL_error(L, "msgpack extension type: invalid encoder!");
}

static int mp_setoption (lua_State *L) {
  const lua_Integer opt = optsnum[luaL_checkoption(L, 1, mp_nullptr, opts)];
  lua_Integer flags = mp_getregi(L, LUA_MSGPACK_REG_OPTIONS, MP_DEFAULT);
  switch (opt) {
    case MP_IGNORE_INVALID:
    case MP_USE_SENTINEL:
    case MP_EMPTY_AS_ARRAY:
    case MP_UNSIGNED_INTEGERS: {
      luaL_checktype(L, 2, LUA_TBOOLEAN);
      flags = lua_toboolean(L, 2) ? (flags | opt) : (flags & ~opt);
      break;
    }
    case MP_NUMBER_AS_INTEGER:
    case MP_NUMBER_AS_FLOAT:
    case MP_NUMBER_AS_DOUBLE: {
      luaL_checktype(L, 2, LUA_TBOOLEAN);
      flags &= lua_toboolean(L, 2) ? ~MP_MASK_NUMBER : ~opt;
      flags |= lua_toboolean(L, 2) ? opt : 0;
      if ((flags & MP_MASK_NUMBER) == 0)
        flags |= (MP_DEFAULT & MP_MASK_NUMBER);
      break;
    }
    case MP_ARRAY_AS_MAP:
    case MP_ARRAY_WITH_HOLES:
    case MP_ARRAY_WITHOUT_HOLES: {
      luaL_checktype(L, 2, LUA_TBOOLEAN);
      flags &= lua_toboolean(L, 2) ? ~MP_MASK_ARRAY : ~opt;
      flags |= lua_toboolean(L, 2) ? opt : 0;
      if ((flags & MP_MASK_ARRAY) == 0)
        flags |= (MP_DEFAULT & MP_MASK_ARRAY);
      break;
    }
    case MP_STRING_BINARY:
    case MP_STRING_COMPAT: {
      luaL_checktype(L, 2, LUA_TBOOLEAN);
      flags &= lua_toboolean(L, 2) ? ~MP_MASK_STRING : ~opt;
      flags |= lua_toboolean(L, 2) ? opt : 0;
      if ((flags & MP_MASK_STRING) == 0)
        flags |= (MP_DEFAULT & MP_MASK_STRING);
      break;
    }
    default:
      break;
  }
  mp_setregi(L, LUA_MSGPACK_REG_OPTIONS, flags);
  return 0;
}

static int mp_getoption (lua_State *L) {
  const lua_Integer opt = optsnum[luaL_checkoption(L, 1, mp_nullptr, opts)];
  const lua_Integer flags = mp_getregi(L, LUA_MSGPACK_REG_OPTIONS, MP_DEFAULT);
  switch (opt) {
    case MP_IGNORE_INVALID:
    case MP_USE_SENTINEL:
    case MP_EMPTY_AS_ARRAY:
    case MP_UNSIGNED_INTEGERS:
      lua_pushboolean(L, (flags & opt) != 0);
      break;
    case MP_STRING_BINARY:
    case MP_STRING_COMPAT: {
      const lua_Integer _sflags = ((flags & MP_MASK_STRING) == 0) ? MP_DEFAULT : flags;
      lua_pushboolean(L, ((_sflags & MP_MASK_STRING) & opt) != 0);
      break;
    }
    case MP_ARRAY_AS_MAP:
    case MP_ARRAY_WITH_HOLES:
    case MP_ARRAY_WITHOUT_HOLES: {
      const lua_Integer _aflags = ((flags & MP_MASK_ARRAY) == 0) ? MP_DEFAULT : flags;
      lua_pushboolean(L, ((_aflags & MP_MASK_ARRAY) & opt) != 0);
      break;
    }
    case MP_NUMBER_AS_INTEGER:
    case MP_NUMBER_AS_FLOAT:
    case MP_NUMBER_AS_DOUBLE: {
      const lua_Integer _nflags = ((flags & MP_MASK_NUMBER) == 0) ? MP_DEFAULT : flags;
      lua_pushboolean(L, ((_nflags & MP_MASK_NUMBER) & opt) != 0);
      break;
    }
    case MP_SMALL_LUA:
      lua_pushboolean(L, LUA_MAXINTEGER < INT64_MAX || LUA_FLOAT_TYPE == LUA_FLOAT_FLOAT);
      break;
    case MP_FULL_64_BITS:
      lua_pushboolean(L, LUA_MAXINTEGER >= INT64_MAX);
      break;
    case MP_LONG_DOUBLE: {
      lua_pushboolean(L, LUA_FLOAT_TYPE == LUA_FLOAT_LONGDOUBLE);
      break;
    }
    default:
      return 0;
  }
  return 1;
}

static int mp_set_string (lua_State *L) {
  static const char *const s_opts[] = { "string", "string_compat", "string_binary", mp_nullptr };
  static const lua_Integer s_optsnum[] = { 0x0, MP_STRING_COMPAT, MP_STRING_BINARY };
  const lua_Integer opt = s_optsnum[luaL_checkoption(L, 1, mp_nullptr, s_opts)];
  lua_Integer flags = mp_getregi(L, LUA_MSGPACK_REG_OPTIONS, MP_DEFAULT) & ~MP_MASK_STRING;
  mp_setregi(L, LUA_MSGPACK_REG_OPTIONS, flags | opt);
  return 0;
}

static int mp_set_array (lua_State *L) {
  static const char *const s_opts[] = { "without_hole", "with_hole", "always_as_map", mp_nullptr };
  static const lua_Integer s_optsnum[] = { MP_ARRAY_WITHOUT_HOLES, MP_ARRAY_WITH_HOLES, MP_ARRAY_AS_MAP };
  const lua_Integer opt = s_optsnum[luaL_checkoption(L, 1, mp_nullptr, s_opts)];
  lua_Integer flags = mp_getregi(L, LUA_MSGPACK_REG_OPTIONS, MP_DEFAULT) & ~MP_MASK_ARRAY;
  flags |= opt;
  if ((flags & MP_ARRAY_AS_MAP) != 0)
    flags &= ~MP_EMPTY_AS_ARRAY;
  mp_setregi(L, LUA_MSGPACK_REG_OPTIONS, flags);
  return 0;
}

static int mp_set_integer (lua_State *L) {
  static const char *const s_opts[] = { "signed", "unsigned", mp_nullptr };
  static const lua_Integer s_optsnum[] = { 0x0, MP_UNSIGNED_INTEGERS };
  const lua_Integer opt = s_optsnum[luaL_checkoption(L, 1, mp_nullptr, s_opts)];
  lua_Integer flags = mp_getregi(L, LUA_MSGPACK_REG_OPTIONS, MP_DEFAULT) & ~MP_UNSIGNED_INTEGERS;
  mp_setregi(L, LUA_MSGPACK_REG_OPTIONS, flags | opt);
  return 0;
}

static int mp_set_number (lua_State *L) {
  static const char *const s_opts[] = { "float", "double", mp_nullptr };
  static const lua_Integer s_optsnum[] = { MP_NUMBER_AS_FLOAT, MP_NUMBER_AS_DOUBLE };
  const lua_Integer opt = s_optsnum[luaL_checkoption(L, 1, mp_nullptr, s_opts)];
  lua_Integer flags = mp_getregi(L, LUA_MSGPACK_REG_OPTIONS, MP_DEFAULT) & ~MP_MASK_NUMBER;
  mp_setregi(L, LUA_MSGPACK_REG_OPTIONS, flags | opt);
  return 0;
}

#if defined(LUA_MSGPACK_SAFE)
static int mp_safe (lua_State *L) {
  int argc = lua_gettop(L), err, total_results;
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_insert(L, 1);
  err = lua_pcall(L, argc, LUA_MULTRET, 0);
  total_results = lua_gettop(L);
  if (err) {
    lua_pushnil(L);
    lua_insert(L, -2);
    return 2;
  }
  return total_results;
}

static int mp_issafe (lua_State *L) {
  lua_pushboolean(L, 1);
  return 1;
}
#else
static int mp_issafe (lua_State *L) {
  lua_pushboolean(L, 0);
  return 1;
}
#endif

static const luaL_Reg msgpack_lib[] = {
  { "pack", mp_pack },
  { "pack_args", mp_pack_args },
#if defined(LUA_MSGPACK_COMPAT)
  { "unpack", mp_unpack_compat },
  { "unpack2", mp_unpack },
#else
  { "unpack", mp_unpack },
#endif
  { "next", mp_unpack_next },
  { "new", mp_packer_new },
  { "setoption", mp_setoption },
  { "getoption", mp_getoption },
  { "safe", mp_issafe },
  { "set_string", mp_set_string },
  { "set_array", mp_set_array },
  { "set_integer", mp_set_integer },
  { "set_number", mp_set_number },
  { "extend", mp_set_extension },
  { "extend_get", mp_get_extension },
  { "extend_clear", mp_clear_extension },
  { "gettype", mp_get_type_extension },
  { "settype", mp_set_type_extension },
  { "sentinel", mp_nullptr }, { "null", mp_nullptr },
  { mp_nullptr, mp_nullptr }
};

#if defined(__cplusplus)
extern "C" {
#endif

LUAMOD_API int luaopen_msgpack (lua_State *L) {
  luaL_newlib(L, msgpack_lib);
#if defined(LUA_MSGPACK_SAFE)
  {
    size_t i;
    for (i = 0; i < (sizeof(msgpack_lib) / sizeof(*msgpack_lib) - 1); i++) {
      lua_getfield(L, -1, msgpack_lib[i].name);
      lua_pushcclosure(L, mp_safe, 1);
      lua_setfield(L, -2, msgpack_lib[i].name);
    }
  }
#endif
  if (luaL_newmetatable(L, LUA_MSGPACK_USERDATA)) {
    luaL_setfuncs(L, msgpack_metafuncs, 0);
    luaL_newlib(L, packers);
    lua_setfield(L, -2, "__index");
  }
  lua_pop(L, 1);
  mp_setregi(L, LUA_MSGPACK_REG_OPTIONS, MP_DEFAULT);
  mp_pushnull(L);
  lua_setfield(L, -2, "null");
  mp_pushnull(L);
  lua_setfield(L, -2, "sentinel");
  lua_newtable(L);
  luaL_setfuncs(L, packers, 0);
  lua_setfield(L, -2, "packers");
  return 1;
}

#if defined(__cplusplus)
}
#endif
