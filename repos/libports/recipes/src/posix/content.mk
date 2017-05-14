MIRROR_FROM_REP_DIR := lib/mk/posix.mk src/lib/posix

content: $(MIRROR_FROM_REP_DIR) LICENSE src/lib/posix/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/posix/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = posix" > $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

