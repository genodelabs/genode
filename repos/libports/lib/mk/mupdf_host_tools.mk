#
# Compile host tools used to create generated header files
#

MUPDF_FONTDUMP := $(BUILD_BASE_DIR)/tool/mupdf/fontdump
MUPDF_CMAPDUMP := $(BUILD_BASE_DIR)/tool/mupdf/cmapdump

HOST_TOOLS += $(MUPDF_FONTDUMP) $(MUPDF_CMAPDUMP)

MUPDF_DIR := $(call select_from_ports,mupdf)/src/lib/mupdf

$(MUPDF_FONTDUMP) $(MUPDF_CMAPDUMP): $(MUPDF_DIR)
	$(MSG_BUILD)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)gcc $(addprefix -I$(MUPDF_DIR)/,fitz pdf) $(MUPDF_DIR)/scripts/$(notdir $@).c -o $@

CC_CXX_WARN_STRICT =
