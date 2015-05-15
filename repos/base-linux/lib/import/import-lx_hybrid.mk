#
# Make Linux headers of the host platform available to the program
#
include $(call select_from_repositories,lib/import/import-syscall.mk)

#
# Manually supply all library search paths of the host compiler to our tool
# chain.
#
HOST_LIB_SEARCH_DIRS = $(shell cc $(CC_MARCH) -print-search-dirs | grep libraries |\
                               sed "s/.*=//"   | sed "s/:/ /g" |\
                               sed "s/\/ / /g" | sed "s/\/\$$//")
#
# Add search path for 'limits.h'
#
# We cannot simply extend 'INC_DIR' because this would give precedence to the
# host include search paths over Genode search path. The variable HOST_INC_DIR
# is appended to the include directory list.
#
HOST_INC_DIR += $(shell echo "int main() {return 0;}" |\
                        LANG=C $(CXX) -x c++ -v -E - 2>&1 |\
                        sed '/^\#include <\.\.\.> search starts here:/,/^End of search list/!d' |\
                        grep "include-fixed")

#
# Add search paths for normal libraries
#
CXX_LINK_OPT += $(addprefix -L,$(HOST_LIB_SEARCH_DIRS))

#
# Determine ldconfig executable
#
# On Ubuntu, /sbin/ is in the PATH variable. Hence we try using the program
# found via 'which'. If 'which' does not return anything (i.e., on Debian),
# try using the expected location '/sbin/'.
#
LDCONFIG := $(firstword $(wildcard $(shell which ldconfig) /sbin/ldconfig))
ifeq ($(LDCONFIG),)
$(error ldconfig is not found)
endif

#
# Add search paths for shared-library lookup
#
# We add all locations of shared libraries present in the ld.cache to our
# library search path.
#
HOST_SO_SEARCH_DIRS := $(sort $(dir $(shell $(LDCONFIG) -p | sed "s/^.* \//\//" | grep "^\/")))
LINK_ARG_PREFIX := -Wl,
CXX_LINK_OPT += $(addprefix $(LINK_ARG_PREFIX)-rpath-link $(LINK_ARG_PREFIX),$(HOST_SO_SEARCH_DIRS))

#
# The '__libc_csu_init' function is normally provided by the C library. We
# override the libc's version in our 'lx_hybrid' library to have a hook for
# Genode-specific initializations. Unfortunately, this way, we get two symbols
# with the same name. So we have to tell the linker to be forgiving. The order
# of the libraries at the linker command line determines which symbol is used.
# Therefore it is important to have 'lx_hybrid.lib.so' listed before '-lc',
# which is always the case when supplying '-lc' via 'EXT_OBJECTS' (not
# 'CXX_LINK_OPT').
#
CXX_LINK_OPT += -Wl,--allow-multiple-definition

#
# Make exceptions work
#
CXX_LINK_OPT += -Wl,--eh-frame-hdr

#
# Add all libraries and their dependencies specified at the 'LX_LIBS'
# variable to the linker command line
#
ifneq ($(LX_LIBS),)
EXT_OBJECTS = $(shell pkg-config --static --libs $(LX_LIBS))
endif

#
# Use the host's startup codes, linker script, and dynamic linker
#
ifneq ($(filter hardening_tool_chain, $(SPECS)),)
EXT_OBJECTS += $(shell cc $(CC_MARCH) -print-file-name=Scrt1.o)
EXT_OBJECTS += $(shell cc $(CC_MARCH) -print-file-name=crti.o)
EXT_OBJECTS += $(shell $(CUSTOM_CC) $(CC_MARCH) -print-file-name=crtbeginS.o)
EXT_OBJECTS += $(shell $(CUSTOM_CC) $(CC_MARCH) -print-file-name=crtendS.o)
else
EXT_OBJECTS += $(shell cc $(CC_MARCH) -print-file-name=crt1.o)
EXT_OBJECTS += $(shell cc $(CC_MARCH) -print-file-name=crti.o)
EXT_OBJECTS += $(shell $(CUSTOM_CC) $(CC_MARCH) -print-file-name=crtbegin.o)
EXT_OBJECTS += $(shell $(CUSTOM_CC) $(CC_MARCH) -print-file-name=crtend.o)
endif
EXT_OBJECTS += $(shell cc $(CC_MARCH) -print-file-name=crtn.o)
EXT_OBJECTS += -lgcc -lgcc_s -lsupc++ -lc
EXT_OBJECTS += -lpthread

USE_HOST_LD_SCRIPT = yes

ifeq (x86_64,$(findstring x86_64,$(SPECS)))
CXX_LINK_OPT += -Wl,--dynamic-linker=/lib64/ld-linux-x86-64.so.2
else
ifeq (arm,$(findstring arm,$(SPECS)))
CXX_LINK_OPT += -Wl,--dynamic-linker=/lib/ld-linux.so.3
else
CXX_LINK_OPT += -Wl,--dynamic-linker=/lib/ld-linux.so.2
endif
endif

# because we use the host compiler's libgcc, omit the Genode toolchain's version
LD_LIBGCC =

# use the host c++ for linking to find shared libraries in DT_RPATH library paths
LD_CMD = c++

# disable format-string security checks, which prevent non-literal format strings
CC_OPT += -Wno-format-security
