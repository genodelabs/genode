SRC_CC = lx_hybrid.cc new_delete.cc
LIBS  += syscall env

vpath new_delete.cc $(BASE_DIR)/src/base/cxx
vpath lx_hybrid.cc   $(REP_DIR)/src/platform
