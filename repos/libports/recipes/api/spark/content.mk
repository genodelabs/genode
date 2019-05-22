ADA_RT_DIR  := $(call port_dir,$(GENODE_DIR)/repos/libports/ports/ada-runtime)
ADA_ALI_DIR := $(call port_dir,$(GENODE_DIR)/repos/libports/ports/ada-runtime)/ada-runtime-alis/alis

MIRROR_FROM_ADA_RT_DIR := \
	$(addprefix ada-runtime/contrib/gcc-8.3.0/,\
		ada.ads \
		system.ads \
		s-stoele.ads \
		a-unccon.ads \
		gnat.ads \
		g-io.ads \
		interfac.ads \
		i-cexten.ads \
		s-arit64.ads \
		s-unstyp.ads \
	) \
	$(addprefix ada-runtime/src/minimal/,\
		s-stalib.ads \
		a-except.ads \
		s-secsta.ads \
		s-parame.ads \
		s-soflin.ads \
		s-exctab.ads \
		i-c.ads \
	) \
	$(addprefix ada-runtime/src/lib/,\
		ss_utils.ads \
		string_utils.ads \
		platform.ads \
		ada_exceptions.ads \
		ada_exceptions.h \
	)

MIRROR_FROM_ADA_ALI_DIR := \
	ada.ali \
	ada_exceptions.ali \
	a-except.ali \
	g-io.ali \
	gnat.ali \
	interfac.ali \
	i-c.ali \
	i-cexten.ali \
	platform.ali \
	s-arit64.ali \
	s-init.ali \
	s-parame.ali \
	s-secsta.ali \
	s-soflin.ali \
	s-stalib.ali \
	s-stoele.ali \
	s-unstyp.ali \
	ss_utils.ali \
	string_utils.ali \
	system.ali \

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

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: lib/mk/spark.mk

lib/mk/spark.mk:
	mkdir -p $(dir $@)
	cp -a $(REP_DIR)/lib/mk/spark.inc $@
