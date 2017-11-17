SRC_DIR := src/app/pdf_view
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: src/lib/mupdf/apps/pdfapp.c src/lib/mupdf/apps/pdfapp.h

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/mupdf)

src/lib/mupdf/apps/%: $(PORT_DIR)/src/lib/mupdf/apps/%
	mkdir -p $(dir $@)
	cp -r $< $@
