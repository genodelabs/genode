ETC := etc-8.2.0

#
# Interface to top-level prepare Makefile
#
PORTS += $(ETC)

#
# Check for tools
#
$(call check_tool,svn)

#
# Subdirectories to check out from FreeBSD's Subversion repository
#
ETC_SVN_BASE = http://svn.freebsd.org/base/release/8.2.0

$(CONTRIB_DIR)/$(ETC):
	$(ECHO) "checking out 'etc' to '$@'"
	$(VERBOSE)svn export $(ETC_SVN_BASE)/etc $@

prepare-etc: $(CONTRIB_DIR)/$(ETC)

clean-etc:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(ETC)
