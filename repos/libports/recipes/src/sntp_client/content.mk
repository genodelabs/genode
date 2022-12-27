SRC_DIR = src/app/sntp_client
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: src/lib/musl_tm

src/lib/musl_tm:
	mkdir -p src/lib
	cp -r $(GENODE_DIR)/repos/libports/$@ $@
