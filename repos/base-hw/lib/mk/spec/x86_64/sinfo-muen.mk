SRC_CC = sinfo.cc

REP_INC_DIR += include/spec/x86_64/muen

vpath sinfo.cc $(call select_from_repositories,src/lib/muen)
