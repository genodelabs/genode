NETPERF_VERSION := 2.6.0
NETPERF := netperf

#
# Interface to top-level prepare Makefile
#
PORTS += $(NETPERF)-$(NETPERF_VERSION)

#
# Check for tools
#
$(call check_tool,svn)

#
# Subdirectories to check out
#
NETPERF_SVN_BASE = http://www.netperf.org/svn

$(CONTRIB_DIR)/$(NETPERF):
	$(ECHO) "checking out 'netperf' to '$@'"
	$(VERBOSE)svn export $(NETPERF_SVN_BASE)/netperf2/tags/$(NETPERF)-$(NETPERF_VERSION) $@

checkout-netperf: $(CONTRIB_DIR)/$(NETPERF)

apply_patches-netperf: checkout-netperf
	$(VERBOSE)patch -d contrib/netperf -N -p0 < $(CURDIR)/src/app/netperf/timer.patch
	$(VERBOSE)echo '#define NETPERF_VERSION "$(NETPERF_VERSION)"' >$(CONTRIB_DIR)/$(NETPERF)/src/netperf_version.h

prepare:: apply_patches-netperf

clean-netperf:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(NETPERF)
