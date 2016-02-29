LIBS = libcore-rust liblibc-rust
CC_RUSTC_OPT += --allow unused_features
RLIB = liballoc_system
include $(REP_DIR)/lib/mk/rust.inc
