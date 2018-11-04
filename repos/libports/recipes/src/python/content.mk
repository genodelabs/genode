PORT_DIR = $(call port_dir,$(GENODE_DIR)/repos/libports/ports/python)

content: include/python

include/python:
	mkdir -p $@
	cp -r $(REP_DIR)/$@/* $@/
	cp -r $(PORT_DIR)/include/* include/

content: src/lib/python

src/lib/python:
	mkdir -p $@
	cp -r $(PORT_DIR)/$@/* $@/
	cp -r $(REP_DIR)/$@/* $@/
	echo "LIBS = python" > $@/target.mk

MIRROR_FROM_REP_DIR := \
	lib/import/import-python.mk \
	lib/mk/python.inc \
	lib/mk/spec/x86_32/python.mk \
	lib/mk/spec/x86_64/python.mk \

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/python/LICENSE $@
