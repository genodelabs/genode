#
# Specific driver portions
#

LIB_MK := $(addprefix lib/mk/,pc_wifi.inc) \
          $(foreach SPEC,x86_32 x86_64,lib/mk/spec/$(SPEC)/pc_wifi.mk) \

MIRROR_FROM_REP_DIR := src/lib/pc/lx_emul \
                       src/include \
                       $(LIB_MK) \
                       $(shell cd $(REP_DIR); find src/lib/pc_wifi -type f) \

#
# Generic driver poritions from dde_linux and os
#
#
MIRROR_FROM_OS_DIR  := src/lib/genode_c_api/uplink.cc \
                       src/lib/genode_c_api/mac_address_reporter.cc

DDE_LINUX_REP_DIR  := $(GENODE_DIR)/repos/dde_linux

WS_PORT_DIR    := $(call port_dir,$(DDE_LINUX_REP_DIR)/ports/wpa_supplicant)
LIBNL_PORT_DIR := $(call port_dir,$(DDE_LINUX_REP_DIR)/ports/libnl)

DDE_LINUX_LIB_MK := \
          $(addprefix lib/mk/,libnl.inc libnl_include.mk wifi.inc) \
          $(foreach SPEC,x86_32 x86_64,lib/mk/spec/$(SPEC)/libnl.mk) \
          $(addprefix lib/mk/,wpa_driver_nl80211.mk wpa_supplicant.mk)

MIRROR_FROM_DDE_LINUX_DIR := $(DDE_LINUX_LIB_MK) \
                             src/drivers/wifi \
                             lib/import/import-libnl_include.mk \
                             lib/import/import-libnl.mk \
                             lib/symbols/wifi \
                             include/wifi \
                             $(shell cd $(DDE_LINUX_REP_DIR); find src/lib/libnl -type f) \
                             $(shell cd $(DDE_LINUX_REP_DIR); find src/lib/wifi -type f) \
                             $(shell cd $(DDE_LINUX_REP_DIR); find src/lib/wpa_driver_nl80211 -type f) \
                             $(shell cd $(DDE_LINUX_REP_DIR); find src/lib/wpa_supplicant -type f)

MIRROR_FROM_WS_PORT_DIR    := $(shell cd $(WS_PORT_DIR); find src/app/wpa_supplicant -type f)
MIRROR_FROM_LIBNL_PORT_DIR := $(shell cd $(LIBNL_PORT_DIR); find src/lib/libnl -type f)

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_OS_DIR) $(MIRROR_FROM_DDE_LINUX_DIR) \
         $(MIRROR_FROM_WS_PORT_DIR) $(MIRROR_FROM_LIBNL_PORT_DIR) cleanup-wpa

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_OS_DIR):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $@

$(MIRROR_FROM_DDE_LINUX_DIR):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/dde_linux/$@ $@

$(MIRROR_FROM_WS_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(WS_PORT_DIR)/$@ $@

$(MIRROR_FROM_LIBNL_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(LIBNL_PORT_DIR)/$@ $@

cleanup-wpa: $(MIRROR_FROM_WS_PORT_DIR)
	@for dir in hs20; do \
		rm -rf src/app/wpa_supplicant/$$dir; done

content: LICENSE
LICENSE:
	( echo "Linux is subject to GNU General Public License version 2, see:"; \
	  echo "https://www.kernel.org/pub/linux/kernel/COPYING"; \
	  echo; \
	  echo "Libnl is subject to GNU LESSER GENERAL PUBLIC LICENSE Verson 2.1, see:"; \
	  echo "  src/lib/libnl/COPYING"; \
	  echo; \
	  echo "Wpa_supplicant is subject to 3-clause BSD license, see:"; \
	  echo "   src/app/wpa_supplicant/COPYING"; ) > $@
