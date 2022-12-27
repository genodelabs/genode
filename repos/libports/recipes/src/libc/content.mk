content: include/libc-plugin src/lib/libc lib/mk LICENSE

LIBC_PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libc)
LIBM_PORT_DIR := $(LIBC_PORT_DIR)

src/lib/libc:
	mkdir -p $@
	cp -r $(LIBC_PORT_DIR)/src/lib/libc/* $@
	cp -r $(REP_DIR)/src/lib/libc/* $@

include/libc-plugin:
	$(mirror_from_rep_dir)

lib/mk:
	mkdir -p $@
	cp $(addprefix $(REP_DIR)/$@/,libc.mk libc-* libm.inc) $@
	for spec in x86_32 x86_64 arm arm_64 riscv; do \
	  mkdir -p $@/spec/$$spec; \
	  cp $(addprefix $(REP_DIR)/$@/spec/$$spec/,libc-* libc.mk libm.mk) $@/spec/$$spec/; done

LICENSE:
	(echo "Based on FreeBSD, which is BSD licensed:"; \
	 echo "  http://www.freebsd.org/copyright/freebsd-license.html"; \
	 echo "Genode-specific adaptations are AGPLv3 licensed:"; \
	 echo "  http://genode.org/about/licenses") > $@

