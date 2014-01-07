include ports/e2fsprogs.inc

#
# Check for tools
#
$(call check_tool,git)

#
# Interface to top-level prepare Makefile
#
PORTS += $(E2FSPROGS)

prepare:: $(CONTRIB_DIR)/$(E2FSPROGS)

#
# Port-specific local rules
#
$(CONTRIB_DIR)/$(E2FSPROGS):
	$(VERBOSE)git clone $(E2FSPROGS_URL) $(CONTRIB_DIR)/$(E2FSPROGS) && \
	cd $(CONTRIB_DIR)/$(E2FSPROGS) && \
	git checkout $(E2FSPROGS_BRANCH)
	$(VERBOSE)for i in src/noux-pkg/e2fsprogs/patches/*.patch; do \
		patch -N -p0 < $$i; done || true
