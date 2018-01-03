RLIB=libcore
include $(REP_DIR)/lib/mk/rust.inc
include $(REP_DIR)/lib/import/import-libcore-rust.mk

#
# Prevent circular dependency
#
LIBS =

CC_CXX_WARN_STRICT =
