PORT_DIR := $(call port_dir,$(GENODE_DIR)/repos/ports/ports/gcc)

MIRROR_FROM_REP_DIR := \
	lib/import/import-ada.mk \
	include/ada \
	lib/symbols/ada

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/gcc/gcc/COPYING $@

MIRROR_FROM_PORT_DIR := $(addprefix src/noux-pkg/gcc/gcc/ada/,\
	a-except.ads \
	s-parame.ads \
	s-stalib.ads \
	s-traent.ads \
	s-soflin.ads \
	s-stache.ads \
	s-stoele.ads \
	s-secsta.ads \
	s-conca2.ads \
	s-arit64.ads \
	ada.ads \
	interfac.ads \
	a-unccon.ads \
	system.ads )

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

content: lib/mk/ada.mk

lib/mk/ada.mk:
	mkdir -p $(dir $@)
	cp -r $(REP_DIR)/lib/mk/ada.inc $@