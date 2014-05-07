include ports/fuse-ext2.inc

FUSE_EXT2_SVN_URL = http://svn.code.sf.net/p/fuse-ext2/code/branch/renzo
EXT2_DIR        = e2fsprogs-1.41.12.newgit/

#
# Interface to top-level prepare Makefile
#
PORTS += $(FUSE_EXT2)

prepare-fuse-ext2: $(CONTRIB_DIR)/$(FUSE_EXT2) include/fuse-ext2

#
# Port-specific local rules
#
$(CONTRIB_DIR)/$(FUSE_EXT2):
	$(ECHO) "checking out 'fuse-ext2 rev. $(FUSE_EXT2_REV)' to '$@'"
	$(VERBOSE)svn export $(FUSE_EXT2_SVN_URL)@$(FUSE_EXT2_REV) $@
	$(VERBOSE)for i in src/lib/fuse-ext2/patches/*.patch; do \
		patch -N -p0 < $$i; done || true

#
# Install fuse-ext2 headers
#
#
include/fuse-ext2:
	$(VERBOSE)mkdir -p $@/{e2p,et,ext2fs}
	$(VERBOSE)for j in e2p et ext2fs; do \
		for i in `find $(CONTRIB_DIR)/$(FUSE_EXT2)/$(EXT2_DIR)/$$j -name *.h`; do \
			ln -fs ../../../$$i $@/$$j; \
		done; done

clean-fuse-ext2:
	$(VERBOSE)rm -rf include/fuse-ext2
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(FUSE_EXT2)

