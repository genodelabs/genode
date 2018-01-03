MUPDF_DIR := $(call select_from_ports,mupdf)/src/lib/mupdf
LIBS      += libc jpeg zlib jbig2dec openjpeg freetype mupdf_host_tools
INC_DIR   += $(addprefix $(MUPDF_DIR)/,fitz pdf xps)

SRC_C  = $(addprefix fitz/,$(notdir $(wildcard $(MUPDF_DIR)/fitz/*.c)))
SRC_C += $(addprefix pdf/, $(notdir $(wildcard $(MUPDF_DIR)/pdf/*.c)))
SRC_C += $(addprefix xps/, $(notdir $(wildcard $(MUPDF_DIR)/xps/*.c)))
SRC_C += $(addprefix draw/,$(notdir $(wildcard $(MUPDF_DIR)/draw/*.c)))

# disable warning noise for contrib code
CC_WARN += -Wall -Wno-uninitialized -Wno-unused-but-set-variable

vpath %.c $(MUPDF_DIR)

SHARED_LIB = yes

MUPDF_GEN_FILES := $(addprefix generated/,cmap_cns.h cmap_gb.h cmap_japan.h cmap_korea.h font_base14.h font_droid.h font_cjk.h)

$(SRC_C:.c=.o): $(MUPDF_GEN_FILES)

MUPDF_FONTDUMP := $(BUILD_BASE_DIR)/tool/mupdf/fontdump
MUPDF_CMAPDUMP := $(BUILD_BASE_DIR)/tool/mupdf/cmapdump

define do_cmap_dump
	$(MSG_CONVERT)$@
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)$(MUPDF_CMAPDUMP) $@ $1 > /dev/null 2> /dev/null
endef

define do_font_dump
	$(MSG_CONVERT)$@
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)$(MUPDF_FONTDUMP) $@ $1 > /dev/null 2> /dev/null
endef

generated/cmap_cns.h:
	$(call do_cmap_dump,$(MUPDF_DIR)/cmaps/cns/*)
generated/cmap_gb.h:
	$(call do_cmap_dump,$(MUPDF_DIR)/cmaps/gb/*)
generated/cmap_japan.h:
	$(call do_cmap_dump,$(MUPDF_DIR)/cmaps/japan/*)
generated/cmap_korea.h:
	$(call do_cmap_dump,$(MUPDF_DIR)/cmaps/korea/*)
generated/font_base14.h:
	$(call do_font_dump,$(MUPDF_DIR)/fonts/*.cff)
generated/font_droid.h:
	$(call do_font_dump,$(addprefix $(MUPDF_DIR)/fonts/droid/,DroidSans.ttf DroidSansMono.ttf))
generated/font_cjk.h:
	$(call do_font_dump,$(MUPDF_DIR)/fonts/droid/DroidSansFallback.ttf)

CC_CXX_WARN_STRICT =
