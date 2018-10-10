PORT_DIR := $(call port_dir,$(GENODE_DIR)/repos/ports/ports/gcc)

MIRROR_FROM_PORT_DIR := $(addprefix src/noux-pkg/gcc/gcc/ada/,\
	a-except.ads \
	a-except.adb \
	s-parame.ads \
	s-parame.adb \
	s-stalib.ads \
	s-stalib.adb \
	s-traent.ads \
	s-traent.adb \
	s-soflin.ads \
	s-stache.ads \
	s-stache.adb \
	s-stoele.ads \
	s-stoele.adb \
	s-secsta.ads \
	s-secsta.adb \
	s-conca2.ads \
	s-conca2.adb \
	s-arit64.ads \
	s-arit64.adb \
	ada.ads \
	interfac.ads \
	a-unccon.ads \
	system.ads )

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

MIRROR_FROM_REP_DIR := \
	lib/mk/ada.mk \
	lib/mk/ada.inc \
	lib/import/import-ada.mk \
	include/ada/exception.h

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: src/lib/ada

src/lib/ada:
	mkdir -p $@
	cp -r $(REP_DIR)/$@/* $@/
	echo "LIBS = ada" > $@/target.mk

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/gcc/gcc/COPYING $@
