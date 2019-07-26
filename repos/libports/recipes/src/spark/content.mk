ADA_RT_DIR := $(call port_dir,$(GENODE_DIR)/repos/libports/ports/ada-runtime)

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
		a-except.adb \
		i-c.ads \
		i-c.adb \
		system.ads \
		s-arit64.ads \
		s-arit64.adb \
		s-exctab.ads \
		s-exctab.adb \
		s-parame.ads \
		s-parame.adb \
		s-secsta.ads \
		s-secsta.adb \
		s-soflin.ads \
		s-soflin.adb \
		s-stalib.ads \
		s-stalib.adb \
		s-stoele.ads \
		s-stoele.adb \
	) \
	$(addprefix ada-runtime/src/lib/,\
		componolit.ads \
		componolit-runtime.ads \
		componolit-runtime-conversions.ads \
		componolit-runtime-conversions.adb \
		componolit-runtime-debug.ads \
		componolit-runtime-debug.adb \
		componolit-runtime-exceptions.ads \
		componolit-runtime-platform.ads \
		componolit-runtime-platform.adb \
		componolit-runtime-strings.ads \
		componolit-runtime-strings.adb \
		init.c \
	) \
	ada-runtime/src/common/s-init.ads \
	ada-runtime/src/common/s-init.adb \
	ada-runtime/platform/genode/genode.cc \
	ada-runtime/platform/componolit_runtime.h \
	ada-runtime/platform/ada_exceptions.h \
	ada-runtime/platform/gnat_helpers.h

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
