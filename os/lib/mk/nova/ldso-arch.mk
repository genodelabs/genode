SRC_CC = parent_cap.cc binary_name.cc
LIBS   = ldso_crt0

vpath parent_cap.cc $(REP_DIR)/src/lib/ldso/arch/nova
vpath binary_name.cc $(REP_DIR)/src/lib/ldso/arch
