#
# This is a dummy target description file with the sole purpose of building
# all libraries.
#
TARGET = libs

#
# Determine all 'lib/mk' sub directories residing within the repositories.
# Use 'wildcard' to handle the case when a repository does not host any
# 'lib/mk' sub directory.
#
LIB_MK_DIRS := $(wildcard $(addsuffix /lib/mk,$(REPOSITORIES)))

#
# Scan the 'lib/mk' directories of all repositories for library description
# files.
#
ALL_LIB_MK_FILES := $(notdir $(foreach DIR,$(LIB_MK_DIRS),$(shell find $(DIR) -name "*.mk")))

#
# Make the pseudo target depend on all libraries, for which an lib.mk file
# exists. Discard the '.mk' suffix and remove duplicates (via 'sort').
#
LIBS = $(sort $(ALL_LIB_MK_FILES:.mk=))

#
# Among all libraries found above, there may be several libraries with
# unsatisfied build requirements. Normally, the build system won't attempt to
# build the target (and its library dependencies) if one or more libraries
# cannot be built. By enabling 'FORCE_BUILD_LIBS', we let the build system
# visit all non-invalid libraries even in the presence of invalid libraries.
#
FORCE_BUILD_LIBS = yes
