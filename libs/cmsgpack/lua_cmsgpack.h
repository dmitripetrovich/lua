#ifndef lua_cmsgpack_h
#define lua_cmsgpack_h

#include <string.h>
#include <stdint.h>

#if defined(LUA_COMPILED_AS_HPP)
extern "C++" {
#endif

#include <lua.h>
#include <luaconf.h>
#include <lauxlib.h>
#if LUA_VERSION_NUM >= 504
  #include <lgrit_lib.h>
#elif LUA_VERSION_NUM == 503
  #include <lobject.h>
  #define LUA_VEC_TYPE LUA_FLOAT_FLOAT
  typedef float lua_VecF;
#else
   #error unsupported Lua version
#endif

#if defined(LUA_COMPILED_AS_HPP)
}
#endif

#include "msgpack/pack.h"
#include "msgpack/unpack.h"
#include "msgpack/zone.h"

#include "pack_template_ext.h"

#if !defined(LUA_MSGPACK_BIT32) && UINTPTR_MAX == UINT_MAX
  #define LUA_MSGPACK_BIT32
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

#if !defined(MP_ZONE_CHUNK_SIZE)
  #define MP_ZONE_CHUNK_SIZE 256
#endif

#ifdef _MSC_VER
  #define LUA_MSGPACK_INLINE __forceinline
#elif defined(__GNUC__)
  #define LUA_MSGPACK_INLINE inline __attribute__((__always_inline__))
#else
  #define LUA_MSGPACK_INLINE inline
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

#if LUA_VERSION_NUM >= 503
  #define mp_ti(v) v
  #define mp_rawgeti(L, I, K) lua_rawgeti((L), (I), (K))
  #define mp_getfield(L, I, K) lua_getfield((L), (I), (K))
#else
  #define mp_ti(v) mp_cast(int, v)
  #define mp_rawgeti(L, I, K) (lua_rawgeti((L), (I), (K)), lua_type((L), -1))
  #define mp_getfield(L, I, K) (lua_getfield((L), (I), (K)), lua_type((L), -1))
#endif

#if LUA_VERSION_NUM < 502
  #define lua_absindex(L, i) ((i) > 0 || (i) <= LUA_REGISTRYINDEX ? (i) : lua_gettop(L) + (i) + 1)
#endif

#if LUA_VERSION_NUM >= 504
  #define mp_newuserdata(L, s) lua_newuserdatauv((L), (s), 0)
#elif LUA_VERSION_NUM >= 501 && LUA_VERSION_NUM <= 503
  #define mp_newuserdata(L, s) lua_newuserdata((L), (s))
#else
  #error unsupported Lua version
#endif

#if LUA_VERSION_NUM >= 503
  #define mp_nil_metafield LUA_TNIL
#else
  #define mp_nil_metafield 0
#endif

#if defined(LUA_MSGPACK_UNSAFE)
  #define mp_checkstack(L, sz) ((void)0)
#else
  #define mp_checkstack(L, sz) luaL_checkstack((L), (sz), "too many (nested) values in encoded msgpack")
#endif

#define mp_rel_index(idx, n) (((idx) < 0) ? ((idx) - (n)) : (idx))

#define IS_INT_TYPE_EQUIVALENT(x, t) (!isinf(x) && mp_cast(lua_Number, mp_cast(t, x)) == (x))
#define IS_INT64_EQUIVALENT(x) IS_INT_TYPE_EQUIVALENT(x, int64_t)
#define IS_INT_EQUIVALENT(x) IS_INT_TYPE_EQUIVALENT(x, int)

#if defined(__cplusplus)
  #include <limits>
  #define MP_MAX_SIZET std::numeric_limits<size_t>::max()
#else
  #define MP_MAX_SIZET ((size_t)(~(size_t)0))
#endif

#if LUA_VERSION_NUM >= 503
  #define MP_MAX_LUAINDEX (sizeof(size_t) < sizeof(lua_Integer) ? MP_MAX_SIZET : mp_cast(size_t, LUA_MAXINTEGER))
