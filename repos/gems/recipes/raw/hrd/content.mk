content: hrd.tar

include $(GENODE_DIR)/repos/base/recipes/content.inc

hrd.tar:
	rm -rf bin share
	mkdir -p bin share/hrd
	sed "1s/usr.//" $(GENODE_DIR)/tool/hrd > bin/hrd
	chmod 755 bin/hrd
	$(GENODE_DIR)/tool/hrd --help > share/hrd/README
	$(TAR) -cf $@ bin share
	rm -rf bin share
