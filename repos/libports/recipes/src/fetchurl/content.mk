MIRROR_FROM_REP_DIR := src/app/fetchurl

#
# Copy of lwIP ingredients
#
MIRROR_FROM_REP_DIR += include/libc-plugin \
                       src/lib/libc_lwip \
                       src/lib/libc_lwip_nic_dhcp \
                       lib/mk/lwip.mk \
                       lib/mk/libc_lwip.mk \
                       lib/mk/libc_lwip_nic_dhcp.mk \
                       lib/import/import-lwip.mk


LWIP_PORT_DIR := $(call port_dir,$(REP_DIR)/ports/lwip)

content: $(MIRROR_FROM_REP_DIR) LICENSE include/lwip src/lib/lwip

include/lwip:
	mkdir -p $@
	cp -r $(LWIP_PORT_DIR)/include/lwip/* $@
	cp -r $(REP_DIR)/include/lwip/* $@

src/lib/lwip:
	mkdir -p $@
	cp -r $(LWIP_PORT_DIR)/src/lib/lwip/* $@
	cp -r $(REP_DIR)/src/lib/lwip/* $@

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

MIRROR_FROM_OS := lib/mk/timed_semaphore.mk \
                  include/os/timed_semaphore.h \
                  src/lib/timed_semaphore

content: $(MIRROR_FROM_OS)

$(MIRROR_FROM_OS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $(dir $@)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@


