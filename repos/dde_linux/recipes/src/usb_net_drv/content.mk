PORT_DIR := $(call port_dir,$(REP_DIR)/ports/dde_linux)

MIRROR_FROM_REP_DIR := src/drivers/usb_net \
                       src/drivers/nic/linux_network_session_base.cc \
                       src/drivers/nic/linux_network_session_base.h \
                       src/lx_kit \
                       src/include \
                       lib/import/import-usb_net_include.mk \
                       lib/import/import-usb_arch_include.mk \
                       lib/mk/usb_net_include.mk \
                       $(foreach SPEC, \
                                 arm arm_64 x86_32 x86_64, \
                                 lib/mk/spec/$(SPEC)/lx_kit_setjmp.mk)

MIRROR_FROM_PORT_DIR := $(shell cd $(PORT_DIR); \
                                find src/drivers/usb_net -type f | \
                                     grep -v ".git")

MIRROR_FROM_PORT_DIR := $(filter-out $(MIRROR_FROM_REP_DIR), \
                                     $(MIRROR_FROM_PORT_DIR))

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
