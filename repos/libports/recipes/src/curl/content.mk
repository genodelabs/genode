content: src/lib/curl/target.mk lib/mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/curl)

src/lib/curl:
	mkdir -p $@
	cp -r $(REP_DIR)/$@/curl_config.h $@
	cp -r $(PORT_DIR)/src/lib/curl/* $@

src/lib/curl/target.mk: src/lib/curl
	echo "LIBS += curl" > $@

lib/mk:
	mkdir -p $@
	cp $(REP_DIR)/lib/mk/curl.inc $@
	for spec in 32bit 64bit; do \
	  mkdir -p $@/spec/$$spec; \
	  cp $(REP_DIR)/$@/spec/$$spec/curl.mk $@/spec/$$spec/; \
	done

LICENSE:
	cp $(PORT_DIR)/src/lib/curl/COPYING $@
