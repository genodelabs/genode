LIBS = libcore-rust libc libc-stdlib ldso-startup
RLIB = liblibc/src
CC_RUSTC_OPT += --cfg 'target_os = "freebsd"'
include $(REP_DIR)/lib/mk/rust.inc
