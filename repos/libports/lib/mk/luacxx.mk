#
# Lua library (C++ variant)
#

include $(REP_DIR)/lib/mk/lua.inc

SRC_C = $(LUA_CORE_C) $(LUA_LIB_C)

# force compilation with C++ compiler
CUSTOM_CC = $(CXX)
CC_WARN  += -Wno-sign-compare
