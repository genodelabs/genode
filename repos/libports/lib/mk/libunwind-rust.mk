SRC_RS = lib.rs
LIBS = libcore-rust
vpath % $(REP_DIR)/src/lib/rust/libunwind

CC_CXX_WARN_STRICT =

#
# libunwind contains the public symbol '_Unwind_Resume', which clashes with
# the symbol provided by the cxx library. The aliasing can lead to unexpected
# symbol resolutions by the dynamic linker at runtime, i.e., a C++ exception
# thrown in the libc ending up in the '_Unwind_Resume' code of libunwind-rust.
#
# This rule solves this uncertainty by making the symbol private to the
# libunwind library.
#
ifeq ($(called_from_lib_mk),yes)
$(LIB).lib.a: hide_cxx_symbols
hide_cxx_symbols: lib.o
	$(VERBOSE)cp $< $<.tmp
	$(VERBOSE)$(OBJCOPY) --localize-symbol=_Unwind_Resume $<.tmp $<
endif
