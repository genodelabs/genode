LIB_MK := $(addprefix lib/mk/,rump_fs.mk vfs_rump.mk rump.inc rump_base.inc) \
          $(foreach SPEC,x86_32 x86_64 arm,lib/mk/spec/$(SPEC)/rump.mk)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/dde_rump)

MIRROR_FROM_REP_DIR := $(LIB_MK) \
                       lib/import/import-rump.mk \
                       src/ld src/lib src/server/rump_fs \
                       include/rump include/rump_fs \
                       include/util

MIRROR_FROM_PORT_DIR := $(shell cd $(PORT_DIR); find src/lib/dde_rump -maxdepth 1 -type f; \
                          for d in src/lib/dde_rump/brlib src/lib/dde_rump/nblibs \
                                   src/lib/dde_rump/src; do \
                            find $$d  -mindepth 1 -maxdepth 1 -not -name .git; \
                          done)

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

content: LICENSE
LICENSE:
	cp $(PORT_DIR)/src/lib/dde_rump/$@ $@
