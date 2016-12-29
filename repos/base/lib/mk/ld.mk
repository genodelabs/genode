#
# Generic ld.lib.so ABI stub library
#
# This library is used to build kernel-independent dynamically linked
# executables. It does not contain any code or data but only the symbol
# information of the binary interface of the Genode API.
#
# Note that this library is not used as runtime at all. At system-integration
# time, it is transparently replaced by the variant of the dynamic linker that
# matches the used kernel.
#

SHARED_LIB := yes

LD_OPT += -T$(BASE_DIR)/src/lib/ldso/linker.ld
