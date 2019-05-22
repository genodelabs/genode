ADA_RT_DIR := $(call port_dir,$(GENODE_DIR)/repos/libports/ports/ada-runtime)

MIRROR_FROM_ADA_RT_DIR := \
	$(addprefix ada-runtime/contrib/gcc-8.3.0/,\
		ada.ads \
		system.ads \
		interfac.ads \
		s-unstyp.ads \
		s-stoele.ads \
		s-stoele.adb \
		a-unccon.ads \
		s-arit64.ads \
		s-arit64.adb \
		gnat.ads \
		g-io.ads \
		g-io.adb \
		i-cexten.ads \
	) \
	$(addprefix ada-runtime/src/lib/,\
		ada_exceptions.ads \
		ada_exceptions.h \
		argv.c \
		exit.c \
		init.c \
		platform.ads \
		platform.adb \
		ss_utils.ads \
		ss_utils.adb \
		string_utils.ads \
		string_utils.adb \
	) \
	ada-runtime/src/common \
	ada-runtime/src/minimal \
	ada-runtime/platform/genode.cc \
	ada-runtime/platform/unwind.h

content: $(MIRROR_FROM_ADA_RT_DIR)

$(MIRROR_FROM_ADA_RT_DIR):
	mkdir -p $(dir $@)
	cp -r $(ADA_RT_DIR)/$@ $@

MIRROR_FROM_REP_DIR := \
	lib/mk/spark.mk \
	lib/mk/spark.inc \
	lib/import/import-spark.mk \
	include/ada/exception.h

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: src/lib/spark/target.mk

src/lib/spark/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = spark" > $@
