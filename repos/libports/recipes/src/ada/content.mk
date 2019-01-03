ADA_RT_DIR := $(call port_dir,$(GENODE_DIR)/repos/libports/ports/ada-runtime)

MIRROR_FROM_ADA_RT_DIR := \
	$(addprefix ada-runtime/contrib/gcc-6.3.0/,\
		ada.ads \
		system.ads \
		interfac.ads \
		s-unstyp.ads \
		s-stoele.ads \
		s-stoele.adb \
		s-imgint.ads \
		s-imgint.adb \
		a-unccon.ads \
		gnat.ads \
		g-io.ads \
		g-io.adb \
	) \
	ada-runtime/src \
	ada-runtime/platform/genode.cc

content: $(MIRROR_FROM_ADA_RT_DIR)

$(MIRROR_FROM_ADA_RT_DIR):
	mkdir -p $(dir $@)
	cp -r $(ADA_RT_DIR)/$@ $@

MIRROR_FROM_REP_DIR := \
	lib/mk/ada.mk \
	lib/mk/ada.inc \
	lib/import/import-ada.mk \
	include/ada/exception.h

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: src/lib/ada/target.mk

src/lib/ada/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = ada" > $@
