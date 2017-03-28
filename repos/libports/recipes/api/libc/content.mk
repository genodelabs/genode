content: include lib/import/import-libc.mk lib/symbols/libc lib/symbols/libm

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libc)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/* $@/
	cp -r $(REP_DIR)/include/libc $@/
	cp -r $(REP_DIR)/include/libc-genode $@/

lib/import/import-libc.mk lib/symbols/libc lib/symbols/libm:
	$(mirror_from_rep_dir)

content: LICENSE

LICENSE:
	(echo "Based on FreeBSD, which is BSD licensed:"; \
	 echo "  http://www.freebsd.org/copyright/freebsd-license.html"; \
	 echo "Genode-specific adaptations are AGPLv3 licensed:"; \
	 echo "  http://genode.org/about/licenses") > $@