#elif LUA_VERSION_NUM == 501 || LUA_VERSION_NUM == 502
  #define MP_MAX_LUAINDEX MP_MAX_SIZET
#else
  #error unsupported Lua version
#endif

#if LUA_VERSION_NUM >= 503
  #define mp_isinteger(L, idx) lua_isinteger((L), (idx))
#else
static LUA_MSGPACK_INLINE int mp_isinteger(lua_State *L, int idx) {
  if (LUA_TNUMBER == lua_type(L, idx)) {
    lua_Number n = lua_tonumber(L, idx);
    return IS_INT_TYPE_EQUIVALENT(n, lua_Integer);
  }
  return 0;
}
#endif

typedef struct lua_mpbuffer {
  char *b;
  size_t size;
  size_t n;
  lua_State *L;
} lua_mpbuffer;

static LUA_MSGPACK_INLINE void *lua_mpbuffer_realloc (lua_State *L, void *target, size_t osize, size_t nsize) {
  void *ud;
  lua_Alloc local_realloc = lua_getallocf(L, &ud);
  return local_realloc(ud, target, osize, nsize);
}

static LUA_MSGPACK_INLINE char *lua_mpbuffer_prepare (lua_State *L, lua_mpbuffer *B, size_t sz) {
  if ((B->size - B->n) < sz) {
    size_t newsize = B->size * 2;
    if (MP_MAX_SIZET - sz < B->n) {
      luaL_error(L, "buffer too large");
      return mp_nullptr;
    }
    if (newsize < B->n + sz)
      newsize = B->n + sz;

    B->b = mp_pcast(char *, lua_mpbuffer_realloc(L, B->b, B->size, newsize));
    B->size = newsize;
    if (B->b == mp_nullptr) {
      luaL_error(L, "unable to allocate buffer");
      return mp_nullptr;
    }
  }
  return B->b + B->n;
}

static LUA_MSGPACK_INLINE char *lua_mpbuffer_initsize (lua_State *L, lua_mpbuffer *B, size_t sz) {
  B->L = L;
  B->b = mp_nullptr;
  B->n = B->size = 0;
  return lua_mpbuffer_prepare(L, B, sz);
}

static LUA_MSGPACK_INLINE void lua_mpbuffer_init (lua_State *L, lua_mpbuffer *B) {
  lua_mpbuffer_initsize(L, B, MP_BUFFER_INITSIZE);
}

static LUA_MSGPACK_INLINE void lua_mpbuffer_free (lua_State *L, lua_mpbuffer *B) {
  if (B->b != mp_nullptr) {
    lua_mpbuffer_realloc(L, B->b, B->size, 0);
    B->b = mp_nullptr;
    B->n = B->size = 0;
  }
  B->L = mp_nullptr;
}

#define LUA_MSGPACK_META_MTYPE "__ext"
#define LUA_MSGPACK_META_ENCODE "__pack"
#define LUA_MSGPACK_META_DECODE "__unpack"
#define GAMELUA_META_FUNCREF "__game_functionReference"

#define LUA_MSGPACK_USERDATA "LUA_MSGPACK"

#if defined(LUAI_FUNC)
  #define LUA_MP_API LUAI_FUNC
#else
	#define LUA_MP_API extern
#endif

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

#if defined(LUA_MSGPACK_BIT32)
  #define MP_DEFAULT (MP_EMPTY_AS_ARRAY | MP_UNSIGNED_INTEGERS | MP_ARRAY_WITHOUT_HOLES | MP_NUMBER_AS_FLOAT | MP_STRING_COMPAT)
#else
  #define MP_DEFAULT (MP_EMPTY_AS_ARRAY | MP_UNSIGNED_INTEGERS | MP_ARRAY_WITHOUT_HOLES | MP_NUMBER_AS_DOUBLE)
#endif

