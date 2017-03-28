INCLUDE_SUB_DIRS := os util packet_stream_rx packet_stream_tx

MIRRORED_FROM_REP_DIR := $(addprefix include/,$(INCLUDE_SUB_DIRS)) \
                         $(addprefix lib/mk/,alarm.mk timeout.mk) \
                         $(addprefix src/lib/,alarm timeout)

content: $(MIRRORED_FROM_REP_DIR) LICENSE

$(MIRRORED_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
