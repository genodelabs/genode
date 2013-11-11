include ports/ntfs-3g.inc

NTFS_3G_TGZ      = $(NTFS_3G).tgz
NTFS_3G_BASE_URL = http://tuxera.com/opensource
NTFS_3G_URL      = $(NTFS_3G_BASE_URL)/$(NTFS_3G_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += ntfs-3g

prepare-ntfs-3g: $(CONTRIB_DIR)/$(NTFS_3G) include/ntfs-3g

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(NTFS_3G_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(NTFS_3G_URL) && touch $@

$(CONTRIB_DIR)/$(NTFS_3G): $(DOWNLOAD_DIR)/$(NTFS_3G_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)for i in src/lib/ntfs-3g/*.patch; do \
		patch -N -p0 < $$i; done || true

#
# Install ntfs-3g headers
#

include/ntfs-3g:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)for i in `find $(CONTRIB_DIR)/$(NTFS_3G)/include/ntfs-3g -name *.h`; do \
		ln -fs ../../$$i $@; done

clean-ntfs-3g:
	$(VERBOSE)rm -rf include/ntfs-3g
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(NTFS_3G)
