REQUIRES = nova x86
LIBS     = cxx lock
SRC_S    = crt0.s
SRC_CC   = _main.cc
INC_DIR += $(REP_DIR)/src/platform

vpath crt0.s   $(BASE_DIR)/src/platform/x86_32
vpath _main.cc $(dir $(call select_from_repositories,src/platform/_main.cc))
