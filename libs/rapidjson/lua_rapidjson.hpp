#ifndef __LUA_RAPIDJSON_HPP__
#define __LUA_RAPIDJSON_HPP__

#if defined(__SSE4_2__)
  #define RAPIDJSON_SSE42
#elif defined(__SSE2__)
  #define RAPIDJSON_SSE2
#endif

#include <functional>
#include <cstring>
#include <vector>
#include <math.h>

#include <rapidjson/internal/stack.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#if defined(LUA_COMPILED_AS_HPP)
  #define LUA_RAPIDJSON_LINKAGE "C++"
#else
  #define LUA_RAPIDJSON_LINKAGE "C"
#endif

#if defined(LUA_INCLUDE_HPP)
  #include <lua.hpp>
#else
extern LUA_RAPIDJSON_LINKAGE {
  #include <lua.h>
  #include <lualib.h>
  #include <lauxlib.h>
}
#endif

extern LUA_RAPIDJSON_LINKAGE {
#if LUA_VERSION_NUM >= 504
  #include <lgrit_lib.h>
  #if LUA_VERSION_NUM >= 505
    #include <llimits.h>
  #endif
#elif LUA_VERSION_NUM == 503
  #include <llimits.h>
  #include <lobject.h>
#else
   #error unsupported Lua version
#endif
}

#include "StringStream.hpp"

#define LUA_RAPIDJSON_REG_ARRAY "lua_rapidjson_array"
#define LUA_RAPIDJSON_REG_OBJECT "lua_rapidjson_object"

#define LUA_RAPIDJSON_META_TOJSON "__tojson"
#define LUA_RAPIDJSON_META_ORDER "__jsonorder"
#define LUA_RAPIDJSON_META_TYPE "__jsontype"
#define LUA_RAPIDJSON_META_TYPE_ARRAY "array"
#define LUA_RAPIDJSON_META_TYPE_OBJECT "object"

#define LUA_RAPIDJSON_STATE_KEYORDER "keyorder"
#define LUA_RAPIDJSON_STATE_EXCEPTION "exception"

#define LUA_RAPIDJSON_ERROR_CYCLE "reference cycle"
#define LUA_RAPIDJSON_ERROR_FAIL "custom encoder failed"
#define LUA_RAPIDJSON_ERROR_TYPE "unsupported type"
#define LUA_RAPIDJSON_ERROR_NUMBER "error encoding number"
#define LUA_RAPIDJSON_ERROR_DEPTH_LIMIT "maximum table nesting depth exceeded"

#if defined(__cpp_constexpr) && __cpp_constexpr >= 201603L
  #define LUA_RAPIDJSON_IF_CONSTEXPR if constexpr
#else
  #define LUA_RAPIDJSON_IF_CONSTEXPR if
#endif

#if !defined(RAPIDJSON_DELIBERATE_FALLTHROUGH)
  #if defined(__has_cpp_attribute) && __has_cpp_attribute(fallthrough)
    #define RAPIDJSON_DELIBERATE_FALLTHROUGH [[fallthrough]]
  #else
    #define RAPIDJSON_DELIBERATE_FALLTHROUGH
  #endif
#endif

#if LUA_VERSION_NUM >= 503
  #define LUA_METAFIELD_FAIL LUA_TNIL
#else
  #define LUA_METAFIELD_FAIL 0
#endif

#if !defined(LUA_MAXINTEGER)
  #define LUA_MAXINTEGER std::numeric_limits<lua_Integer>::max()
  #define LUA_MININTEGER std::numeric_limits<lua_Integer>::min()
#endif

#define JSON_UNUSED(x) ((void)(x))

#if LUA_VERSION_NUM >= 503
  #define JSON_MAX_LUAINDEX (sizeof(size_t) < sizeof(lua_Integer) ? std::numeric_limits<size_t>::max() \
                                                                  : static_cast<size_t>(LUA_MAXINTEGER))
#else
  #define JSON_MAX_LUAINDEX std::numeric_limits<size_t>::max()
#endif

#define json_rel_index(idx, n) (((idx) < 0) ? ((idx) - (n)) : (idx))

#if LUA_VERSION_NUM >= 503
  #define json_isinteger(L, idx) lua_isinteger((L), (idx))
#else
static inline int json_isinteger (lua_State *L, int idx) {
  if (LUA_TNUMBER == lua_type(L, idx)) {
    const lua_Number n = lua_tonumber(L, idx);
    return !isinf(n) && static_cast<lua_Number>(static_cast<lua_Integer>(n)) == n;
  }
  return 0;
}
#endif

#if defined(LUA_RAPIDJSON_UNSAFE)
  #define json_checkstack(L, sz) ((void)0)
#else
  #define json_checkstack(L, sz)              \
    do {                                      \
      if (!lua_checkstack((L), (sz)))         \
        throw rapidjson::LuaStackException(); \
    } while ( 0 )
#endif

