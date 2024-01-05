MIRROR_FROM_REP_DIR := lib/import/import-curl.mk \
                       lib/symbols/curl \
                       src/lib/curl

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/curl)

content: include

include:
	mkdir -p $@
	cp -r $(PORT_DIR)/include/curl $@/

content: src/lib/curl

content: LICENSE FindCURL.cmake

LICENSE:
	cp $(PORT_DIR)/src/lib/curl/COPYING $@

FindCURL.cmake:
	echo 'set(CURL_FOUND True)' > $@
