LWIP     = lwip-1.3.2
LWIP_ZIP = $(LWIP).zip
LWIP_URL = http://mirrors.zerg.biz/nongnu/lwip/$(LWIP_ZIP)

#
# Interface to top-level prepare Makefile
#
PORTS += $(LWIP)

#
# Check for tools
#
$(call check_tool,unzip)

prepare-lwip: $(CONTRIB_DIR)/$(LWIP) include/lwip/lwip include/lwip/netif

$(CONTRIB_DIR)/$(LWIP): clean-lwip

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(LWIP_ZIP):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LWIP_URL) && touch $@

$(CONTRIB_DIR)/$(LWIP): $(DOWNLOAD_DIR)/$(LWIP_ZIP)
	$(VERBOSE)unzip $< -d $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -d $(CONTRIB_DIR) -p0 -i ../src/lib/lwip/libc_select_notify.patch
	$(VERBOSE)patch -d $(CONTRIB_DIR) -p0 -i ../src/lib/lwip/errno.patch
	$(VERBOSE)patch -d $(CONTRIB_DIR) -p0 -i ../src/lib/lwip/sol_socket_definition.patch

include/lwip/lwip:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)ln -s $(addprefix ../../../, $(wildcard $(CONTRIB_DIR)/$(LWIP)/src/include/lwip/*.h)) -t $@
	$(VERBOSE)ln -s $(addprefix ../../../, $(wildcard $(CONTRIB_DIR)/$(LWIP)/src/include/ipv4/lwip/*.h)) -t $@

include/lwip/netif:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)ln -s $(addprefix ../../../, $(wildcard $(CONTRIB_DIR)/$(LWIP)/src/include/netif/*.h)) -t $@

clean-lwip:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LWIP)
	$(VERBOSE)rm -rf include/lwip/lwip
	$(VERBOSE)rm -rf include/lwip/netif
