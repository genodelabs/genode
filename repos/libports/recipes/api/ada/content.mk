ADA_RT_DIR := $(call port_dir,$(GENODE_DIR)/repos/libports/ports/ada-runtime)

MIRROR_FROM_ADA_RT_DIR := \
	$(addprefix ada-runtime/contrib/gcc-6.3.0/,\
		ada.ads \
		system.ads \
		s-stoele.ads \
		a-unccon.ads \
		gnat.ads \
		g-io.ads \
	)\
	$(addprefix ada-runtime/src/,\
		s-stalib.ads \
		a-except.ads \
		s-secsta.ads \
		s-parame.ads \
		s-soflin.ads \
	)\
	$(addprefix ada-runtime/src/lib/,\
		ss_utils.ads \
	)

#	$(addprefix ada-runtime/src/,\
		s-stache.ads \
		s-conca2.ads \
		s-arit64.ads \
	)\
	$(addprefix ada-runtime/contrib/gcc-6.3.0/,\
		interfac.ads \
		system.ads \
	)

content: $(MIRROR_FROM_ADA_RT_DIR)

$(MIRROR_FROM_ADA_RT_DIR):
	mkdir -p src/noux-pkg/gcc/gcc/ada/
	cp -r $(ADA_RT_DIR)/$@ src/noux-pkg/gcc/gcc/ada/

MIRROR_FROM_REP_DIR := \
	lib/import/import-ada.mk \
	include/ada \
	lib/symbols/ada

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: lib/mk/ada.mk

lib/mk/ada.mk:
	mkdir -p $(dir $@)
	cp -r $(REP_DIR)/lib/mk/ada.inc $@
