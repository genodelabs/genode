ADA_RT_DIR  := $(call port_dir,$(GENODE_DIR)/repos/libports/ports/ada-runtime)
ADA_ALI_DIR := $(call port_dir,$(GENODE_DIR)/repos/libports/ports/ada-runtime)/ada-runtime-alis/alis

MIRROR_FROM_ADA_RT_DIR := \
	$(addprefix ada-runtime/contrib/gcc-6.3.0/,\
		ada.ads \
		system.ads \
		s-stoele.ads \
		a-unccon.ads \
		a-comlin.ads \
		gnat.ads \
		g-io.ads \
	)\
	$(addprefix ada-runtime/src/minimal/,\
		s-stalib.ads \
		a-except.ads \
		s-secsta.ads \
		s-parame.ads \
		s-soflin.ads \
	)\
	$(addprefix ada-runtime/src/lib/,\
		ss_utils.ads \
	)\
	$(addprefix ada-runtime/src/common/,\
		a-reatim.ads \
	)

MIRROR_FROM_ADA_ALI_DIR := \
	ada.ali \
	a-reatim.ali \
	s-arit64.ali \
	ada_exceptions.ali \
	a-except.ali \
	g-io.ali \
	gnat.ali \
	interfac.ali \
	platform.ali \
	s-imgint.ali \
	s-parame.ali \
	s-secsta.ali \
	s-soflin.ali \
	s-stalib.ali \
	s-stoele.ali \
	ss_utils.ali \
	string_utils.ali \
	s-unstyp.ali \
	system.ali \
	a-comlin.ali \

content: $(MIRROR_FROM_ADA_RT_DIR) $(MIRROR_FROM_ADA_ALI_DIR)

$(MIRROR_FROM_ADA_RT_DIR):
	mkdir -p include
	cp -a $(ADA_RT_DIR)/$@ include/

$(MIRROR_FROM_ADA_ALI_DIR):
	mkdir -p lib/ali/spark
	cp -a $(ADA_ALI_DIR)/$@ lib/ali/spark/

MIRROR_FROM_REP_DIR := \
	lib/import/import-spark.mk \
	include/ada \
	lib/symbols/spark \
	include/spark/component.h

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: lib/mk/spark.mk

lib/mk/spark.mk:
	mkdir -p $(dir $@)
	cp -a $(REP_DIR)/lib/mk/spark.inc $@
