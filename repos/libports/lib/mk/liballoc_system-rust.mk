LIBS = libcore-rust liblibc-rust
CC_RUSTC_OPT += --allow unused_features
RLIB = liballoc_genode
SRC_RS = lib.rs
vpath % $(REP_DIR)/src/lib/rust/$(RLIB)

CC_CXX_WARN_STRICT =
