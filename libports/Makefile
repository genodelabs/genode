#
# \brief  Download and unpack upstream library source codes
# \author Norman Feske
# \date   2009-10-16
#

#
# Print help information by default
#
help::

VERBOSE     ?= @
ECHO         = @echo
DOWNLOAD_DIR = download
CONTRIB_DIR  = contrib
GNU_FIND     = find
SHELL        = bash

#
# Create download and contrib directory so that '<port>.mk' files
# do not need to care for them.
#
prepare: $(DOWNLOAD_DIR) $(CONTRIB_DIR)

#
# Utility to check if a tool is installed
#
check_tool = $(if $(shell which $(1)),,$(error Need to have '$(1)' installed.))

$(call check_tool,wget)
$(call check_tool,patch)

#
# Include information about available ports
#
# Each '<port>.mk' file in the 'ports/' directory extends the following
# variables:
#
# PORTS     - list names of the available ports, e.g., 'freetype-2.3.9'
# GEN_DIRS  - list of automatically generated directories
# GEN_FILES - list of automatically generated files
#
# Furthermore, each '<port>.mk' file extends the 'prepare' rule for
# downloading and unpacking the corresponding upstream sources.
#
PKG ?= $(patsubst ports/%.mk,%,$(wildcard ports/*.mk))
include $(addprefix ports/,$(addsuffix .mk,$(PKG)))

help::
	$(ECHO)
	$(ECHO) "Download and unpack upstream source codes:"
	@for i in $(PORTS); do echo "  $$i"; done
	$(ECHO)
	$(ECHO) "Downloads will be placed into the '$(DOWNLOAD_DIR)/' directory."
	$(ECHO) "Source codes will be unpacked in the '$(CONTRIB_DIR)/' directory."
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare  - download and unpack upstream source codes"
	$(ECHO) "clean    - remove upstream source codes"
	$(ECHO) "cleanall - remove upstream source codes and downloads"
	$(ECHO)
	$(ECHO) "--- available arguments ---"
	$(ECHO) "PKG=<package-list>  - prepare only the specified packages,"
	$(ECHO) "                      each package specified w/o version number"

prepare: $(addprefix prepare-,$(PKG))

$(DOWNLOAD_DIR) $(CONTRIB_DIR):
	$(VERBOSE)mkdir -p $@

clean: $(addprefix clean-,$(PKG))
	$(VERBOSE)if [ -d $(CONTRIB_DIR) ]; then \
                 $(GNU_FIND) $(CONTRIB_DIR) -depth -type d -empty -delete; fi

cleanall: clean
	$(VERBOSE)rm -rf $(DOWNLOAD_DIR)
