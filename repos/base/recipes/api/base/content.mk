content: include mk/spec lib LICENSE

# architectures, for which a 'trace/timestamp.h' header is available
ARCHS := riscv arm_v6 arm_v7 arm_64 x86_32 x86_64

include:
	mkdir -p include
	cp -r $(REP_DIR)/include/* $@/

LIB_MK_FILES := base.mk ld.mk ldso_so_support.mk

lib:
	mkdir -p lib/mk lib/symbols
	cp $(addprefix $(REP_DIR)/lib/mk/,$(LIB_MK_FILES)) lib/mk/
	cp $(REP_DIR)/lib/symbols/ld lib/symbols/
	sed -i "/KERNEL/d" lib/mk/ld.mk

SPECS := x86_32 x86_64 32bit 64bit

mk/spec:
	mkdir -p $@
	cp $(foreach spec,$(SPECS),$(REP_DIR)/mk/spec/$(spec).mk) $@

content: lib/mk/ldso_so_support.mk src/lib/ldso/so_support.c

lib/mk/ldso_so_support.mk src/lib/ldso/so_support.c:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

