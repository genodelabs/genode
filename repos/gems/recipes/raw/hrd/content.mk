content: hrd.tar

include $(GENODE_DIR)/repos/base/recipes/content.inc

hrd.tar:
	rm -rf bin
	mkdir -p bin
	sed "1s/usr.//" $(GENODE_DIR)/tool/hrd > bin/hrd
	chmod 755 bin/hrd
	$(TAR) -cf $@ bin
	rm -rf bin
