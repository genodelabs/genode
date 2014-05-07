include ports/dosbox.inc

DOSBOX_SVN_URL = http://svn.code.sf.net/p/dosbox/code-0/dosbox/trunk

#
# Interface to top-level prepare Makefile
#
PORTS += $(DOSBOX)

prepare-dosbox: $(CONTRIB_DIR)/$(DOSBOX)

#
# Port-specific local rules
#
$(CONTRIB_DIR)/$(DOSBOX):
	$(ECHO) "checking out 'dosbox rev. $(DOSBOX_REV)' to '$@'"
	$(VERBOSE)svn export $(DOSBOX_SVN_URL)@$(DOSBOX_REV) $@
	$(VERBOSE)for i in src/app/dosbox/patches/*.patch; do \
		patch -N -p0 < $$i; done || true

clean-dosbox:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(DOSBOX)
