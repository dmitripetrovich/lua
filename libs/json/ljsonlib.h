#ifndef ljsonlib_h
#define ljsonlib_h

#include <lua.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(LUAMOD_API)
  #define LUAMOD_API LUALIB_API
#endif

#define LUA_JSON_LIBNAME "json"

LUAMOD_API int (luaopen_json) (lua_State *L);

#if defined(__cplusplus)
}
#endif

#endif