#if defined(LUA_COMPILED_AS_HPP)
  #define json_call(L, n, r)                              \
    do {                        \
      if (lua_pcall(L, n, r, 0) != LUA_OK)                \
        throw rapidjson::LuaCallException(lua_gettop(L)); \
    } while (0)
#else
  #define json_call(L, n, r) lua_call(L, n, r)
#endif

#if LUA_VERSION_NUM >= 503
  #define json_getfield(L, I, K) lua_getfield((L), (I), (K))
  #define json_gettable(L, I) lua_gettable((L), (I))
#else
  #define json_getfield(L, I, K) (lua_getfield((L), (I), (K)), lua_type((L), -1))
  #define json_gettable(L, I) (lua_gettable((L), (I)), lua_type((L), -1))
#endif

#if LUA_VERSION_NUM >= 504
  #define json_newuserdata(L, s) lua_newuserdatauv((L), (s), 0)
#elif LUA_VERSION_NUM >= 501 && LUA_VERSION_NUM <= 503
  #define json_newuserdata(L, s) lua_newuserdata((L), (s))
#else
  #error unsupported Lua version
#endif

#if !defined(LUA_RAPIDJSON_BIT32) && UINTPTR_MAX == UINT_MAX
  #define LUA_RAPIDJSON_BIT32
#endif

#if !defined(LUA_RAPIDJSON_TABLE_CUTOFF)
  #define LUA_RAPIDJSON_TABLE_CUTOFF 11
#endif

#if !defined(LUA_RAPIDJSON_DEFAULT_DEPTH)
  #define LUA_RAPIDJSON_DEFAULT_DEPTH 32
#endif

#define LUA_RAPIDJSON_SOURCE rapidjson::UTF8<>
#define LUA_RAPIDJSON_TARGET rapidjson::UTF8<>

  #define RAPIDJSON_ALLOCATOR rapidjson::LuaAllocator
  #define RAPIDJSON_ALLOCATOR_INIT(L, NAME) rapidjson::LuaAllocator NAME(L)

#if LUA_VERSION_NUM >= 503
  #if LUA_FLOAT_TYPE == LUA_FLOAT_FLOAT
    #define LUA_NUMBER_FMT_LEN 7
  #elif LUA_FLOAT_TYPE == LUA_FLOAT_LONGDOUBLE
    #define LUA_NUMBER_FMT_LEN 19
  #elif LUA_FLOAT_TYPE == LUA_FLOAT_DOUBLE
    #define LUA_NUMBER_FMT_LEN 14
  #else
    #error "numeric float type not defined"
  #endif
#else
  #define LUA_NUMBER_FMT_LEN 14
#endif

#define MAXNUMBER2STR 44

#if !defined(l_sprintf)
  #if defined(LUA_USE_WINDOWS) && defined(__STDC_WANT_SECURE_LIB__)
    #define l_sprintf(s, sz, f, i) sprintf_s((s), (sz), (f), (i))
  #else
    #define l_sprintf(s, sz, f, i) snprintf((s), (sz), (f), (i))
  #endif
#endif

static const char *lua_dtoa(char *s, size_t n, double value) {
  const char *end = nullptr;

  int l = l_sprintf(s, n, LUA_NUMBER_FMT, value);
  if (s[strspn(s, "-0123456789")] == '\0') {
    s[l++] = '.';
    s[l++] = '0';
    return s + l;
  }
  else {
    end = s + l;
    for (char *begin = s; begin != end; ++begin) {
      if (*begin == ',')
        *begin = '.';
    }
  }
  return end;
}

#define _EXP2(a, b) a##b
#define EXP(digits) _EXP2(1##E, digits)

static inline double lua_grisuRound(double d) {
  if ((std::numeric_limits<double>::max() / EXP(LUA_NUMBER_FMT_LEN)) <= d)
    return d;

#if __cplusplus <= 199711L
    double e = d * EXP(LUA_NUMBER_FMT_LEN);
    e = (e < 0) ? static_cast<double>(long(e - 0.5)) : static_cast<double>(long(e + 0.5));
    return e / EXP(LUA_NUMBER_FMT_LEN);
#else
    return std::round(d * EXP(LUA_NUMBER_FMT_LEN)) / EXP(LUA_NUMBER_FMT_LEN);
#endif
}

#define LUA_RAPIDJSON_API LUAI_FUNC

LUA_RAPIDJSON_API int json_null (lua_State *L);

LUA_RAPIDJSON_API bool is_json_null (lua_State *L, int idx);

LUA_RAPIDJSON_API bool has_json_type (lua_State *L, int idx, bool *is_array);

LUA_RAPIDJSON_API bool table_is_json_array (lua_State *L, int idx, lua_Integer flags, size_t *array_length);

