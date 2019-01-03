FROM_BASE_LINUX          := etc src/lib/syscall src/lib/lx_hybrid lib/import
FROM_BASE_LINUX_AND_BASE := lib/mk src/lib/base src/include
FROM_BASE                := src/lib/alarm src/lib/timeout

content: $(FROM_BASE_LINUX) $(FROM_BASE_LINUX_AND_BASE) $(FROM_BASE) LICENSE

$(FROM_BASE_LINUX):
	mkdir -p $@
	cp -r $(REP_DIR)/$@/* $@

$(FROM_BASE_LINUX_AND_BASE):
	mkdir -p $@
	cp -r $(GENODE_DIR)/repos/base/$@/* $@
	cp -r $(REP_DIR)/$@/* $@

$(FROM_BASE):
	mkdir -p $@
	cp -r $(GENODE_DIR)/repos/base/$@/* $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
