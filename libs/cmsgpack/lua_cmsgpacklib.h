#ifndef lua_cmsgpacklib_h
#define lua_cmsgpacklib_h

#include <lua.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(LUAMOD_API)
  #define LUAMOD_API LUALIB_API
#endif

#define LUA_MSGPACK_LIBNAME "msgpack"
LUAMOD_API int (luaopen_cmsgpack) (lua_State *L);

LUALIB_API int mp_pack (lua_State *L);

LUALIB_API int mp_pack_args (lua_State *L);

LUALIB_API int mp_unpack (lua_State *L);

LUALIB_API int mp_unpack_compat (lua_State *L);

LUALIB_API int mp_unpack_next (lua_State *L);

LUALIB_API int mp_packer_new (lua_State *L);

LUALIB_API int mp_set_extension (lua_State *L);

LUALIB_API int mp_get_extension (lua_State *L);

LUALIB_API int mp_clear_extension (lua_State *L);

LUALIB_API int mp_set_type_extension (lua_State *L);

LUALIB_API int mp_get_type_extension (lua_State *L);

LUALIB_API int mp_null (lua_State *L);

LUALIB_API int mp_setoption (lua_State *L);
LUALIB_API int mp_getoption (lua_State *L);

#if defined(__cplusplus)
}
#endif

#endif
