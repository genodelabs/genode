LIB_MK := $(addprefix lib/mk/, \
            rump_common.inc \
            rump_fs.mk \
            rump.inc \
            rump_include.inc \
            rump_prefix.inc \
            rump_tools.mk \
            vfs_rump.mk) \
          $(foreach SPEC,x86_32 x86_64 arm arm_64 riscv, \
            lib/mk/spec/$(SPEC)/rump.mk \
            lib/mk/spec/$(SPEC)/rump_include.mk)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/dde_rump)

MIRROR_FROM_REP_DIR := $(LIB_MK) \
                       lib/import/import-rump.mk \
                       src/lib src/include

MIRROR_FROM_PORT_DIR := src/lib/dde_rump/src \
                        src/lib/libc \
                        src/lib/dde_rump_backport

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

content: LICENSE
LICENSE:
	( echo "The NetBSD Foundation's (TNF) license is a “2 clause” Berkeley-style"; \
	  echo "license, which is used for all code contributed to TNF.If you write"; \
	  echo "code and assign the copyright to TNF, this is the license that will"; \
	  echo "be used. For the license of a individual file, please refer to the"; \
	  echo "header information.") > $@
