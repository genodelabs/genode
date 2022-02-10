SRC_CC = vfs.cc

DDE_LINUX_DIR := $(subst /src/include/lx_kit,,$(call select_from_repositories,src/include/lx_kit))

INC_DIR += $(DDE_LINUX_DIR)/src/include

LIBS := wifi

vpath %.cc $(REP_DIR)/src/lib/vfs/wifi

SHARED_LIB := yes
