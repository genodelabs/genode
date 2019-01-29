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
	lib/mk/spark.mk \
	lib/mk/spark.inc \
	lib/import/import-spark.mk \
	include/spark/exception.h

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: src/lib/spark/target.mk

src/lib/spark/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = spark" > $@
