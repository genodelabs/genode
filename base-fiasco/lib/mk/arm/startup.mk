REQUIRES = fiasco arm
LIBS     = cxx lock
SRC_S    = crt0.s
SRC_CC   = _main.cc
INC_DIR += $(REP_DIR)/src/platform

vpath crt0.s   $(REP_DIR)/src/platform/arm
vpath _main.cc $(dir $(call select_from_repositories,src/platform/_main.cc))