#define MSGPACK_EXT_FUNCREF       10
#define MSGPACK_EXT_LOCALFUNCREF  11
#define MSGPACK_EXT_VECTOR2       20
#define MSGPACK_EXT_VECTOR3       21
#define MSGPACK_EXT_VECTOR4       22
#define MSGPACK_EXT_QUATERNION    23

#define LUA_MSGPACK_EXT_VALID(i) (INT8_MIN <= (i) && (i) <= INT8_MAX)

#define LUA_MSGPACK_EXT_USER_VALID(i) (0x0 <= (i) && (i) <= INT8_MAX)

#define LUA_MSGPACK_EXT_RESERVED(i) (                                          \
  (i) == MSGPACK_EXT_VECTOR2 || (i) == MSGPACK_EXT_VECTOR3 ||                  \
  (i) == MSGPACK_EXT_VECTOR4 || (i) == MSGPACK_EXT_QUATERNION                  \
)

#define LUA_MSGPACK_LUA_TYPE(x) (x + 1000)

#define EXT_INVALID -1024

typedef struct lua_msgpack {
  lua_Integer flags;
  union {
    struct {
      msgpack_packer packer;
      lua_mpbuffer buffer;
    } packed;
    struct {
      msgpack_zone zone;
    } unpacked;
  } u;
} lua_msgpack;

LUA_MP_API lua_msgpack *lua_msgpack_create (lua_State *L, lua_Integer flags);

LUA_MP_API int lua_msgpack_destroy (lua_State *L, int idx, lua_msgpack *ud);

LUA_MP_API void lua_msgpack_extension (lua_State *L, lua_Integer type, lua_CFunction encoder, lua_CFunction decoder);

LUA_MP_API void lua_msgpack_encode (lua_State *L, lua_msgpack *ud, int idx, int level);

LUA_MP_API int lua_msgpack_decode (lua_State *L, lua_msgpack *ud, const char *s,
  size_t len, size_t *offset, int limit, const char **error, msgpack_unpack_return *err_code);

LUA_MP_API void mp_replace_null (lua_State *L);

LUA_MP_API int mp_is_null (lua_State *L, int idx);

LUA_MP_API lua_Integer mp_ext_type (lua_State *L, int idx);

LUA_MP_API int mp_encode_ext_metatable (lua_State *L, lua_msgpack *ud, int idx, int8_t ext_id, int8_t has_ext);

LUA_MP_API int mp_encode_registered_ext_type (lua_State *L, lua_msgpack *ud, int idx, int8_t ext_id);

LUA_MP_API int mp_encode_registered_lua_type (lua_State *L, lua_msgpack *ud, int idx, int type);

LUA_MP_API int mp_parse_vector (lua_State *L, int idx, lua_Float4 *v);

LUA_MP_API int mp_table_is_an_array (lua_State *L, int idx, lua_Integer flags, size_t *array_length);

LUA_MP_API void mp_encode_lua_table_as_array (lua_State *L, lua_msgpack *ud, int idx, int level, size_t array_length);

LUA_MP_API void mp_encode_lua_table_as_map (lua_State *L, lua_msgpack *ud, int idx, int level);

static LUA_MSGPACK_INLINE msgpack_packer* mp_get_packer(lua_State *L, lua_msgpack *ud) {
  ud->u.packed.buffer.L = L;
  return &ud->u.packed.packer;
}

#define lua_msgpack_op(NAME, PACKER)                                    \
  static LUA_MSGPACK_INLINE void(NAME)(lua_State *L, lua_msgpack *ud) { \
    ((void)(L));                                                        \
    PACKER(mp_get_packer(L, ud));                                     \
  }

#define lua_msgpack_number_func(NAME, PACKER, TYPE)                              \
  static LUA_MSGPACK_INLINE void(NAME)(lua_State *L, lua_msgpack *ud, int idx) { \
    PACKER(mp_get_packer(L, ud), mp_cast(TYPE, lua_tonumber(L, idx)));         \
  }

