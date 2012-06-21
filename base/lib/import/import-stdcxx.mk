#
# Support for using standard C++ headers for Genode programs
#

#
# Add the location of the compiler's C++ headers to search path
#
# We add all header locations that have "c++" or "include-fixed" to the search
# path. The 'c++' subdirectory contains the actual standard C++ headers.
# However, for using them together with Boost, we need to access 'limits.h' as
# provided within the 'include-fixed' location.
#
INC_DIR += $(shell echo "int main() {return 0;}" |\
                   LANG=C $(CXX) -x c++ -v -E - 2>&1 |\
                   sed '/^\#include <\.\.\.> search starts here:/,/^End of search list/!d' |\
                   grep "c++")

#
# Link libstdc++ that comes with the tool chain
#
ifneq ($(filter hardening_tool_chain, $(SPECS)),)
EXT_OBJECTS += $(shell $(CUSTOM_CXX_LIB) $(CC_MARCH) -print-file-name=libstdc++.so)
else
EXT_OBJECTS += $(shell $(CUSTOM_CXX_LIB) $(CC_MARCH) -print-file-name=libstdc++.a)
endif
