/*
** $Id: lundump.h $
** load precompiled Lua chunks
** See Copyright Notice in lua.h
*/

#ifndef lundump_h
#define lundump_h

#include <limits.h>

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/* data to catch conversion errors */
#define LUAC_DATA	"\x19\x93\r\n\x1a\n"

#define LUAC_INT	-0x5678
#define LUAC_INST	0x12345678
#define LUAC_NUM	cast_num(-370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define LUAC_VERSION	(LUA_VERSION_MAJOR_N*16+LUA_VERSION_MINOR_N)

/*
** Format byte: 0 is the official format. Each fork feature that adds
** opcodes or chunk sections sets a bit, so a binary chunk only loads in a
** build with the same such features (opcode numbers depend on them).
*/
#if defined(GRIT_POWER_FASTCALL)
#define LUAC_FORMAT_FASTCALL	0x01
#else
#define LUAC_FORMAT_FASTCALL	0
#endif
#if defined(GRIT_POWER_IMPORT)
#define LUAC_FORMAT_IMPORT	0x02
#else
#define LUAC_FORMAT_IMPORT	0
#endif
#if defined(GRIT_POWER_INTERP)
#define LUAC_FORMAT_INTERP	0x04
#else
#define LUAC_FORMAT_INTERP	0
#endif
#if defined(GRIT_POWER_DEFER)
#define LUAC_FORMAT_DEFER	0x08
#else
#define LUAC_FORMAT_DEFER	0
#endif
#define LUAC_FORMAT	(LUAC_FORMAT_FASTCALL | LUAC_FORMAT_IMPORT | LUAC_FORMAT_INTERP | LUAC_FORMAT_DEFER)

#if defined(GRIT_POWER_UNDUMPHOOK)
extern lua_UndumpHook pUndumpHook;
#endif

#if !defined(LUA_NO_BYTECODE)
/* load one chunk; from lundump.c */
LUAI_FUNC LClosure* luaU_undump (lua_State* L, ZIO* Z, Mbuffer *buff,
                                 Table *anchor, const char* name, int fixed);
#endif

/* dump one chunk; from ldump.c */
LUAI_FUNC int luaU_dump (lua_State* L, const Proto* f, lua_Writer w,
                         void* data, int strip);

#endif
