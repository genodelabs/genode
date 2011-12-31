MUPDF     := mupdf-0.9
MUPDF_TGZ := mupdf-0.9.tar.gz
MUPDF_URL := http://mupdf.googlecode.com/files/mupdf-0.9-source.tar.gz
MUPDF_DIR := $(CONTRIB_DIR)/$(MUPDF)

#
# Interface to top-level prepare Makefile
#
PORTS += $(MUPDF)

prepare-mupdf: $(CONTRIB_DIR)/$(MUPDF)

$(CONTRIB_DIR)/$(MUPDF): clean-mupdf

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(MUPDF_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) -O $@ $(MUPDF_URL) && touch $@

$(CONTRIB_DIR)/$(MUPDF): $(DOWNLOAD_DIR)/$(MUPDF_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

clean-mupdf: clean-mupdf_contrib
clean-mupdf_contrib:
	$(VERBOSE)rm -rf $(MUPDF_DIR)

#
# Install public headers to 'include/mupdf/'
#
MUPDF_INC_DIR   := include/mupdf
MUPDF_INC_FILES := $(addprefix $(MUPDF_INC_DIR)/,mupdf.h fitz.h muxps.h)

prepare-mupdf: $(MUPDF_INC_FILES)

$(MUPDF_INC_FILES): $(MUPDF_INC_DIR)

$(MUPDF_INC_DIR):
	$(VERBOSE)mkdir -p $@

include/mupdf/mupdf.h:
	$(VERBOSE)ln -s ../../$(MUPDF_DIR)/pdf/mupdf.h $@
include/mupdf/muxps.h:
	$(VERBOSE)ln -s ../../$(MUPDF_DIR)/xps/muxps.h $@
include/mupdf/fitz.h:
	$(VERBOSE)ln -s ../../$(MUPDF_DIR)/fitz/fitz.h $@

clean-mupdf: clean-mupdf_include
clean-mupdf_include:
	$(VERBOSE)rm -rf include/mupdf

#
# Compile tools used to create generated header files
#
MUPDF_FONTDUMP = tool/mupdf/fontdump
MUPDF_CMAPDUMP = tool/mupdf/cmapdump

prepare-mupdf: $(MUPDF_FONTDUMP) $(MUPDF_CMAPDUMP)

$(MUPDF_FONTDUMP) $(MUPDF_CMAPDUMP): $(MUPDF_DIR)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)gcc $(addprefix -I$(MUPDF_DIR)/,fitz pdf) $(MUPDF_DIR)/scripts/$(notdir $@).c -o $@

clean-mupdf: clean-mupdf_tool
clean-mupdf_tool:
	$(VERBOSE)rm -rf tool/mupdf

#
# Create generated (lib-internal) header files
#
MUPDF_GEN_DIR   := $(MUPDF_DIR)/generated
MUPDF_GEN_FILES := $(addprefix $(MUPDF_GEN_DIR)/,cmap_cns.h cmap_gb.h cmap_japan.h cmap_korea.h font_base14.h font_droid.h font_cjk.h)

$(MUPDF_GEN_FILES): $(MUPDF_FONTDUMP) $(MUPDF_CMAPDUMP)
$(MUPDF_GEN_FILES): $(MUPDF_GEN_DIR)

prepare-mupdf: $(MUPDF_GEN_FILES)

$(MUPDF_GEN_DIR):
	$(VERBOSE)mkdir -p $@

MUPDF_ABS_DIR = $(realpath $(MUPDF_DIR))

$(MUPDF_DIR)/generated/cmap_cns.h:
	$(VERBOSE)$(MUPDF_CMAPDUMP) $@ $(MUPDF_ABS_DIR)/cmaps/cns/*
$(MUPDF_DIR)/generated/cmap_gb.h:
	$(VERBOSE)$(MUPDF_CMAPDUMP) $@ $(MUPDF_ABS_DIR)/cmaps/gb/*
$(MUPDF_DIR)/generated/cmap_japan.h:
	$(VERBOSE)$(MUPDF_CMAPDUMP) $@ $(MUPDF_ABS_DIR)/cmaps/japan/*
$(MUPDF_DIR)/generated/cmap_korea.h:
	$(VERBOSE)$(MUPDF_CMAPDUMP) $@ $(MUPDF_ABS_DIR)/cmaps/korea/*
$(MUPDF_DIR)/generated/font_base14.h:
	$(VERBOSE)$(MUPDF_FONTDUMP) $@ $(MUPDF_ABS_DIR)/fonts/*.cff
$(MUPDF_DIR)/generated/font_droid.h:
	$(VERBOSE)$(MUPDF_FONTDUMP) $@ $(addprefix $(MUPDF_ABS_DIR)/fonts/droid/,DroidSans.ttf DroidSansMono.ttf)
$(MUPDF_DIR)/generated/font_cjk.h:
	$(VERBOSE)$(MUPDF_FONTDUMP) $@ $(MUPDF_ABS_DIR)/fonts/droid/DroidSansFallback.ttf

