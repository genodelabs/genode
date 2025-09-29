content: hrd.tar

hrd.tar:
	rm -rf bin
	mkdir -p bin
	sed "1s/usr.//" $(GENODE_DIR)/tool/hrd > bin/hrd
	chmod 755 bin/hrd
	tar --owner=0 --group=0 --numeric-owner --mode='go=' --mtime='2025-09-29 00:00Z' -cf $@ bin
	rm -rf bin
