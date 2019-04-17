RUMP_PORT_DIR := $(call select_from_ports,dde_rump)/src/lib/dde_rump
RUMP_BASE     := $(BUILD_BASE_DIR)/var/libcache/rump

CC_C_OPT += -D__NetBSD__ -D'CTASSERT(x)=' -DMAXPHYS=32768 -D_RUMP_NATIVE_ABI \
            -DMAXUSERS=32 -DCOMPAT_50=1 -DCOMPAT_60=1 -DDIAGNOSTIC -DKTRACE \
            -DRUMP_KERNEL_IS_LIBC -D_RUMPKERNEL -D_KERNEL -DMULTIPROCESSOR \
            -D_MODULE -DMODULAR -DRUMP_USE_CTOR

#
# silence contrib code
#
CC_C_OPT += -fno-builtin-printf -fno-builtin-vprintf -fno-builtin-log \
            -fno-builtin-putchar
CC_C_OPT += -Wno-pointer-sign -Wno-unused-but-set-variable \
            -Wno-unused-variable -Wno-format-zero-length \
            -Wno-nonnull-compare -Wno-misleading-indentation \
            -Wno-format -Wno-incompatible-pointer-types

# vi:set ft=make :
