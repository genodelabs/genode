REP_INC_DIR += include/stdcxx
REP_INC_DIR += include/stdcxx/std
REP_INC_DIR += include/stdcxx/c_std
REP_INC_DIR += include/stdcxx/c_global
REP_INC_DIR += include/stdcxx-genode

LIBS += libc
include $(call select_from_repositories,lib/import/import-libc.mk)

# prevent gcc headers from defining mbstate
CC_OPT += -D_GLIBCXX_HAVE_MBSTATE_T

# use compiler-builtin atomic operations
CC_OPT += -D_GLIBCXX_ATOMIC_BUILTINS_4
