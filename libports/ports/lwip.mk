include ports/lwip.inc

LWIP_TGZ = $(LWIP).tar.gz
LWIP_URL = http://git.savannah.gnu.org/cgit/lwip.git/snapshot/$(LWIP_TGZ)

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
$(DOWNLOAD_DIR)/$(LWIP_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LWIP_URL) && touch $@

$(CONTRIB_DIR)/$(LWIP): $(DOWNLOAD_DIR)/$(LWIP_TGZ)
	$(VERBOSE)tar xvzf $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)find ./src/lib/lwip/ -name "*.patch" |\
		xargs -ixxx sh -c "patch -p0 -r - -N -d $(CONTRIB_DIR) < xxx" || true

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
