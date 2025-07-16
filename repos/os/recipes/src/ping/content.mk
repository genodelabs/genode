MIRROR_FROM_REP_DIR := $(addprefix src/server/nic_router/,ipv4_address_prefix.cc ipv4_address_prefix.h)

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	mkdir -p $(dir $@)
	cp -r $(REP_DIR)/$@ $@

SRC_DIR = src/app/ping
include $(GENODE_DIR)/repos/base/recipes/src/content.inc
