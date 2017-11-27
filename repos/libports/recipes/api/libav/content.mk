MIRROR_FROM_REP_DIR := lib/symbols/avcodec \
                       lib/symbols/avfilter \
                       lib/symbols/avformat \
                       lib/symbols/avresample \
                       lib/symbols/avutil \
                       lib/import/import-av.inc \
                       lib/import/import-avcodec.mk \
                       lib/import/import-avfilter.mk \
                       lib/import/import-avformat.mk \
                       lib/import/import-avresample.mk \
                       lib/import/import-avutil.mk \
                       lib/symbols/swscale lib/import/import-swscale.mk

content: $(MIRROR_FROM_REP_DIR) include LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libav)

include:
	mkdir -p $@
	cp -r $(PORT_DIR)/include/libav/* $@/
	cp -r $(REP_DIR)/src/lib/libav/libavutil $@/

LICENSE:
	cp $(PORT_DIR)/src/lib/libav/LICENSE $@
