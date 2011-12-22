LIBS     = cxx lock l4
SRC_S    = crt0.s
SRC_CC   = _main.cc
INC_DIR += $(REP_DIR)/src/platform
INC_DIR += $(BASE_DIR)/src/platform
INC_DIR += $(REP_DIR)/include/codezero/dummies

vpath crt0.s   $(BASE_DIR)/src/platform/arm
vpath _main.cc $(dir $(call select_from_repositories,src/platform/_main.cc))
