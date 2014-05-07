LUA      = lua-5.1.5
LUA_DIR  = $(REP_DIR)/contrib/$(LUA)/src
LIBS    += libc libm
INC_DIR += $(LUA_DIR)
CC_DEF   = -DLUA_ANSI -DLUA_USE_MKSTEMP

# core objects
LUA_CORE_C = lapi.c lcode.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c lmem.c \
             lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c  \
             lundump.c lvm.c lzio.c

# library objects, e.g. string handling
LUA_LIB_C = lauxlib.c lbaselib.c ldblib.c liolib.c lmathlib.c loslib.c ltablib.c \
            lstrlib.c loadlib.c linit.c

vpath %.c $(LUA_DIR)

# vi: set ft=make :
