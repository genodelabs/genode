MIRROR_FROM_REP_DIR := lib/import/import-curl.mk \
                       lib/symbols/curl

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/curl)

content: include

include:
	mkdir -p $@/curl
	cp $(PORT_DIR)/include/curl/* $@/curl

content: src/lib/curl

src/lib/curl:
	mkdir -p $@
	for spec in 32bit 64bit; do \
	  mkdir -p $@/spec/$$spec/curl; \
	  cp $(REP_DIR)/$@/spec/$$spec/curl/curlbuild.h $@/spec/$$spec/curl/; \
	done

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/curl/COPYING $@
