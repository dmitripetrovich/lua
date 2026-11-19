# Developer's makefile for building Lua
# see luaconf.h for further customization

FEATURES= \
	-DLUA_C99_MATHLIB \
	-DGRIT_COMPAT_IPAIRS \
	-DGRIT_COMPAT_GETN \
	-DGRIT_COMPAT_NUMFMT \
	-DGRIT_COMPAT_GLM \
	-DGRIT_POWER_DEFER_OLD \
	-DGRIT_POWER_COMPOUND \
	-DGRIT_POWER_INTABLE \
	-DGRIT_POWER_TABINIT \
	-DGRIT_POWER_SAFENAV \
	-DGRIT_POWER_CCOMMENT \
	-DGRIT_POWER_IFEXPR \
	-DGRIT_POWER_LAMBDA \
	-DGRIT_POWER_LITERAL \
	-DGRIT_POWER_JOAAT \
	-DGRIT_POWER_EACH \
	-DGRIT_POWER_WOW \
	-DGRIT_POWER_READONLY \
	-DGRIT_POWER_CHRONO \
	-DGRIT_POWER_BLOB \
	-DGRIT_POWER_BUDGET \
	-DGRIT_POWER_GCPACE \
	-DGRIT_POWER_ALLOWANCE \
	-DGRIT_POWER_CONTINUE \
	-DGRIT_POWER_IMPORT \
	-DGRIT_POWER_FASTCALL \
	-DGRIT_POWER_FINDSPLIT \
	-DGRIT_POWER_CHECKHOOK \
	-DGRIT_POWER_COHOOK \
	-DGRIT_POWER_GAMELUA \
	-DGRIT_POWER_UNDUMPHOOK \
	-DGRIT_POWER_READLINE_HISTORY \
	-DGRIT_POWER_LIBS \
	-DGRIT_POWER_INTERP \
	-DGRIT_POWER_STRBUF \
	-DGRIT_POWER_STDEXT \
	-DGRIT_POWER_COALESCE \
	-DGRIT_POWER_DEFAULTS \
	-DGRIT_POWER_SWITCH \
	-DLUA_EXT_ITERATION \
	-DLUA_EXT_ITOA

ALT_FEATURES= $(filter-out -DGRIT_POWER_DEFER_OLD,$(FEATURES)) \
	-DGRIT_POWER_DEFER \
	-DGRIT_POWER_NAMESPACE_SEL \
	-DGRIT_POWER_UHASH \
	-DLUA_NO_DUMP

WARNINGS= -Wall -Wfatal-errors -Wextra -Wshadow -Wundef -Wwrite-strings \
	-Wredundant-decls -Wdisabled-optimization -Wdouble-promotion \
	-Wmissing-declarations -Wconversion -Wdeclaration-after-statement \
	-Wmissing-prototypes -Wnested-externs -Wstrict-prototypes -Wc++-compat \
	-Wold-style-definition
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),clang)
WARNINGS+= -Wlogical-op
endif

ifeq ($(shell uname),Darwin)
SYSTEM= -DLUA_USE_POSIX -DLUA_USE_DLOPEN -DLUA_READLINELIB='"libedit.3.dylib"'
SYSLIBS=
SOFLAGS= -shared -undefined dynamic_lookup
else
SYSTEM= -DLUA_USE_LINUX
SYSLIBS= -Wl,-E -ldl
SOFLAGS= -shared -fPIC
endif

CFLAGS= -std=c99 -O2 $(SYSTEM) $(WARNINGS) -I. -isystem cglm/include -MMD -MP
LDLIBS= -lm $(SYSLIBS)

SRC= lapi lcode lctype ldebug ldo ldump lfunc lgc llex lmem lobject \
	lopcodes lparser lstate lstring ltable ltm lundump lvm lzio ltests lglm \
	lauxlib lbaselib ldblib liolib lmathlib loslib ltablib lstrlib lutf8lib \
	loadlib lcorolib linit \
	libs/json/ljson libs/json/ljsonlib libs/msgpack/lmsgpack \
	libs/msgpack/lmsgpacklib
objs= $(addprefix $(1)/,$(addsuffix .o,$(SRC)))

FLAGS= $(FEATURES)
build/alt/%: FLAGS= $(ALT_FEATURES)
build/tests/%: FLAGS= $(FEATURES) -DLUA_USER_H='"ltests.h"' -Og -g

build: build/lua

export LUA_HISTORY= $(CURDIR)/build/lua_history

TESTLIBS= testes/libs/lib1.so testes/libs/lib11.so testes/libs/lib2.so \
	testes/libs/lib21.so testes/libs/lib2-v2.so

ifeq ($(TEST),)
test: build build/alt/lua build/tests/lua $(TESTLIBS)
	cd testes && ulimit -S -s 1100 && ../build/lua -W -e"_port=true" all.lua
	cd testes && ulimit -S -s 1100 && for f in ext defer lglm readonly json msgpack; do ../build/alt/lua -W $$f.lua || exit 1; done
	cd testes && ulimit -S -s 1100 && ../build/tests/lua -W -e"_port=true" all.lua
else
test: build $(if $(filter defer defer.lua,$(TEST)),build/alt/lua) $(TESTLIBS)
	cd testes && ulimit -S -s 1100 && for f in $(TEST); do f=$${f%.lua}; lua=../build/lua; if [ $$f = defer ]; then lua=../build/alt/lua; fi; $$lua -W $$f.lua || exit 1; done
endif

testes/libs/lib2-v2.so: testes/libs/lib22.c
	$(CC) -std=c99 -Wall -O2 -I. $(SOFLAGS) -o $@ $<

testes/libs/%.so: testes/libs/%.c
	$(CC) -std=c99 -Wall -O2 -I. $(SOFLAGS) -o $@ $<

build/lua: $(call objs,build) build/lua.o
	$(CC) -o $@ $^ $(LDLIBS)

build/alt/lua: $(call objs,build/alt) build/alt/lua.o
	$(CC) -o $@ $^ $(LDLIBS)

build/tests/lua: $(call objs,build/tests) build/tests/lua.o
	$(CC) -o $@ $^ $(LDLIBS)

build/%.o: %.c makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(FLAGS) -c -o $@ $<

build/alt/%.o: %.c makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(FLAGS) -c -o $@ $<

build/tests/%.o: %.c makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(FLAGS) -c -o $@ $<

-include $(shell find build -name '*.d' 2>/dev/null)

.PHONY: build test
