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

checkout-etc: $(CONTRIB_DIR)/$(ETC)

apply_patches-etc: checkout-etc
	$(VERBOSE)find ./src/noux-pkg/etc/patches/ -name "*.patch" |\
		xargs -ixxx sh -c "patch -p0 -r - -N -d $(CONTRIB_DIR)/$(ETC) < xxx" || true

prepare:: apply_patches-etc

clean-etc:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(ETC)
