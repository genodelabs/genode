#
# Additional symbols we need to keep when using the arm-none-linux-gnueabi
# tool chain
#
KEEP_SYMBOLS += __aeabi_ldivmod __aeabi_uldivmod __dynamic_cast
KEEP_SYMBOLS += _ZN10__cxxabiv121__vmi_class_type_infoD0Ev

#
# Override sources of the base repository with our changed version
#
vpath exception.cc $(REP_DIR)/src/base/cxx

include $(BASE_DIR)/lib/mk/cxx.mk
