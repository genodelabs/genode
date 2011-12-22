REQUIRES = okl4 x86
LIBS     = cxx lock
SRC_S    = crt0.s
SRC_CC   = _main.cc
INC_DIR += $(BASE_DIR)/src/platform $(REP_DIR)/src/platform

vpath crt0.s   $(dir $(call select_from_repositories,src/platform/x86_32/crt0.s))
vpath _main.cc $(dir $(call select_from_repositories,src/platform/_main.cc))
