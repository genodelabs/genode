MIRROR_FROM_REP_DIR := lib/mk/spec/x86_32/libc-setjmp.mk \
                       lib/mk/spec/x86_64/libc-setjmp.mk \
                       lib/mk/spec/arm/libc-setjmp.mk \
                       lib/mk/libc-common.inc \
                       lib/import/import-libc.mk \
                       lib/import/import-libc-setjmp.mk \
                       include/libc-genode/sys/syscall.h

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libc)

MIRROR_FROM_PORT_DIR := $(shell cd $(PORT_DIR); \
                                find -name "*jmp*" -or -name "asm.h" \
                                                   -or -name "cdefs.h"\
                                                   -or -name "SYS.h")

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

content: LICENSE

LICENSE:
	(echo "Based on FreeBSD, which is BSD licensed:"; \
	 echo "  http://www.freebsd.org/copyright/freebsd-license.html") > $@

