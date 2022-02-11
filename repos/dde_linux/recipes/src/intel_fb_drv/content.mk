LIB_MK := lib/mk/spec/x86/intel_fb_drv.mk \
          lib/mk/spec/x86/intel_fb_include.mk \
          $(foreach SPEC,x86_32 x86_64,lib/mk/spec/$(SPEC)/lx_kit_setjmp.mk)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/dde_linux)

MIRROR_FROM_REP_DIR := $(LIB_MK) \
                       lib/import/import-intel_fb_include.mk \
                       src/include src/lib/legacy/lx_kit \
                       src/lib/lx_kit/spec \
                       $(foreach SPEC,x86 x86_32 x86_64,src/include/spec/$(SPEC)) \
                       $(shell cd $(REP_DIR); find src/drivers/framebuffer/intel -type f)

MIRROR_FROM_PORT_DIR := $(shell cd $(PORT_DIR); find src/drivers/framebuffer/intel -type f | grep -v ".git")
MIRROR_FROM_PORT_DIR := $(filter-out $(MIRROR_FROM_REP_DIR),$(MIRROR_FROM_PORT_DIR))

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp $(PORT_DIR)/$@ $@

content: LICENSE
LICENSE:
	( echo "GNU General Public License version 2, see:"; \
	  echo "https://www.kernel.org/pub/linux/kernel/COPYING" ) > $@
