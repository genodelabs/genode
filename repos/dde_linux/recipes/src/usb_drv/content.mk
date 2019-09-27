LIB_MK := lib/import/import-usb_include.mk lib/mk/usb_include.mk lib/mk/rpi_usb.mk \
          lib/import/import-usb_arch_include.mk \
          $(foreach SPEC,x86_32 x86_64 arm,lib/mk/spec/$(SPEC)/lx_kit_setjmp.mk)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/dde_linux)

MIRROR_FROM_REP_DIR := $(LIB_MK) \
                       src/drivers/usb \
                       src/include src/lx_kit \
                       $(shell cd $(REP_DIR); find src/lib/rpi_usb -type f)

MIRROR_FROM_PORT_DIR := $(shell cd $(PORT_DIR); find src/lib/usb -type f | grep -v ".git")
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
