include ports/lwip.inc

LWIP_URL = git://git.savannah.nongnu.org/lwip.git
LWIP_REV = fe63f36656bd66b4051bdfab93e351a584337d7c
PATCH_URL_WINDOW_SCALING = https://savannah.nongnu.org/patch/download.php?file_id=28026

#
# Interface to top-level prepare Makefile
#
PORTS += $(LWIP)

#
# Check for tools
#
$(call check_tool, git)
$(call check_tool, patch)

prepare-lwip: $(CONTRIB_DIR)/$(LWIP) include/lwip/lwip include/lwip/netif

$(CONTRIB_DIR)/$(LWIP): clean-lwip

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(LWIP)/.git:
	$(VERBOSE)git clone $(LWIP_URL) $(DOWNLOAD_DIR)/$(LWIP) && \
	cd $(DOWNLOAD_DIR)/$(LWIP) && \
	git reset --hard $(LWIP_REV) && \
	wget $(PATCH_URL_WINDOW_SCALING) -O window_scaling.patch && \
	cd ../.. && touch $@


$(CONTRIB_DIR)/$(LWIP)/.git: $(DOWNLOAD_DIR)/$(LWIP)/.git
	$(VERBOSE)git clone $(DOWNLOAD_DIR)/$(LWIP) $(CONTRIB_DIR)/$(LWIP)
	$(VERBOSE)find ./src/lib/lwip/ -name "*.patch" |\
		xargs -ixxx sh -c "patch -p1 -r - -N -d $(CONTRIB_DIR)/$(LWIP) < xxx" || true
	$(VERBOSE)patch -p1 -d $(CONTRIB_DIR)/$(LWIP) < $(DOWNLOAD_DIR)/$(LWIP)/window_scaling.patch

$(CONTRIB_DIR)/$(LWIP): $(CONTRIB_DIR)/$(LWIP)/.git

include/lwip/lwip:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)ln -s $(addprefix ../../../, $(wildcard $(CONTRIB_DIR)/$(LWIP)/src/include/lwip/*.h)) -t $@
	$(VERBOSE)ln -s $(addprefix ../../../, $(wildcard $(CONTRIB_DIR)/$(LWIP)/src/include/ipv4/lwip/*.h)) -t $@
	$(VERBOSE)ln -s $(addprefix ../../../, $(wildcard $(CONTRIB_DIR)/$(LWIP)/src/include/ipv6/lwip/*.h)) -t $@

include/lwip/netif:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)ln -s $(addprefix ../../../, $(wildcard $(CONTRIB_DIR)/$(LWIP)/src/include/netif/*.h)) -t $@

clean-lwip:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LWIP)
	$(VERBOSE)rm -rf include/lwip/lwip
	$(VERBOSE)rm -rf include/lwip/netif