static inline int parseVector (lua_State *L, int idx, lua_Float4 *f) {
  int args = 0;
#if LUA_VERSION_NUM >= 504
  switch (lua_tofloat4(L, idx, V_PARSETABLE, f)) {
    case LUA_VVECTOR1: args = 1; break;
    case LUA_VVECTOR2: args = 2; break;
    case LUA_VVECTOR3: args = 3; break;
    case LUA_VQUAT: case LUA_VVECTOR4: args = 4; break;
    default:
      luaL_typeerror(L, idx, "number or vector type");
      break;
  }
#elif LUA_VERSION_NUM == 503
  switch (lua_type(L, idx)) {
    case LUA_TVECTOR2: args = 2; lua_checkvector2(L, idx, &f->x, &f->y); break;
    case LUA_TVECTOR3: args = 3; lua_checkvector3(L, idx, &f->x, &f->y, &f->z); break;
    case LUA_TVECTOR4: args = 4; lua_checkvector4(L, idx, &f->x, &f->y, &f->z, &f->w); break;
    case LUA_TQUAT: args = 4; lua_checkquat(L, idx, &f->w, &f->x, &f->y, &f->z); break;
    default:
      const char *msg = lua_pushfstring(L, "%s expected, got %s", "vector", luaL_typename(L, idx));
      luaL_argerror(L, idx, msg);
  }
#else
#error unsupported Lua version
#endif
  return args;
}

#define JSON_OPTION_RESERVED    0x0
#define JSON_PRETTY_PRINT       0x01
#define JSON_SORT_KEYS          0x02
#define JSON_LUA_NULL           0x04
#define JSON_NESTING_NULL       0x08
#define JSON_UNSIGNED_INTEGERS  0x10
#define JSON_NAN_AND_INF        0x20
#define JSON_ENCODE_INT32       0x40
#define JSON_ENCODER_ARRAY_VECTOR 0x80

#define JSON_LUA_DTOA           0x100
#define JSON_LUA_GRISU          0x200

#define JSON_ARRAY_SINGLE_LINE  0x10000
#define JSON_ARRAY_EMPTY        0x20000
#define JSON_ARRAY_WITH_HOLES   0x40000

#define JSON_ENCODER_HANDLER    0x2000000
#define JSON_DECODER_PRESET     0x4000000
#define JSON_ENCODER_DECIMALS   0x8000000
#define JSON_ENCODER_INDENT     0x10000000
#define JSON_ENCODER_INDENT_AMT 0x20000000
#define JSON_ENCODER_MAX_DEPTH  0x40000000
#define JSON_TABLE_KEY_ORDER    0x80000000

#if defined(LUA_RAPIDJSON_BIT32)
  #define JSON_DEFAULT_BIT32 JSON_ENCODE_INT32
#else
  #define JSON_DEFAULT_BIT32 JSON_OPTION_RESERVED
#endif

#define JSON_DEFAULT (JSON_LUA_NULL | JSON_ARRAY_EMPTY | JSON_ARRAY_WITH_HOLES | JSON_NAN_AND_INF | JSON_DEFAULT_BIT32)

namespace LuaSAX {
  struct Key : std::unary_function<LuaSAX::Key, bool> {
    bool is_number, is_integer;
    union Data {
      lua_Number n;
      lua_Integer i;
      struct {
        const char *key;
        size_t len;
      } s;

      Data() = default;
      Data(lua_Number _n) : n(_n) { }
      Data(lua_Integer _i) : i(_i) { }
      Data(const char *_s, size_t _l) {
        s.key = _s;
        s.len = _l;
      }
    } data;

    Key() = default;
    Key(lua_Number _nu) : is_number(true), is_integer(false), data(_nu) { }
    Key(lua_Integer _in) : is_number(true), is_integer(true), data(_in) { }
    Key(const char *_str, size_t _len) : is_number(false), is_integer(false), data(_str, _len) { }

    RAPIDJSON_FORCEINLINE lua_Number asNumber() const {
      return is_integer ? static_cast<lua_Number>(data.i) : data.n;
    }

    bool operator<(const LuaSAX::Key &k) const {
      if (is_number) return k.is_number ? (asNumber() < k.asNumber()) : true;
      return k.is_number ? false : (strcmp(data.s.key, k.data.s.key) < 0);
    }

    bool operator()(const LuaSAX::Key &k) const {
      if (is_number) return k.is_number ? (asNumber() == k.asNumber()) : false;
      return k.is_number ? false : (strcmp(data.s.key, k.data.s.key) == 0);
    }
  };

