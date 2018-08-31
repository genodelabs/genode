LIB_MK := $(addprefix lib/mk/,libnl.inc libnl_include.mk iwl_firmware.mk wifi.inc \
                              wifi_include.mk) \
          $(foreach SPEC,x86_32 x86_64,lib/mk/spec/$(SPEC)/libnl.mk) \
          $(foreach SPEC,x86_32 x86_64,lib/mk/spec/$(SPEC)/lx_kit_setjmp.mk) \
          $(foreach SPEC,x86_32 x86_64,lib/mk/spec/$(SPEC)/wifi.mk) \
          $(addprefix lib/mk/spec/x86/,wpa_driver_nl80211.mk wpa_supplicant.mk)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/dde_linux)

MIRROR_FROM_REP_DIR := $(LIB_MK) \
                       lib/import/import-wifi_include.mk \
                       lib/import/import-libnl_include.mk \
                       lib/import/import-libnl.mk \
                       include/wifi src/include src/lx_kit \
                       $(shell cd $(REP_DIR); find src/drivers/wifi -type f) \
                       $(shell cd $(REP_DIR); find src/lib/libnl -type f) \
                       $(shell cd $(REP_DIR); find src/lib/wifi -type f) \
                       $(shell cd $(REP_DIR); find src/lib/wpa_driver_nl80211 -type f) \
                       $(shell cd $(REP_DIR); find src/lib/wpa_supplicant -type f)

MIRROR_FROM_PORT_DIR := $(shell cd $(PORT_DIR); find src/lib/libnl -type f) \
                        $(shell cd $(PORT_DIR); find src/lib/wifi -type f) \
                        $(shell cd $(PORT_DIR); find src/app/wpa_supplicant -type f)
MIRROR_FROM_PORT_DIR := $(filter-out $(MIRROR_FROM_REP_DIR),$(MIRROR_FROM_PORT_DIR))

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_PORT_DIR) cleanup-wpa

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

cleanup-wpa: $(MIRROR_FROM_PORT_DIR)
	@for dir in .git doc eap_example hs20 mac80211_hwsim radius_example \
		hostapd tests wlantest wpadebug wpaspy; do \
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
