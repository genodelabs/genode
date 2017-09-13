MIRROR_FROM_REP_DIR := lib/import/import-libc.mk \
                       lib/symbols/libc \
                       lib/symbols/libm \
                       lib/mk/pthread.mk \
                       src/lib/pthread

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libc)

content: include

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/* $@/
	cp -r $(REP_DIR)/include/libc $@/
	cp -r $(REP_DIR)/include/libc-genode $@/

content: LICENSE

LICENSE:
	(echo "Based on FreeBSD, which is BSD licensed:"; \
	 echo "  http://www.freebsd.org/copyright/freebsd-license.html"; \
	 echo "Genode-specific adaptations are AGPLv3 licensed:"; \
	 echo "  http://genode.org/about/licenses") > $@

