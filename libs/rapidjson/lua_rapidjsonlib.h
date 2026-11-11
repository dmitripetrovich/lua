#ifndef lrapidjsonlib_h
#define lrapidjsonlib_h

#include <lua.h>

#if LUA_VERSION_NUM == 501
  #define LUAMOD_API LUALIB_API
#endif

#if defined(__cplusplus)
extern "C" {
#endif

LUALIB_API int rapidjson_encode(lua_State *L);
LUALIB_API int rapidjson_decode(lua_State *L);

LUALIB_API int rapidjson_setoption (lua_State *L);
LUALIB_API int rapidjson_getoption (lua_State *L);

LUALIB_API int rapidjson_object (lua_State *L);
LUALIB_API int rapidjson_isobject (lua_State *L);

LUALIB_API int rapidjson_array (lua_State *L);
LUALIB_API int rapidjson_isarray (lua_State *L);

#define LUA_RAPIDJSON_JSON_LIBNAME "json"
LUAMOD_API int (luaopen_rapidjson) (lua_State *L);

#if defined(__cplusplus)
}
#endif

#endif
