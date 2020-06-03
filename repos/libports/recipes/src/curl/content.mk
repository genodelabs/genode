MIRROR_FROM_REP_DIR := \
	lib/mk/curl.inc \
	lib/mk/spec/32bit/curl.mk \
	lib/mk/spec/64bit/curl.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: src/lib/curl/target.mk

src/lib/curl/target.mk: src/lib/curl
	echo "LIBS += curl" > $@

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/curl)

content: src/lib/curl

src/lib/curl:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/curl/lib $@/
	cp -r $(REP_DIR)/src/lib/curl/spec $@/

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/curl/COPYING $@