#define lua_msgpack_int_func(NAME, PACKER, TYPE)                                 \
  static LUA_MSGPACK_INLINE void(NAME)(lua_State *L, lua_msgpack *ud, int idx) { \
    PACKER(mp_get_packer(L, ud), mp_cast(TYPE, lua_tointeger(L, idx)));        \
  }

#define lua_msgpack_str_func(NAME, LEN, BODY)                                    \
  static LUA_MSGPACK_INLINE void(NAME)(lua_State *L, lua_msgpack *ud, int idx) { \
    size_t len = 0;                                                              \
    const char *s = lua_tolstring(L, idx, &len);                                 \
    LEN(mp_get_packer(L, ud), len);                                            \
    BODY(mp_get_packer(L, ud), s, len);                                        \
  }

lua_msgpack_int_func(lua_pack_char, msgpack_pack_char, char)

lua_msgpack_int_func(lua_pack_signed_char, msgpack_pack_signed_char, signed char)
lua_msgpack_int_func(lua_pack_short, msgpack_pack_short, short)
lua_msgpack_int_func(lua_pack_int, msgpack_pack_int, int)
lua_msgpack_int_func(lua_pack_long, msgpack_pack_long, long)
lua_msgpack_int_func(lua_pack_long_long, msgpack_pack_long_long, long long)
lua_msgpack_int_func(lua_pack_unsigned_char, msgpack_pack_unsigned_char, unsigned char)
lua_msgpack_int_func(lua_pack_unsigned_short, msgpack_pack_unsigned_short, unsigned short)
lua_msgpack_int_func(lua_pack_unsigned_int, msgpack_pack_unsigned_int, unsigned int)
lua_msgpack_int_func(lua_pack_unsigned_long, msgpack_pack_unsigned_long, unsigned long)
lua_msgpack_int_func(lua_pack_unsigned_long_long, msgpack_pack_unsigned_long_long, unsigned long long)

lua_msgpack_int_func(lua_pack_uint8, msgpack_pack_uint8, uint8_t)
lua_msgpack_int_func(lua_pack_uint16, msgpack_pack_uint16, uint16_t)
lua_msgpack_int_func(lua_pack_uint32, msgpack_pack_uint32, uint32_t)
lua_msgpack_int_func(lua_pack_uint64, msgpack_pack_uint64, uint64_t)
lua_msgpack_int_func(lua_pack_int8, msgpack_pack_int8, int8_t)
lua_msgpack_int_func(lua_pack_int16, msgpack_pack_int16, int16_t)
lua_msgpack_int_func(lua_pack_int32, msgpack_pack_int32, int32_t)
lua_msgpack_int_func(lua_pack_int64, msgpack_pack_int64, int64_t)

lua_msgpack_int_func(lua_pack_fix_uint8, msgpack_pack_fix_uint8, uint8_t)
lua_msgpack_int_func(lua_pack_fix_uint16, msgpack_pack_fix_uint16, uint16_t)
lua_msgpack_int_func(lua_pack_fix_uint32, msgpack_pack_fix_uint32, uint32_t)
lua_msgpack_int_func(lua_pack_fix_uint64, msgpack_pack_fix_uint64, uint64_t)
lua_msgpack_int_func(lua_pack_fix_int8, msgpack_pack_fix_int8, int8_t)
lua_msgpack_int_func(lua_pack_fix_int16, msgpack_pack_fix_int16, int16_t)
lua_msgpack_int_func(lua_pack_fix_int32, msgpack_pack_fix_int32, int32_t)
lua_msgpack_int_func(lua_pack_fix_int64, msgpack_pack_fix_int64, int64_t)

lua_msgpack_int_func(lua_pack_signed_int16, msgpack_pack_signed_int16, int16_t)
lua_msgpack_int_func(lua_pack_signed_int32, msgpack_pack_signed_int32, int32_t)
lua_msgpack_int_func(lua_pack_signed_int64, msgpack_pack_signed_int64, int64_t)

