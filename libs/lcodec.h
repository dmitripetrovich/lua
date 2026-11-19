#ifndef lcodec_h
#define lcodec_h

#include <stddef.h>

#define LCODEC_MAX_LUAINDEX (sizeof(size_t) < sizeof(lua_Integer) ? (size_t)-1 : (size_t)LUA_MAXINTEGER)

typedef struct lcodec_Scan {
  size_t count;
  size_t max;
  size_t nfield;
  size_t entries;
  int isarray;
} lcodec_Scan;

static int lcodec_isnull (lua_State *L, int idx) {
  return lua_type(L, idx) == LUA_TLIGHTUSERDATA && lua_touserdata(L, idx) == NULL;
}

static size_t lcodec_optsizet (lua_State *L, int arg, size_t def) {
  if (lua_isnoneornil(L, arg))
    return def;
  if (lua_isinteger(L, arg)) {
    const lua_Integer i = lua_tointeger(L, arg);
    if (i >= 0 && (size_t)i <= LCODEC_MAX_LUAINDEX)
      return (size_t)i;
    luaL_argerror(L, arg, "invalid integer argument");
    return 0;
  }
  luaL_argerror(L, arg, lua_pushfstring(L, "integer expected"));
  return 0;
}

static int lcodec_isindex (lua_State *L, int idx, lua_Integer *n) {
  if (!lua_isinteger(L, idx))
    return 0;
  *n = lua_tointeger(L, idx);
  return *n >= 1 && (size_t)*n <= LCODEC_MAX_LUAINDEX;
}

static int lcodec_isnfield (lua_State *L, lua_Integer *n) {
  size_t keylen = 0;
  const char *key;
  if (lua_type(L, -2) != LUA_TSTRING || !lcodec_isindex(L, -1, n))
    return 0;
  key = lua_tolstring(L, -2, &keylen);
  return key != NULL && keylen == 1 && key[0] == 'n';
}

static void lcodec_scantable (lua_State *L, int idx, int usenfield, int full, lcodec_Scan *s) {
  const int stacktop = lua_gettop(L);
  s->count = 0;
  s->max = 0;
  s->nfield = 0;
  s->entries = 0;
  s->isarray = 1;
  lua_pushnil(L);
  while (lua_next(L, idx)) {
    lua_Integer n = 0;
    if (full)
      lua_checkhook(L, 1);
    s->entries++;
    if (s->isarray && lcodec_isindex(L, -2, &n)) {
      s->count++;
      if ((size_t)n > s->max)
        s->max = (size_t)n;
    }
    else if (s->isarray && usenfield && lcodec_isnfield(L, &n))
      s->nfield = (size_t)n;
    else {
      s->isarray = 0;
      if (!full)
        break;
    }
    lua_pop(L, 1);
  }
  lua_settop(L, stacktop);
}

#endif
