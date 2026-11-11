/*
** $Id: lgrit_lib.h $
** API definitions for gritLua.
**
** All functions defined in this header are expected to have default linkage and
** interface with other Lua libraries. The vector API in lua.h supersedes this
** header, which is kept for libraries written against the gritLua API.
**
** See Copyright Notice in lua.h
*/

#ifndef lgritlib_h
#define lgritlib_h

#include "luaconf.h"
#include "lua.h"
#include "lauxlib.h"

/* Avoid old-style-cast for C++ libraries */
#if defined(__cplusplus)
  #define cast_vec(i) static_cast<lua_VecF>((i))
#else
  #define cast_vec(i) ((lua_VecF)(i))
#endif

#if !defined(LABEL_VECTOR3)
  #define LABEL_INTEGER "integer"
  #define LABEL_NUMBER "number"
  #define LABEL_VECTOR "vector"
  #define LABEL_VECTOR1 "vector1"
  #define LABEL_VECTOR2 "vector2"
  #define LABEL_VECTOR3 "vector3"
  #define LABEL_VECTOR4 "vector4"
  #define LABEL_QUATERN "quat"
  #define LABEL_MATRIX "matrix"
#endif

/* vector/quaternion floating point type */
#define LUA_VEC_TYPE LUA_FLOAT_FLOAT
typedef float lua_VecF;

/* gritLua vector and quaternion extension; quaternions are {x, y, z, w} */
typedef struct lua_Float4 { lua_VecF x, y, z, w; } lua_Float4;

/*
** @NOTE: LUA_VVECTOR1 is an implicit vector type. Internally this value must
** map to LUA_VNUMFLT.
*/
#define LUA_VVECTOR1 (LUA_TNUMBER | (1 << 4))

/*
** {==================================================================
** LuaGLM C-API
** ===================================================================
*/

/* Returns the name of the type encoded by the vector variant */
LUA_API const char *glm_typename (lua_State *L, int idx);

/* Push a string representing the vector/matrix object on top of the stack. */
LUA_API const char *glm_pushstring (lua_State *L, int idx);

/*
** Unpack an individual vector and place its contents to onto the Lua stack,
** returning the number of elements (i.e., dimensions of vector).
*/
LUA_API int glm_unpack_vector (lua_State *L, int idx);

/*
** Unpack an the individual column-vectors of the given Matrix and place them
** onto the Lua stack, returning the number of elements, dimensions of vector,
** placed on the stack.
*/
LUA_API int glm_unpack_matrix (lua_State *L, int idx);

/*
** Jenkins-hash the object at the provided index. String values are hashed,
** boolean and numeric values are casted to lua_Integer; otherwise, zero is
** returned.
**
** @PARAM ignore_case: A string value is hashed as-is. Otherwise, the lowercase
**  of each string character is computed then hashed.
*/
LUA_API lua_Integer glm_tohash (lua_State *L, int idx, int ignore_case);

/* }================================================================== */

/*
** {==================================================================
** @DEPRECATED gritLua base API
** ===================================================================
*/

#define V_NOTABLE    0x0  /* Only explicit vector types can be parsed */
#define V_PARSETABLE 0x1  /* Attempt to parse table values as vectors. */
#define V_NONUMBER   0x2  /* Ignore lua_Number being the implicit VECTOR1; i.e. the dimensions of the returned vector >= 2. */

/* Number of dimensions associated with the vector object */
LUA_API int luaVec_dimensions (int rtt);

/*
** Returns the variant of the vector at the given index (zero if it is not a
** vector), storing its contents in 'vector' unless it is NULL.
*/
LUA_API int lua_tofloat4 (lua_State *L, int idx, int flags, lua_Float4 *vector);
LUA_API void lua_pushfloat4 (lua_State *L, lua_Float4 f4, int variant);

/*
** Jenkins-hash the object at the provided index. String values are hashed,
** boolean and numeric values are casted to lua_Integer; otherwise, zero is
** returned.
*/
LUA_API lua_Integer lua_ToHash (lua_State *L, int idx, int ignore_case);

/*
** Compatibility API: check if the object at the given index is a vector of a
** specific variant.
*/
#define lua_isvector1(L, I, F) (lua_tofloat4((L), (I), (F), NULL) == LUA_VVECTOR1)
#define lua_isvector2(L, I, F) (lua_tofloat4((L), (I), (F), NULL) == LUA_VVECTOR2)
#define lua_isvector3(L, I, F) (lua_tofloat4((L), (I), (F), NULL) == LUA_VVECTOR3)
#define lua_isvector4(L, I, F) (lua_tofloat4((L), (I), (F), NULL) == LUA_VVECTOR4)
#define lua_isquaternion(L, I, F) (lua_tofloat4((L), (I), V_NOTABLE, NULL) == LUA_VQUAT \ || (((F) & V_PARSETABLE) != 0 && lua_tofloat4((L), (I), (F), NULL) == LUA_VVECTOR4))

/*
** Compatibility API: Populate a Float4 of an explicit type, throwing an error
** otherwise.
*/
#define _lua_checkv(L, I, F, V, T, ERR)          \
  do {                                           \
    if (lua_tofloat4((L), (I), (F), (V)) != (T)) \
      luaL_typeerror((L), (I), (ERR));           \
  } while (0)

#define lua_checkv1(L, I, F, V) _lua_checkv(L, I, F, V, LUA_VVECTOR1, LABEL_VECTOR1)
#define lua_checkv2(L, I, F, V) _lua_checkv(L, I, F, V, LUA_VVECTOR2, LABEL_VECTOR2)
#define lua_checkv3(L, I, F, V) _lua_checkv(L, I, F, V, LUA_VVECTOR3, LABEL_VECTOR3)
#define lua_checkv4(L, I, F, V) _lua_checkv(L, I, F, V, LUA_VVECTOR4, LABEL_VECTOR4)
#define lua_checkquat(L, I, F, V) _lua_checkv(L, I, F, V, LUA_VQUAT, LABEL_QUATERN)

/*
** Compatibility API, replaced by: a single lua_pushfloat4. This macro avoids
** the use of designated initializers for C++ (e.g., MSVC C7555)
*/
#define _lua_pushvector(L, T, X, Y, Z, W)              \
  do {                                                 \
    /* Avoid use of designated initializers for C++ */ \
    lua_Float4 f4 = { (X), (Y), (Z), (W) };            \
    lua_pushfloat4((L), f4, (T));                      \
  } while (0)

#define lua_pushvector2(L, X, Y) _lua_pushvector(L, LUA_VVECTOR2, (X), (Y), cast_vec(0.0), cast_vec(0.0))
#define lua_pushvector3(L, X, Y, Z) _lua_pushvector(L, LUA_VVECTOR3, (X), (Y), (Z), cast_vec(0.0))
#define lua_pushvector4(L, X, Y, Z, W) _lua_pushvector(L, LUA_VVECTOR4, (X), (Y), (Z), (W))
#define lua_pushquaternion(L, W, X, Y, Z) _lua_pushvector(L, LUA_VQUAT, (X), (Y), (Z), (W))

/* }================================================================== */

#endif
