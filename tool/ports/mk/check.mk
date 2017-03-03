#
# \brief  Check for remote 3rd-party source code
# \author Stefan Kalkowski
# \date   2015-03-04
#
# This makefile must be invoked from the port directory.
#
# Arguments:
#
# PORT - port description file
#

check:

#
# Utility to check if a tool is installed
#
check_tool = $(if $(shell which $(1)),,$(error Need to have '$(1)' installed.))

$(call check_tool,curl)
$(call check_tool,git)
$(call check_tool,svn)

# repository that contains the port description, used to look up patch files
REP_DIR := $(realpath $(dir $(PORT))/..)

#
# Include definitions provided by the port description file
#
include $(PORT)

#
# Include common definitions
#
include $(GENODE_DIR)/tool/ports/mk/common.inc

#
# Default rule that triggers the actual check steps
#
check: $(DOWNLOADS)

#
# Check source codes from a Git repository
#
%.git:
	$(VERBOSE)git ls-remote --exit-code $(URL($*)) >/dev/null 2>&1

#
# Check source codes from Subversion repository
#
%.svn:
	$(VERBOSE)svn info -r $(REV($*)) $(URL($*)) >/dev/null 2>&1

#
# Check plain remote file
#
# We call curl a second time if the first check fails. This gives download
# sites time to reconsider their response and helps, for example, to check the
# qemu-usb port.
#
CURL_CMD = curl -f -L -k -r -2 --max-time 15 --retry 2 $(URL($*)) > /dev/null 2>&1
%.file:
	$(VERBOSE)$(CURL_CMD) || (sleep 1; $(CURL_CMD))

%.archive: %.file
	$(VERBOSE)true
