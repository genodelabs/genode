MIRROR_FROM_OS := \
	src/server/nic_router/ipv4_address_prefix.cc \
	src/server/nic_router/ipv4_address_prefix.h

content: $(MIRROR_FROM_OS)

$(MIRROR_FROM_OS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $@

content: src/lib/musl_tm

src/lib/musl_tm:
	mkdir -p src/lib
	cp -r $(GENODE_DIR)/repos/libports/$@ $@

SRC_DIR = src/app/sntp_client
include $(GENODE_DIR)/repos/base/recipes/src/content.inc
