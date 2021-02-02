MIRROR_FROM_REP_DIR := lib/mk/net.mk include/net src/lib/net

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)
