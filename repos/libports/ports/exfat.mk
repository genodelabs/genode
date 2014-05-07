include ports/exfat.inc

EXFAT_TGZ      = $(EXFAT).tar.gz
EXFAT_BASE_URL = https://exfat.googlecode.com/files
EXFAT_URL      = $(EXFAT_BASE_URL)/$(EXFAT_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += exfat

prepare-exfat: $(CONTRIB_DIR)/$(EXFAT) include/exfat

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(EXFAT_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(EXFAT_URL) && touch $@

$(CONTRIB_DIR)/$(EXFAT): $(DOWNLOAD_DIR)/$(EXFAT_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -N -p0 < src/lib/exfat/main.c.patch

#
# Install exfat headers
#

include/exfat:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)for i in `find $(CONTRIB_DIR)/$(EXFAT)/libexfat -name *.h`; do \
		ln -fs ../../$$i $@; done

clean-exfat:
	$(VERBOSE)rm -rf include/exfat
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(EXFAT)
