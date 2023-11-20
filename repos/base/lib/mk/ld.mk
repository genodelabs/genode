#
# Generic ld.lib.so ABI stub library
#
# The ABI of this library is used to build kernel-independent dynamically
# linked executables. The library does not contain any code or data but only
# the symbol information of the binary interface of the Genode API.
#
# Note that this library is not used as runtime at all. At system-integration
# time, it is transparently replaced by the variant of the dynamic linker that
# matches the used kernel.
#
# By adding a LIB dependency from the kernel-specific dynamic linker, we
# let dep_lib.mk generate the rule for ld-<KERNEL>.so into the var/libdeps
# file. The build of the ld-<KERNEL>.so is triggered because the top-level
# Makefile manually adds the dependency 'ld.so: ld-<KERNEL>.so' to the
# var/libdeps file for the currently selected kernel.
#
LIBS += $(addprefix ld-,$(KERNEL))

# as the stub libarary is not used at runtime, disregard it as build artifact
BUILD_ARTIFACTS :=
