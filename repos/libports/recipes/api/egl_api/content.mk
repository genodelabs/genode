content: include LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/egl_api)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/* $@
	cp -r $(REP_DIR)/include/EGL $@

LICENSE:
	grep '\*\*' $(PORT_DIR)/include/EGL/egl.h >$@
