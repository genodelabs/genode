content: hid.tar

include $(GENODE_DIR)/repos/base/recipes/content.inc

hid.tar:
	rm -rf bin share
	mkdir -p bin share/hid
	sed "1s/usr.//" $(GENODE_DIR)/tool/hid > bin/hid
	chmod 755 bin/hid
	$(GENODE_DIR)/tool/hid --help > share/hid/README
	$(TAR) -cf $@ bin share
	rm -rf bin share