  static int populate_key_vector (lua_State *L, int idx, std::vector<LuaSAX::Key> &sink) {
#if LUA_VERSION_NUM >= 502
    const size_t length = static_cast<size_t>(lua_rawlen(L, idx));
#else
    const size_t length = lua_objlen(L, idx);
#endif
    for (size_t i = 1; i <= length; ++i) {
#if LUA_VERSION_NUM >= 503
      const int type = lua_rawgeti(L, idx, static_cast<lua_Integer>(i));
#else
      lua_pushinteger(L, static_cast<lua_Integer>(i));
      lua_rawget(L, json_rel_index(idx, 1));
      const int type = lua_type(L, -1);
#endif
      switch (type) {
        case LUA_TNUMBER: {
#if LUA_VERSION_NUM >= 503
          if (lua_isinteger(L, -1))
            sink.push_back(LuaSAX::Key(lua_tointeger(L, -1)));
          else
#endif
          sink.push_back(LuaSAX::Key(lua_tonumber(L, -1)));
          break;
        }
        case LUA_TSTRING: {
          size_t len = 0;
          const char *s = lua_tolstring(L, -1, &len);
          sink.push_back(LuaSAX::Key(s, len));
          break;
        }
        default: {
#if defined(LUA_RAPIDJSON_SANITIZE_KEYS)
          throw rapidjson::LuaTypeException(type, rapidjson::LuaTypeException::UnsupportedKeyOrder);
#endif
          break;
        }
      }
      lua_pop(L, 1);
    }
    return 0;
  }

  template<typename StackAllocator>
  struct Decoder {
private:
    struct Ctx {
      typedef void (*ctx_callback) (lua_State *, struct Ctx &);
      rapidjson::SizeType index;
      ctx_callback callback;

      Ctx() : index(0), callback(&Unused) { }
      Ctx(const Ctx &rhs) : index(rhs.index), callback(rhs.callback) { }
      explicit Ctx(ctx_callback f) : index(0), callback(f) { }

      const Ctx &operator=(const Ctx &rhs) {
        if (this != &rhs) {
          index = rhs.index;
          callback = rhs.callback;
        }
        return *this;
      }

      RAPIDJSON_FORCEINLINE void Push(lua_State *Ls) { callback(Ls, *this); }

      static void Unused(lua_State *L, Ctx &ctx) {
        JSON_UNUSED(L);
        JSON_UNUSED(ctx);
      }

      static RAPIDJSON_FORCEINLINE Ctx Object() {
        return Ctx([](lua_State *_L, Ctx &ctx) {
          JSON_UNUSED(ctx);
          lua_rawset(_L, -3);
        });
      }

      static RAPIDJSON_FORCEINLINE Ctx Array() {
        return Ctx([](lua_State *_L, Ctx &ctx) {
#if LUA_VERSION_NUM >= 503
          lua_rawseti(_L, -2, ++ctx.index);
#else
          lua_pushinteger(_L, ++ctx.index);
          lua_pushvalue(_L, -2);
          lua_rawset(_L, -4);
          lua_pop(_L, 1);
#endif
        });
      }
    };

    lua_State *L;
    rapidjson::internal::Stack<StackAllocator> &stack_;
    lua_Integer flags;
    int nullarg;
    int objectarg;
    int arrayarg;
    Ctx context_;

public:
    explicit Decoder(lua_State *_L, rapidjson::internal::Stack<StackAllocator> &_stack, lua_Integer _flags = 0, int _nullidx = -1, int _oidx = -1, int _aidx = -1)
      : L(_L), stack_(_stack), flags(_flags), nullarg(_nullidx), objectarg(_oidx), arrayarg(_aidx) {
#if LUA_RAPIDJSON_DEFAULT_DEPTH <= 64
      stack_.template Reserve<Ctx>(LUA_RAPIDJSON_DEFAULT_DEPTH >> 1);
#else
      stack_.template Reserve<Ctx>(1 << 4);
#endif
    }

#if defined(LUA_RAPIDJSON_COMPAT)
    #define LUA_JSON_HANDLE(NAME, ...) RAPIDJSON_FORCEINLINE bool NAME(__VA_ARGS__, bool mapValue = false)
    #define LUA_JSON_HANDLE_NULL(NAME) RAPIDJSON_FORCEINLINE bool NAME(bool mapValue = false)
    #define LUA_JSON_SUBMIT() \
      do {                    \
        if (!mapValue)        \
          context_.Push(L);   \
      } while ( 0 )

    bool ImplicitArrayInObjectContext(const rapidjson::SizeType u) {
#if LUA_VERSION_NUM >= 503
      lua_rawseti(L, -2, static_cast<lua_Integer>(u));
#else
      lua_json_checkstack(L, 3);
      lua_pushinteger(L, static_cast<lua_Integer>(u));
      lua_pushvalue(L, -2);
      lua_rawset(L, -4);
      lua_pop(L, 1);
#endif
      return true;
    }

    bool ImplicitObjectInContext() {
      lua_rawset(L, -3);
      return true;
    }
#else
    #define LUA_JSON_SUBMIT() context_.Push(L)
    #define LUA_JSON_HANDLE(NAME, ...) RAPIDJSON_FORCEINLINE bool NAME(__VA_ARGS__)
    #define LUA_JSON_HANDLE_NULL(NAME) RAPIDJSON_FORCEINLINE bool NAME()
#endif