lua_msgpack_number_func(lua_pack_float, msgpack_pack_float, float)
lua_msgpack_number_func(lua_pack_double, msgpack_pack_double, double)

lua_msgpack_op(lua_pack_nil, msgpack_pack_nil)
lua_msgpack_op(lua_pack_true, msgpack_pack_true)
lua_msgpack_op(lua_pack_false, msgpack_pack_false)

lua_msgpack_str_func(lua_pack_string, msgpack_pack_str, msgpack_pack_str_body)
lua_msgpack_str_func(lua_pack_v4, msgpack_pack_v4raw, msgpack_pack_v4raw_body)
lua_msgpack_str_func(lua_pack_bin, msgpack_pack_bin, msgpack_pack_bin_body)

static LUA_MSGPACK_INLINE void lua_pack_array (lua_State *L, lua_msgpack *ud, int idx, int level) {
  ((void)level);
  mp_encode_lua_table_as_array(L, ud, idx, level,
#if LUA_VERSION_NUM < 502
    mp_cast(size_t, lua_objlen(L, -1))
#else
    mp_cast(size_t, lua_rawlen(L, -1))
#endif
  );
}

static LUA_MSGPACK_INLINE void lua_pack_map (lua_State *L, lua_msgpack *ud, int idx, int level) {
  mp_encode_lua_table_as_map(L, ud, idx, level);
}

static LUA_MSGPACK_INLINE void lua_pack_boolean (lua_State *L, lua_msgpack *ud, int idx) {
  if (lua_toboolean(L, idx))
    msgpack_pack_true(mp_get_packer(L, ud));
  else
    msgpack_pack_false(mp_get_packer(L, ud));
}

static LUA_MSGPACK_INLINE void lua_pack_integer (lua_State *L, lua_msgpack *ud, int idx) {
  msgpack_packer *pk = mp_get_packer(L, ud);
#if LUA_VERSION_NUM >= 503
  #if defined(LUA_MSGPACK_BIT32)
  if (ud->flags & MP_UNSIGNED_INTEGERS)
    msgpack_pack_int32(pk, mp_cast(int32_t, lua_tointeger(L, idx)));
  else
    msgpack_pack_signed_int32(pk, mp_cast(int32_t, lua_tointeger(L, idx)));
  #else
  if (ud->flags & MP_UNSIGNED_INTEGERS)
    msgpack_pack_int64(pk, mp_cast(int64_t, lua_tointeger(L, idx)));
  else
    msgpack_pack_signed_int64(pk, mp_cast(int64_t, lua_tointeger(L, idx)));
  #endif
#else
  #if defined(LUA_MSGPACK_BIT32)
  if (ud->flags & MP_UNSIGNED_INTEGERS)
    msgpack_pack_int32(pk, mp_cast(int32_t, lua_tonumber(L, idx)));
  else
    msgpack_pack_signed_int32(pk, mp_cast(int32_t, lua_tonumber(L, idx)));
  #else
  if (ud->flags & MP_UNSIGNED_INTEGERS)
    msgpack_pack_int64(pk, mp_cast(int64_t, lua_tonumber(L, idx)));
  else
    msgpack_pack_signed_int64(pk, mp_cast(int64_t, lua_tonumber(L, idx)));
  #endif
#endif
}

