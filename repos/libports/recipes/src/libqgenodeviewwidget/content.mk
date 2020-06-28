MIRROR_FROM_REP_DIR := lib/mk/libqgenodeviewwidget.mk \
                       src/lib/qgenodeviewwidget

content: $(MIRROR_FROM_REP_DIR) LICENSE src/lib/qgenodeviewwidget/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/qgenodeviewwidget/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = libqgenodeviewwidget" > $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