    LUA_JSON_HANDLE_NULL(Null) {
      if (nullarg > 0)
        lua_pushvalue(L, nullarg);
      else if ((flags & JSON_LUA_NULL))
        lua_pushnil(L);
      else
        json_null(L);
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(Bool, bool b) {
      lua_pushboolean(L, b);
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(Int, int i) {
      lua_pushinteger(L, i);
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(Uint, unsigned u) {
      LUA_RAPIDJSON_IF_CONSTEXPR (sizeof(lua_Integer) > sizeof(unsigned) || u <= static_cast<unsigned>(LUA_MAXINTEGER))
        lua_pushinteger(L, static_cast<lua_Integer>(u));
      else
        lua_pushnumber(L, static_cast<lua_Number>(u));
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(Int64, int64_t i) {
      LUA_RAPIDJSON_IF_CONSTEXPR (sizeof(lua_Integer) >= sizeof(int64_t) || (i <= LUA_MAXINTEGER && i >= LUA_MININTEGER))
        lua_pushinteger(L, static_cast<lua_Integer>(i));
      else
        lua_pushnumber(L, static_cast<lua_Number>(i));
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(Uint64, uint64_t u) {
      if (sizeof(lua_Integer) > sizeof(uint64_t) || u <= static_cast<uint64_t>(LUA_MAXINTEGER))
        lua_pushinteger(L, static_cast<lua_Integer>(u));
      else
        lua_pushnumber(L, static_cast<lua_Number>(u));
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(Double, double d) {
      lua_pushnumber(L, static_cast<lua_Number>(d));
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(RawNumber, const char *str, rapidjson::SizeType length, bool copy) {
      JSON_UNUSED(copy);

      lua_getglobal(L, "tonumber");
      lua_pushlstring(L, str, length);
      json_call(L, 1, 1);
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(String, const char *str, rapidjson::SizeType length, bool copy) {
      JSON_UNUSED(copy);

      lua_pushlstring(L, str, length);
      LUA_JSON_SUBMIT();
      return true;
    }

    RAPIDJSON_FORCEINLINE bool StartObject() {
#if !defined(LUA_RAPIDJSON_UNSAFE)
      if (lua_checkstack(L, 2)) {
#endif
        lua_createtable(L, 0, 0);
        if (objectarg > 0)
          lua_pushvalue(L, objectarg);
        else
          luaL_getmetatable(L, LUA_RAPIDJSON_REG_OBJECT);
        lua_setmetatable(L, -2);

        *stack_.template Push<Ctx>(1) = context_;
        context_ = Ctx::Object();
        return true;
#if !defined(LUA_RAPIDJSON_UNSAFE)
      }
      return false;
#endif
    }

    RAPIDJSON_FORCEINLINE bool Key(const char *str, rapidjson::SizeType length, bool copy) const {
      JSON_UNUSED(copy);
      lua_pushlstring(L, str, length);
      return true;
    }

    LUA_JSON_HANDLE(EndObject, rapidjson::SizeType memberCount) {
      JSON_UNUSED(memberCount);

      context_ = *stack_.template Pop<Ctx>(1);
      LUA_JSON_SUBMIT();
      return true;
    }

    RAPIDJSON_FORCEINLINE bool StartArray() {
#if !defined(LUA_RAPIDJSON_UNSAFE)
      if (lua_checkstack(L, 2)) {
#endif
        lua_createtable(L, 0, 0);
        if (arrayarg > 0)
          lua_pushvalue(L, arrayarg);
        else
          luaL_getmetatable(L, LUA_RAPIDJSON_REG_ARRAY);
        lua_setmetatable(L, -2);

        *stack_.template Push<Ctx>(1) = context_;
        context_ = Ctx::Array();
        return true;
#if !defined(LUA_RAPIDJSON_UNSAFE)
      }
      return false;
#endif
    }

    LUA_JSON_HANDLE(EndArray, rapidjson::SizeType elementCount) {
#if !defined(LUA_RAPIDJSON_COMPAT)
      lua_assert(elementCount == context_.index);
#endif
      JSON_UNUSED(elementCount);

      context_ = *stack_.template Pop<Ctx>(1);
      LUA_JSON_SUBMIT();
      return true;
    }
  };

  class Encoder {
private:
    lua_Integer flags;
    int max_depth;
    int error_handler_idx;
    std::vector<LuaSAX::Key> &order;

    template<typename Writer>
    bool OrderedKey(const LuaSAX::Key &key, Writer &writer) const {
#if LUA_VERSION_NUM >= 503
      if (key.is_integer) {
        char buffer[MAXNUMBER2STR];
        const char *end = rapidjson::internal::i64toa(static_cast<int64_t>(key.data.i), buffer);
        return writer.Key(buffer, static_cast<rapidjson::SizeType>(end - buffer));
      }
      else
#endif
      if (key.is_number) {
        const double d = static_cast<double>(key.data.n);
        if (rapidjson::internal::Double(d).IsNanOrInf()) {
          if (!(flags & JSON_NAN_AND_INF))
            return false;

          const char *lit = "";
          if (rapidjson::internal::Double(d).IsNan())
            lit = "NaN";
          else if (rapidjson::internal::Double(d).Sign())
            lit = "-Infinity";
          else
            lit = "Infinity";
          return writer.Key(lit, static_cast<rapidjson::SizeType>(strlen(lit)));
        }

        char buffer[MAXNUMBER2STR + 2] = { 0 };
        const char *end = buffer;
        if (flags & JSON_LUA_DTOA)
          end = lua_dtoa(buffer, MAXNUMBER2STR, d);
        else {
          const double _d = (flags & JSON_LUA_GRISU) ? lua_grisuRound(d) : d;
          end = rapidjson::internal::dtoa(_d, buffer, writer.GetMaxDecimalPlaces());
        }
        return writer.Key(buffer, static_cast<rapidjson::SizeType>(end - buffer));
      }
      return writer.Key(key.data.s.key, static_cast<rapidjson::SizeType>(key.data.s.len));
    }

    void populate_unordered_vector(lua_State *L, int idx, const std::vector<LuaSAX::Key> &key_order, std::vector<LuaSAX::Key> &sink) const {
      const int i_idx = json_rel_index(idx, 1);
      json_checkstack(L, 3);

      lua_pushnil(L);
      while (lua_next(L, i_idx)) {
        switch (lua_type(L, -2)) {
          case LUA_TNUMBER: {
            LuaSAX::Key k;
#if LUA_VERSION_NUM >= 503
            if (lua_isinteger(L, -1))
              k = LuaSAX::Key(lua_tointeger(L, -1));
            else
#endif
            k = LuaSAX::Key(lua_tonumber(L, -2));
            if (std::find_if(key_order.begin(), key_order.end(), k) == key_order.end())
              sink.push_back(k);
            break;
          }
          case LUA_TSTRING: {
            size_t len = 0;
            const char *s = lua_tolstring(L, -2, &len);

            LuaSAX::Key k(s, len);
            if (std::find_if(key_order.begin(), key_order.end(), k) == key_order.end())
              sink.push_back(k);
            break;
          }
          default:
#if defined(LUA_RAPIDJSON_SANITIZE_KEYS)
            throw rapidjson::LuaTypeException(lua_type(L, -2), rapidjson::LuaTypeException::UnsupportedKeyOrder);
#endif
            break;
        }

        lua_pop(L, 1);
      }
    }

    template<typename Writer>
    bool handle_exception(lua_State *L, Writer &writer, int idx, int depth, const char *reason, const char **output) const {
      bool result = false;
      if (error_handler_idx > 0) {
        json_checkstack(L, 3);
        lua_pushvalue(L, error_handler_idx);
        lua_pushstring(L, reason);
        lua_pushvalue(L, json_rel_index(idx, 2));
        json_call(L, 2, 2);

        if (lua_isnil(L, -2))
          *output = luaL_optstring(L, -1, nullptr);
        else {
          encodeValue(L, writer, -2, depth + 1);
          result = true;
        }
        lua_pop(L, 2);
      }
      return result;
    }

public:
    Encoder(lua_Integer _flags, int _maxdepth, int _error_handler_idx, std::vector<LuaSAX::Key> &_order)
      : flags(_flags), max_depth(_maxdepth), error_handler_idx(_error_handler_idx), order(_order) {
    }

    template<typename Writer>
    void encodeValue(lua_State *L, Writer &writer, int idx, int depth = 0) const {
      switch (lua_type(L, idx)) {
        case LUA_TNIL:
          writer.Null();
          break;
        case LUA_TBOOLEAN:
          writer.Bool(lua_toboolean(L, idx) != 0);
          break;
        case LUA_TNUMBER: {
          if (json_isinteger(L, idx)) {
            if (flags & JSON_ENCODE_INT32) {
              if (flags & JSON_UNSIGNED_INTEGERS)
                writer.Uint(static_cast<unsigned>(lua_tointeger(L, idx)));
              else
                writer.Int(static_cast<int>(lua_tointeger(L, idx)));
            }
            else {
              if (flags & JSON_UNSIGNED_INTEGERS)
                writer.Uint64(static_cast<uint64_t>(lua_tointeger(L, idx)));
              else
                writer.Int64(static_cast<int64_t>(lua_tointeger(L, idx)));
            }
          }
          else {
            const double d = static_cast<double>(lua_tonumber(L, idx));
            const bool is_inf = rapidjson::internal::Double(d).IsNanOrInf();

#if defined(LUA_RAPIDJSON_COMPAT)
            if (is_inf)
              writer.Null();
            else
#endif
            if ((flags & JSON_LUA_DTOA) && !is_inf) {
              char buffer[MAXNUMBER2STR + 2] = { 0 };
              const char *end = lua_dtoa(buffer, MAXNUMBER2STR, d);
              if (!writer.RawValue(buffer, static_cast<rapidjson::SizeType>(end - buffer), rapidjson::kNumberType))
                throw rapidjson::LuaException("error encoding lua float");
            }
            else {
              const double _d = ((flags & JSON_LUA_GRISU) && !is_inf) ? lua_grisuRound(d) : d;
              if (!writer.Double(_d)) {
                const char *output = nullptr;
                if (!handle_exception(L, writer, idx, depth, LUA_RAPIDJSON_ERROR_NUMBER, &output)) {
                  throw rapidjson::LuaException((output != nullptr) ? output : "error encoding: kWriteNanAndInfFlag");
                }
              }
            }
          }
          break;
        }
#if LUA_VERSION_NUM >= 504
        case LUA_TVECTOR: {
#elif LUA_VERSION_NUM == 503
        case LUA_TVECTOR2:
        case LUA_TVECTOR3:
        case LUA_TVECTOR4:
        case LUA_TQUAT: {
#else
  #error unsupported Lua version
#endif
          lua_Float4 v;
          int args = parseVector(L, idx, &v);
          if (flags & JSON_ENCODER_ARRAY_VECTOR) {
            writer.StartArray();
            if (args) { writer.Double(static_cast<double>(v.x)); args--; }
            if (args) { writer.Double(static_cast<double>(v.y)); args--; }
            if (args) { writer.Double(static_cast<double>(v.z)); args--; }
            if (args) { writer.Double(static_cast<double>(v.w)); args--; }
            writer.EndArray();
          }
          else {
            writer.StartObject();
            if (args) { if (writer.Key("x")) writer.Double(static_cast<double>(v.x)); args--; }
            if (args) { if (writer.Key("y")) writer.Double(static_cast<double>(v.y)); args--; }
            if (args) { if (writer.Key("z")) writer.Double(static_cast<double>(v.z)); args--; }
            if (args) { if (writer.Key("w")) writer.Double(static_cast<double>(v.w)); args--; }
            writer.EndObject();
          }
          break;
        }
        case LUA_TSTRING: {
          size_t len;
          const char *s = lua_tolstring(L, idx, &len);
          if (!writer.String(s, static_cast<rapidjson::SizeType>(len)))
            throw rapidjson::LuaException("error encoding string");
          break;
        }
        case LUA_TTABLE: {
          encodeTable(L, writer, idx, depth + 1);
          break;
        }
        case LUA_TFUNCTION: {
          if (is_json_null(L, idx)) {
            writer.Null();
            break;
          }
          RAPIDJSON_DELIBERATE_FALLTHROUGH;
        }
        case LUA_TLIGHTUSERDATA:
        case LUA_TUSERDATA:
        case LUA_TTHREAD:
        case LUA_TNONE:
        default: {
          if (!encodeMetafield(L, writer, idx, depth)) {
            const char *output = nullptr;
            if (!handle_exception(L, writer, idx, depth, LUA_RAPIDJSON_ERROR_TYPE, &output)) {
              if (output)
                throw rapidjson::LuaException(output);
              else
                throw rapidjson::LuaTypeException(lua_type(L, idx), rapidjson::LuaTypeException::UnsupportedType);
            }
          }
          break;
        }
      }
    }

    template<typename Writer>
    bool encodeMetafield(lua_State *L, Writer &writer, int idx, int depth) const {
      JSON_UNUSED(depth);
      const int type = luaL_getmetafield(L, idx, LUA_RAPIDJSON_META_TOJSON);
      if (type == LUA_METAFIELD_FAIL)
        return false;

      bool result = true;
#if LUA_VERSION_NUM > 502
      if (type == LUA_TFUNCTION) {
#else
      if (lua_type(L, -1) == LUA_TFUNCTION) {
#endif
        lua_pushvalue(L, json_rel_index(idx, 1));
        json_call(L, 1, 1);
        if (lua_type(L, -1) == LUA_TSTRING) {
          size_t len;
          const char *s = lua_tolstring(L, -1, &len);
          if (!writer.RawValue(s, len, rapidjson::Type::kObjectType))
            throw rapidjson::LuaException("error encoding raw value");
        }
        else {
          result = false;
          throw rapidjson::LuaException("Invalid " LUA_RAPIDJSON_META_TOJSON " result");
        }
        lua_pop(L, 1);
      }
      else {
        result = false;
        throw rapidjson::LuaException("Invalid " LUA_RAPIDJSON_META_TOJSON " function");
      }
      return result;
    }

    template<typename Writer>
    void encodeTable(lua_State *L, Writer &writer, int idx, int depth) const {
      const int top = lua_gettop(L);
      if (depth > max_depth) {
        const char *output = nullptr;
        if (!handle_exception(L, writer, idx, depth, LUA_RAPIDJSON_ERROR_CYCLE, &output)) {
          if (flags & JSON_NESTING_NULL)
            writer.Null();
          else
            throw rapidjson::LuaException((output != nullptr) ? output : LUA_RAPIDJSON_ERROR_DEPTH_LIMIT);
        }
        return;
      }

      size_t array_length;
      if (encodeMetafield(L, writer, idx, depth)) {
      }
      else if (table_is_json_array(L, idx, flags, &array_length))
        encode_array(L, writer, idx, array_length, depth);
      else if (luaL_getmetafield(L, idx, LUA_RAPIDJSON_META_ORDER) != LUA_METAFIELD_FAIL) {
        if (lua_type(L, -1) == LUA_TFUNCTION) {
          lua_pushvalue(L, json_rel_index(idx, 1));
          json_call(L, 1, 1);
        }

        if (lua_type(L, -1) == LUA_TTABLE) {
          std::vector<LuaSAX::Key> meta_order, unorder;
          populate_key_vector(L, -1, meta_order);
          lua_settop(L, top);

          populate_unordered_vector(L, idx, meta_order, unorder);
          encodeOrderedObject(L, writer, idx, depth, meta_order, unorder);
        }
        else {
          throw rapidjson::LuaException("Invalid " LUA_RAPIDJSON_META_ORDER " result");
        }
      }
      else if ((flags & JSON_SORT_KEYS) != 0 || order.size() != 0) {
        std::vector<LuaSAX::Key> unorder;
        populate_unordered_vector(L, idx, order, unorder);
        if (flags & JSON_SORT_KEYS)
          std::sort(unorder.begin(), unorder.end());

        encodeOrderedObject(L, writer, idx, depth, order, unorder);
      }
      else {
        encodeObject(L, writer, idx, depth);
      }
    }

    template<typename Writer>
    void encode_array(lua_State *L, Writer &writer, int idx, size_t array_length, int depth) const {
      writer.StartArray();
      for (size_t i = 1; i <= array_length; ++i) {
#if LUA_VERSION_NUM >= 503
        lua_rawgeti(L, idx, static_cast<lua_Integer>(i));
#else
        lua_pushinteger(L, static_cast<lua_Integer>(i));
        lua_rawget(L, json_rel_index(idx, 1));
#endif
        encodeValue(L, writer, -1, depth);
        lua_pop(L, 1);
      }
      writer.EndArray();
    }

    template<typename Writer>
    void encodeObject(lua_State *L, Writer &writer, int idx, int depth) const {
      const int i_idx = json_rel_index(idx, 1);
      json_checkstack(L, 3);

      writer.StartObject();
      lua_pushnil(L);
      while (lua_next(L, i_idx)) {
        LuaSAX::Key key;
        switch (lua_type(L, -2)) {
          case LUA_TSTRING: {
            size_t len = 0;
            const char *s = lua_tolstring(L, -2, &len);
            key = LuaSAX::Key(s, len);
            break;
          }
          case LUA_TNUMBER: {
#if LUA_VERSION_NUM >= 503
            if (json_isinteger(L, -2))
              key = LuaSAX::Key(lua_tointeger(L, -2));
            else
#endif
            key = LuaSAX::Key(lua_tonumber(L, -2));
            break;
          }
          default: {
#if defined(LUA_RAPIDJSON_SANITIZE_KEYS)
            throw rapidjson::LuaTypeException(lua_type(L, -2), rapidjson::LuaTypeException::UnsupportedKeyOrder);
#else
            lua_pop(L, 1);
            continue;
#endif
          }
        }

        if (OrderedKey<Writer>(key, writer))
          encodeValue(L, writer, -1, depth);

        lua_pop(L, 1);
      }
      writer.EndObject();
    }

    template<typename Writer>
    void encodeOrderedObject(lua_State *L, Writer &writer, int idx, int depth, const std::vector<LuaSAX::Key> &keyorder, const std::vector<LuaSAX::Key> &unordered) const {
      const int i_idx = json_rel_index(idx, 1);
      json_checkstack(L, 2);

      writer.StartObject();

      auto oi = keyorder.end();
      for (auto i = keyorder.begin(); i != oi; ++i) {
        if (i->is_integer)
          lua_pushinteger(L, i->data.i);
        else if (i->is_number)
          lua_pushnumber(L, i->data.n);
        else
          lua_pushlstring(L, i->data.s.key, i->data.s.len);

        if (json_gettable(L, i_idx) != LUA_TNIL && OrderedKey<Writer>(*i, writer))
          encodeValue(L, writer, -1, depth);

        lua_pop(L, 1);
      }

      auto e = unordered.end();
      for (auto i = unordered.begin(); i != e; ++i) {
        if (i->is_integer)
          lua_pushinteger(L, i->data.i);
        else if (i->is_number)
          lua_pushnumber(L, i->data.n);
        else
          lua_pushlstring(L, i->data.s.key, i->data.s.len);

        if (json_gettable(L, i_idx) != LUA_TNIL && OrderedKey<Writer>(*i, writer))
          encodeValue(L, writer, -1, depth);

        lua_pop(L, 1);
      }

      writer.EndObject();
    }
  };
}

#endif
