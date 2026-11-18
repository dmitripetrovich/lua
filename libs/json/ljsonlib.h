#ifndef ljsonlib_h
#define ljsonlib_h

#include <lua.h>

#if LUA_VERSION_NUM == 501
  #define LUAMOD_API LUALIB_API
#endif

#if defined(__cplusplus)
extern "C" {
#endif

LUALIB_API int ljson_encode(lua_State *L);
LUALIB_API int ljson_decode(lua_State *L);

LUALIB_API int ljson_setoption (lua_State *L);
LUALIB_API int ljson_getoption (lua_State *L);

LUALIB_API int ljson_object (lua_State *L);
LUALIB_API int ljson_isobject (lua_State *L);

LUALIB_API int ljson_array (lua_State *L);
LUALIB_API int ljson_isarray (lua_State *L);

#define LUA_JSON_LIBNAME "json"
LUAMOD_API int (luaopen_json) (lua_State *L);

#if defined(__cplusplus)
}
#endif

#endif
