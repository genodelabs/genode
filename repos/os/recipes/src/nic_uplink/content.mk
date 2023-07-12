MIRROR_FROM_REP_DIR := \
	$(addprefix src/server/nic_router/, \
		communication_buffer.cc \
		communication_buffer.h \
		session_env.h \
		session_creation.h \
		list.h \
	)

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	mkdir -p $(dir $@)
	cp -r $(REP_DIR)/$@ $@

SRC_DIR = src/server/nic_uplink
include $(GENODE_DIR)/repos/base/recipes/src/content.inc
