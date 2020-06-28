MIRROR_FROM_REP_DIR := lib/mk/libqpluginwidget.mk \
                       src/lib/qpluginwidget

content: $(MIRROR_FROM_REP_DIR) LICENSE src/lib/qpluginwidget/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/qpluginwidget/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = libqpluginwidget" > $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
