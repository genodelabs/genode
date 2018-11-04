PORT_DIR := $(call port_dir,$(GENODE_DIR)/repos/libports/ports/python)

MIRROR_FROM_REP_DIR := \
	lib/import/import-python.mk \
	lib/symbols/python

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/python/LICENSE $@

content: include/python

include/python:
	mkdir -p $@
	cp -r $(REP_DIR)/$@/* $@/
	cp -r $(PORT_DIR)/include/* include/
