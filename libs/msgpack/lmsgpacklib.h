#ifndef lmsgpacklib_h
#define lmsgpacklib_h

#include <lua.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(LUAMOD_API)
  #define LUAMOD_API LUALIB_API
#endif

#define LUA_MSGPACK_LIBNAME "msgpack"

LUAMOD_API int (luaopen_msgpack) (lua_State *L);

#if defined(__cplusplus)
}
#endif

#endif
