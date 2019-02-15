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
	$(addprefix ada-runtime/src/minimal/,\
		s-stalib.ads \
		a-except.ads \
		s-secsta.ads \
		s-parame.ads \
		s-soflin.ads \
	)\
	$(addprefix ada-runtime/src/lib/,\
		ss_utils.ads \
	)

content: $(MIRROR_FROM_ADA_RT_DIR)

$(MIRROR_FROM_ADA_RT_DIR):
	mkdir -p include
	cp $(ADA_RT_DIR)/$@ include/

MIRROR_FROM_REP_DIR := \
	lib/import/import-spark.mk \
	include/ada \
	lib/symbols/spark \
	lib/ali/spark

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: lib/mk/spark.mk

lib/mk/spark.mk:
	mkdir -p $(dir $@)
	cp -r $(REP_DIR)/lib/mk/spark.inc $@
