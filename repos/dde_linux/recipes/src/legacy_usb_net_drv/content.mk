PORT_DIR := $(call port_dir,$(REP_DIR)/ports/dde_linux)

MIRROR_FROM_REP_DIR := src/drivers/legacy_usb_net \
                       src/drivers/nic/linux_network_session_base.cc \
                       src/drivers/nic/linux_network_session_base.h \
                       src/lib/legacy/lx_kit \
                       src/lib/lx_kit/spec \
                       src/include/legacy \
                       lib/import/import-usb_net_include.mk \
                       lib/import/import-usb_arch_include.mk \
                       lib/mk/usb_net_include.mk \
                       $(foreach SPEC, \
                                 arm arm_64 arm_v6 arm_v7 x86 x86_32 x86_64, \
                                 src/include/spec/$(SPEC)) \
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
