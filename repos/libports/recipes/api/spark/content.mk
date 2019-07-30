ADA_RT_DIR  := $(call port_dir,$(GENODE_DIR)/repos/libports/ports/ada-runtime)
ADA_ALI_DIR := $(call port_dir,$(GENODE_DIR)/repos/libports/ports/ada-runtime)/ada-runtime-alis/alis

MIRROR_FROM_ADA_RT_DIR := \
	$(addprefix ada-runtime/contrib/gcc-8.3.0/,\
		ada.ads \
		a-unccon.ads \
		interfac.ads \
		i-cexten.ads \
		s-maccod.ads \
		s-unstyp.ads \
	) \
	$(addprefix ada-runtime/src/minimal/,\
		a-except.ads \
		i-c.ads \
		system.ads \
		s-arit64.ads \
		s-exctab.ads \
		s-parame.ads \
		s-secsta.ads \
		s-soflin.ads \
		s-stalib.ads \
		s-stoele.ads \
	) \
	$(addprefix ada-runtime/src/lib/,\
		componolit.ads \
		componolit-runtime.ads \
		componolit-runtime-conversions.ads \
		componolit-runtime-debug.ads \
		componolit-runtime-exceptions.ads \
		componolit-runtime-platform.ads \
		componolit-runtime-strings.ads \
	) \
	ada-runtime/src/common/s-init.ads

MIRROR_FROM_ADA_ALI_DIR := \
    	a-except.ali \
	ada.ali \
	componolit-runtime-conversions.ali \
	componolit-runtime-debug.ali \
	componolit-runtime-exceptions.ali \
	componolit-runtime-platform.ali \
	componolit-runtime-strings.ali \
	componolit-runtime.ali \
	componolit.ali \
	i-c.ali \
	i-cexten.ali \
	interfac.ali \
	s-arit64.ali \
	s-init.ali \
	s-parame.ali \
	s-secsta.ali \
	s-soflin.ali \
	s-stalib.ali \
	s-stoele.ali \
	s-unstyp.ali \
	system.ali

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