static LUA_MSGPACK_INLINE void lua_pack_number (lua_State *L, lua_msgpack *ud, int idx) {
#if LUA_VERSION_NUM >= 503
  if (lua_isinteger(L, idx) || (ud->flags & MP_NUMBER_AS_INTEGER))
    lua_pack_integer(L, ud, idx);
  else if (ud->flags & MP_NUMBER_AS_FLOAT)
    msgpack_pack_float(mp_get_packer(L, ud), mp_cast(float, lua_tonumber(L, idx)));
  else
    msgpack_pack_double(mp_get_packer(L, ud), mp_cast(double, lua_tonumber(L, idx)));
#else
  lua_Number n = lua_tonumber(L, idx);
#if defined(LUA_MSGPACK_BIT32)
  if (IS_INT_EQUIVALENT(n) || (ud->flags & MP_NUMBER_AS_INTEGER)) {
#else
  if (IS_INT64_EQUIVALENT(n) || (ud->flags & MP_NUMBER_AS_INTEGER)) {
#endif
    lua_pack_integer(L, ud, idx);
  }
  else if (ud->flags & MP_NUMBER_AS_FLOAT)
    msgpack_pack_float(mp_get_packer(L, ud), mp_cast(float, n));
  else
    msgpack_pack_double(mp_get_packer(L, ud), mp_cast(double, n));
#endif
}

static LUA_MSGPACK_INLINE void lua_pack_table (lua_State *L, lua_msgpack *ud, int idx, int level) {
  size_t array_length = 0;
  if ((ud->flags & MP_ARRAY_AS_MAP) == MP_ARRAY_AS_MAP)
    mp_encode_lua_table_as_map(L, ud, idx, level);
  else if (mp_table_is_an_array(L, idx, ud->flags, &array_length))
    mp_encode_lua_table_as_array(L, ud, idx, level, array_length);
  else
    mp_encode_lua_table_as_map(L, ud, idx, level);
}

static LUA_MSGPACK_INLINE void lua_pack_extended_table (lua_State *L, lua_msgpack *ud, int idx, int level) {
  lua_Integer type = 0;
  if ((type = mp_ext_type(L, idx)) != EXT_INVALID) {
    if (!mp_encode_registered_ext_type(L, ud, idx, mp_cast(int8_t, type))) {
      luaL_error(L, "msgpack extension type: not registered!");
      return;
    }
  }
  else if (mp_encode_ext_metatable(L, ud, idx, 0, 0)) {
  }
  else if (mp_encode_registered_lua_type(L, ud, idx, LUA_TTABLE)) {
  }
  else {
    lua_pack_table(L, ud, idx, level);
  }
}

static LUA_MSGPACK_INLINE void lua_pack_parse_string (lua_State *L, lua_msgpack *ud, int idx) {
  msgpack_packer *pk = mp_get_packer(L, ud);

  size_t len = 0;
  const char *s = mp_nullptr;
  if ((s = lua_tolstring(L, idx, &len)) == mp_nullptr) {
    len = 0;
    s = "";
  }

  if ((ud->flags & MP_STRING_COMPAT) == MP_STRING_COMPAT) {
    msgpack_pack_v4raw(pk, len);
    msgpack_pack_v4raw_body(pk, s, len);
  }
  else if ((ud->flags & MP_STRING_BINARY) == MP_STRING_BINARY) {
    msgpack_pack_bin(pk, len);
    msgpack_pack_bin_body(pk, s, len);
  }
  else {
    msgpack_pack_str(pk, len);
    msgpack_pack_str_body(pk, s, len);
  }
}

static void lua_pack_vector (lua_State *L, lua_msgpack *ud, int idx) {
  union { lua_VecF f; lua_VecI i; } mem;
  unsigned char buf[4 * sizeof(lua_VecF)];
  msgpack_packer *pk = mp_get_packer(L, ud);

  lua_Float4 v;
  int type = 0, size = 0;
  switch ((type = mp_parse_vector(L, idx, &v))) {
    case MSGPACK_EXT_VECTOR2: {
      size = 2;
      _msgpack_storevec(&buf[0 * sizeof(lua_VecF)], (mem.f = v.x, mem.i));
      _msgpack_storevec(&buf[1 * sizeof(lua_VecF)], (mem.f = v.y, mem.i));
      break;
    }
    case MSGPACK_EXT_VECTOR3: {
      size = 3;
      _msgpack_storevec(&buf[0 * sizeof(lua_VecF)], (mem.f = v.x, mem.i));
      _msgpack_storevec(&buf[1 * sizeof(lua_VecF)], (mem.f = v.y, mem.i));
      _msgpack_storevec(&buf[2 * sizeof(lua_VecF)], (mem.f = v.z, mem.i));
      break;
    }
    case MSGPACK_EXT_VECTOR4:
    case MSGPACK_EXT_QUATERNION: {
      size = 4;
      _msgpack_storevec(&buf[0 * sizeof(lua_VecF)], (mem.f = v.x, mem.i));
      _msgpack_storevec(&buf[1 * sizeof(lua_VecF)], (mem.f = v.y, mem.i));
      _msgpack_storevec(&buf[2 * sizeof(lua_VecF)], (mem.f = v.z, mem.i));
      _msgpack_storevec(&buf[3 * sizeof(lua_VecF)], (mem.f = v.w, mem.i));
      break;
    }
    default:
      luaL_error(L, "invalid vector type");
      break;
  }
  msgpack_pack_ext(pk, size * sizeof(lua_VecF), (int8_t)type);
  msgpack_pack_ext_body(pk, (const void *)buf, size * sizeof(lua_VecF));
}

static LUA_MSGPACK_INLINE void lua_pack_type_extended (lua_State *L, lua_msgpack *ud, int idx) {
  const int t = lua_type(L, idx);
  lua_Integer type = 0;
  if ((type = mp_ext_type(L, idx)) != EXT_INVALID) {
    if (!mp_encode_registered_ext_type(L, ud, idx, mp_cast(int8_t, type))) {
      if ((ud->flags & MP_IGNORE_INVALID)) {
        msgpack_pack_nil(mp_get_packer(L, ud));
        return;
      }

      luaL_error(L, "msgpack extension type: not registered!");
      return;
    }
  }
  else if (mp_encode_ext_metatable(L, ud, idx, 0, 0)) {
  }
  else if (mp_encode_registered_lua_type(L, ud, idx, t)) {
  }
  else if ((ud->flags & MP_IGNORE_INVALID))
    msgpack_pack_nil(mp_get_packer(L, ud));
  else {
    luaL_error(L, "type <%s> cannot be msgpack'd", lua_typename(L, t));
  }
}

static LUA_MSGPACK_INLINE void lua_pack_any (lua_State *L, lua_msgpack *ud, int idx, int level) {
  int t = lua_type(L, idx);
#if defined(LUA_MSGPACK_ERROR_NESTING)
  if (t == LUA_TTABLE && level >= MP_MAX_NESTING) {
    luaL_error(L, "maximum table nesting depth (%d) exceeded", MP_MAX_NESTING);
    return;
  }
#else
  if (t == LUA_TTABLE && level >= MP_MAX_NESTING)
    t = LUA_TNIL;
#endif

  switch (t) {
    case LUA_TNIL: msgpack_pack_nil(mp_get_packer(L, ud)); break;
    case LUA_TBOOLEAN: lua_pack_boolean(L, ud, idx); break;
    case LUA_TNUMBER: lua_pack_number(L, ud, idx); break;
    case LUA_TSTRING: lua_pack_parse_string(L, ud, idx); break;
    case LUA_TTABLE: lua_pack_extended_table(L, ud, idx, level); break;
    case LUA_TFUNCTION: {
#if LUA_VERSION_NUM > 501
      if (mp_is_null(L, idx)) {
        msgpack_pack_nil(mp_get_packer(L, ud));
        break;
      }
#endif
      lua_pack_type_extended(L, ud, idx);
      break;
    }
#if LUA_VERSION_NUM >= 504
    case LUA_TVECTOR:
      lua_pack_vector(L, ud, idx);
      break;
#elif LUA_VERSION_NUM == 503
    case LUA_TVECTOR2:
    case LUA_TVECTOR3:
    case LUA_TVECTOR4:
    case LUA_TQUAT:
      lua_pack_vector(L, ud, idx);
      break;
#else
  #error unsupported Lua version
#endif
    case LUA_TLIGHTUSERDATA:
    case LUA_TUSERDATA:
    case LUA_TTHREAD:
    default:
      lua_pack_type_extended(L, ud, idx);
      break;
  }
}

#endif
