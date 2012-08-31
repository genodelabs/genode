include ports/stdcxx.inc

STDCXX_TBZ2 = $(STDCXX).tar.bz2
STDCXX_URL  = ftp://ftp.fu-berlin.de/gnu/gcc/gcc-$(STDCXX_VERSION)/gcc-g++-$(STDCXX_VERSION).tar.bz2

#
# Interface to top-level prepare Makefile
#
PORTS += $(STDCXX)

STDCXX_GEN_INCLUDES := \
                include/stdcxx-genode/new \
                include/stdcxx-genode/exception \
                include/stdcxx-genode/initializer_list  \
                include/stdcxx-genode/bits/c++locale.h \
                include/stdcxx-genode/bits/hash_bytes.h \
                include/stdcxx-genode/bits/error_constants.h \
                include/stdcxx-genode/bits/cxxabi_forced.h \
                include/stdcxx-genode/bits/basic_file.h \
                include/stdcxx-genode/bits/c++io.h \
                include/stdcxx-genode/bits/atomic_word.h \
                include/stdcxx-genode/bits/ctype_base.h \
                include/stdcxx-genode/bits/ctype_inline.h \
                include/stdcxx-genode/bits/ctype_noninline.h \
                include/stdcxx-genode/bits/time_members.h \
                include/stdcxx-genode/bits/messages_members.h \
                include/stdcxx-genode/bits/exception_ptr.h \
                include/stdcxx-genode/bits/nested_exception.h

prepare-stdcxx: $(CONTRIB_DIR)/$(STDCXX) include/stdcxx $(STDCXX_GEN_INCLUDES)

$(CONTRIB_DIR)/$(STDCXX): clean-stdcxx

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(STDCXX_TBZ2):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) -O$@ $(STDCXX_URL) && touch $@

$(CONTRIB_DIR)/$(STDCXX): $(DOWNLOAD_DIR)/$(STDCXX_TBZ2)
	@#$(VERBOSE)tar xfj $< --transform "s/nova-userland/vancouver/" -C $(CONTRIB_DIR)
	$(VERBOSE)tar xfj $< -C $(CONTRIB_DIR) gcc-$(STDCXX_VERSION)/libstdc++-v3 \
	                     --transform "s/gcc-$(STDCXX_VERSION).libstdc++-v3/stdcxx-$(STDCXX_VERSION)/" && touch $@

include/stdcxx:
	$(VERBOSE)ln -s ../$(CONTRIB_DIR)/$(STDCXX)/include $@
include/stdcxx-genode/new:
	$(VERBOSE)ln -s ../../$(CONTRIB_DIR)/$(STDCXX)/libsupc++/new $@
include/stdcxx-genode/exception:
	$(VERBOSE)ln -s ../../$(CONTRIB_DIR)/$(STDCXX)/libsupc++/exception $@
include/stdcxx-genode/initializer_list:
	$(VERBOSE)ln -s ../../$(CONTRIB_DIR)/$(STDCXX)/libsupc++/initializer_list $@
include/stdcxx-genode/bits/hash_bytes.h:
	$(VERBOSE)ln -s ../../../$(CONTRIB_DIR)/$(STDCXX)/libsupc++/hash_bytes.h $@
include/stdcxx-genode/bits/exception_ptr.h:
	$(VERBOSE)ln -s ../../../$(CONTRIB_DIR)/$(STDCXX)/libsupc++/exception_ptr.h $@
include/stdcxx-genode/bits/nested_exception.h:
	$(VERBOSE)ln -s ../../../$(CONTRIB_DIR)/$(STDCXX)/libsupc++/nested_exception.h $@
include/stdcxx-genode/bits/cxxabi_forced.h:
	$(VERBOSE)ln -s ../../../$(CONTRIB_DIR)/$(STDCXX)/libsupc++/cxxabi_forced.h $@
include/stdcxx-genode/bits/c++locale.h:
	$(VERBOSE)ln -s ../../../$(CONTRIB_DIR)/$(STDCXX)/config/locale/generic/c_locale.h $@
include/stdcxx-genode/bits/error_constants.h:
	$(VERBOSE)ln -s ../../../$(CONTRIB_DIR)/$(STDCXX)/config/os/generic/error_constants.h $@
include/stdcxx-genode/bits/basic_file.h:
	$(VERBOSE)ln -s ../../../$(CONTRIB_DIR)/$(STDCXX)/config/io/basic_file_stdio.h $@
include/stdcxx-genode/bits/c++io.h:
	$(VERBOSE)ln -s ../../../$(CONTRIB_DIR)/$(STDCXX)/config/io/c_io_stdio.h $@
include/stdcxx-genode/bits/atomic_word.h:
	$(VERBOSE)ln -s ../../../$(CONTRIB_DIR)/$(STDCXX)/config/cpu/generic/atomic_word.h $@
include/stdcxx-genode/bits/ctype_base.h:
	$(VERBOSE)ln -s ../../../$(CONTRIB_DIR)/$(STDCXX)/config/os/generic/ctype_base.h $@
include/stdcxx-genode/bits/ctype_inline.h:
	$(VERBOSE)ln -s ../../../$(CONTRIB_DIR)/$(STDCXX)/config/os/generic/ctype_inline.h $@
include/stdcxx-genode/bits/ctype_noninline.h:
	$(VERBOSE)ln -s ../../../$(CONTRIB_DIR)/$(STDCXX)/config/os/generic/ctype_noninline.h $@
include/stdcxx-genode/bits/time_members.h:
	$(VERBOSE)ln -s ../../../$(CONTRIB_DIR)/$(STDCXX)/config/locale/generic/time_members.h $@
include/stdcxx-genode/bits/messages_members.h:
	$(VERBOSE)ln -s ../../../$(CONTRIB_DIR)/$(STDCXX)/config/locale/generic/messages_members.h $@

clean-stdcxx:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(STDCXX)
	$(VERBOSE)rm -f include/stdcxx
	$(VERBOSE)rm -f  $(STDCXX_GEN_INCLUDES)
