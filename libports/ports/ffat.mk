FFAT     = ff007e
FFAT_ZIP = $(FFAT).zip

#
# Download archive from genode.org instead of the original location
# 'http://elm-chan.org/fsw/ff/ff007e.zip' because the elm-chan webserver
# does not like wget.
#
FFAT_URL = http://genode.org/files/$(FFAT_ZIP)

#
# Interface to top-level prepare Makefile
#
PORTS += ffat-0.07e

#
# Check for tools
#
$(call check_tool,unzip)

prepare-ffat: $(CONTRIB_DIR)/$(FFAT)

$(CONTRIB_DIR)/$(FFAT): clean-ffat

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(FFAT_ZIP):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(FFAT_URL) && touch $@

FFAT_HEADERS := ff.h diskio.h integer.h ffconf.h
FFAT_DELETE  := diskio.c
FFAT_PATCH   := config.patch

include/ffat:
	$(VERBOSE)mkdir -p $@

$(CONTRIB_DIR)/$(FFAT): $(DOWNLOAD_DIR)/$(FFAT_ZIP) include/ffat
	$(VERBOSE)unzip $< -d $(CONTRIB_DIR)/$(FFAT) && touch $@
	@# create symbolic links for public headers from contrib dir
	$(VERBOSE)for i in $(FFAT_HEADERS); do \
	  ln -sf ../../$(CONTRIB_DIR)/$(FFAT)/src/$$i include/ffat/; done
	$(VERBOSE)rm $(addprefix $(CONTRIB_DIR)/$(FFAT)/src/,$(FFAT_DELETE))
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(FFAT) -p1 -i ../../src/lib/ffat/config.patch

clean-ffat:
	$(VERBOSE)rm -f  $(addprefix include/ffat/,$(FFAT_HEADERS))
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(FFAT)
