/*
** Lua core, libraries and interpreter as one translation unit: every
** internal function becomes static, so the compiler can inline across
** files (the VM's hot paths especially). With -DMAKE_LIB it is just the
** library, for embedding; otherwise it is the stand-alone interpreter:
**
** $ cc -O2 -std=c99 -DLUA_USE_LINUX -Wl,-E -I. -isystem cglm/include \
**     -o lua lbundle.c <the .c files of libs/json and libs/msgpack> -lm -ldl
**
** (the json and msgpack libraries are separate translation units).
*/

/* default is to build the full interpreter */
#ifndef MAKE_LIB
#ifndef MAKE_LUA
#define MAKE_LUA
#endif
#endif


/*
** Choose suitable platform-specific features. Default is no
** platform-specific features. Some of these options may need extra
** libraries such as -ldl -lreadline -lncurses
*/
#if 0
#define LUA_USE_LINUX
#define LUA_USE_MACOSX
#define LUA_USE_POSIX
#endif


/*
** Other specific features
*/
#if 0
#define LUA_32BITS
#define LUA_USE_C89
#endif


/* no need to change anything below this line ----------------------------- */

#include "lprefix.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* setup for luaconf.h */
#define LUA_CORE
#define LUA_LIB

#include "luaconf.h"

/* do not export internal symbols */
#undef LUAI_FUNC
#undef LUAI_DDEC
#undef LUAI_DDEF
#define LUAI_FUNC	static
#define LUAI_DDEC(def)	/* empty */
#define LUAI_DDEF	static

/* core -- used by all */
#include "lzio.c"
#include "lctype.c"
#include "lopcodes.c"
#include "lmem.c"
#include "lundump.c"
#include "ldump.c"
#include "lstate.c"
#include "lgc.c"
#include "llex.c"
#include "lcode.c"
#include "lparser.c"
#include "ldebug.c"
#include "lfunc.c"
#include "lobject.c"
#include "ltm.c"
#include "lstring.c"
#include "ltable.c"
#include "ldo.c"
#include "lvm.c"
#include "lapi.c"
#include "lglm.c"

/* auxiliary library -- used by all */
#include "lauxlib.c"

/* standard library */
#include "lbaselib.c"
#include "lcorolib.c"
#include "ldblib.c"
#include "liolib.c"
#include "lmathlib.c"
#include "loadlib.c"
#include "loslib.c"
#include "lstrlib.c"
#include "ltablib.c"
#include "lutf8lib.c"
#include "linit.c"

/* test library -- used only for internal development */
#if defined(LUA_DEBUG)
#include "ltests.c"
#endif

/* lua */
#ifdef MAKE_LUA
#include "lua.c"
#endif
