INCLUDE_SUB_DIRS := os util packet_stream_rx packet_stream_tx spec/x86_64/os \
                    spec/arm/os spec/x86_32/os spec/arm_64/os

MIRRORED_FROM_REP_DIR := $(addprefix include/,$(INCLUDE_SUB_DIRS))

content: $(MIRRORED_FROM_REP_DIR) LICENSE

$(MIRRORED_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
