LIBS = libcore-rust libc ldso-startup
RLIB = liblibc/src
CC_RUSTC_OPT += --cfg 'target_os = "netbsd"'
include $(REP_DIR)/lib/mk/rust.inc
