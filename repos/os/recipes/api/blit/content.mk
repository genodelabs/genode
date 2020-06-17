MIRROR_FROM_REP_DIR := include/blit \
                       src/lib/blit \
                       lib/mk/blit.mk \
                       lib/mk/spec/arm_64/blit.mk \
                       lib/mk/spec/x86_32/blit.mk \
                       lib/mk/spec/arm/blit.mk \
                       lib/mk/spec/x86_64/blit.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

