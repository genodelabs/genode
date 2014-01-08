SRC_CC = parent_cap.cc binary_name.cc
SRC_C  = dummy.c
LIBS   = ldso_crt0 syscall

vpath parent_cap.cc $(REP_DIR)/src/lib/ldso/arch
vpath binary_name.cc $(REP_DIR)/src/lib/ldso/arch
vpath dummy.c $(REP_DIR)/src/lib/ldso/arch/codezero
