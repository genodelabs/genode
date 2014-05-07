OPENJPEG     = openjpeg-1.4
OPENJPEG_TGZ = openjpeg-1.4.tar.gz
OPENJPEG_URL = http://openjpeg.googlecode.com/files/openjpeg_v1_4_sources_r697.tgz

#
# Interface to top-level prepare Makefile
#
PORTS += $(OPENJPEG)

prepare-openjpeg: $(CONTRIB_DIR)/$(OPENJPEG)

$(CONTRIB_DIR)/$(OPENJPEG): clean-openjpeg include/openjpeg/openjpeg.h

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(OPENJPEG_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) -O $@ $(OPENJPEG_URL) && touch $@

$(CONTRIB_DIR)/$(OPENJPEG): $(DOWNLOAD_DIR)/$(OPENJPEG_TGZ)
	$(VERBOSE)tar xfz $< --transform "s/openjpeg_v1_4_sources_r697/$(OPENJPEG)/" -C $(CONTRIB_DIR) && touch $@

include/openjpeg/openjpeg.h:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -s ../../$(CONTRIB_DIR)/$(OPENJPEG)/libopenjpeg/openjpeg.h $@

clean-openjpeg:
	$(VERBOSE)rm -rf include/openjpeg
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(OPENJPEG)
