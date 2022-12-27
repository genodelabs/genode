FROM_BASE_LINUX          := etc src/lib/syscall src/lib/lx_hybrid lib/import include
FROM_BASE_LINUX_AND_BASE := src/lib/base src/include

content: $(FROM_BASE_LINUX) $(FROM_BASE_LINUX_AND_BASE) LICENSE

$(FROM_BASE_LINUX):
	mkdir -p $@
	cp -r $(REP_DIR)/$@/* $@

$(FROM_BASE_LINUX_AND_BASE):
	mkdir -p $@
	cp -r $(GENODE_DIR)/repos/base/$@/* $@
	cp -r $(REP_DIR)/$@/* $@

BASE_LIB_MK_CONTENT := \
	$(addprefix lib/mk/,base-common.inc timeout.mk)

content: $(BASE_LIB_MK_CONTENT)

$(BASE_LIB_MK_CONTENT):
	mkdir -p $(dir $@)
	cp $(GENODE_DIR)/repos/base/$@ $@

content: src/lib/timeout

src/lib/timeout:
	mkdir -p $@
	cp -r $(GENODE_DIR)/repos/base/$@/* $@

BASE_LINUX_LIB_MK_CONTENT := \
	$(addprefix lib/mk/,lx_hybrid.mk base-linux.inc base-linux-common.mk) \
	$(foreach S,arm arm_64 x86_32 x86_64,lib/mk/spec/$S/syscall-linux.mk)

content: $(BASE_LINUX_LIB_MK_CONTENT)

$(BASE_LINUX_LIB_MK_CONTENT):
	mkdir -p $(dir $@)
	cp $(REP_DIR)/$@ $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
