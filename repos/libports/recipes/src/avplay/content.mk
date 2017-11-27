content: src/app/avplay lib/import LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libav)

lib/import:
	mkdir -p $@
	cp $(REP_DIR)/$@/import-av.inc $@

src/app/avplay:
	mkdir -p $@
	cp $(PORT_DIR)/src/lib/libav/avplay.c \
	   $(PORT_DIR)/src/lib/libav/cmdutils.* $@
	cp $(REP_DIR)/src/app/avplay/* $@
	cp $(REP_DIR)/src/lib/libav/config.h $@
	rm $@/avplay.patch

LICENSE:
	cp $(PORT_DIR)/src/lib/libav/LICENSE $@
