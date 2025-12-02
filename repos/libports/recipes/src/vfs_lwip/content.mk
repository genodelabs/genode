MIRROR_FROM_REP_DIR := lib/mk/vfs_lwip.mk

MIRROR_FROM_OS := src/lib/vfs/ip/vfs.cc \
                  src/lib/vfs/ip/vfs_lwip.cc \
                  src/lib/vfs/ip/vfs_ip.h \
                  src/lib/vfs/ip/sockopt.h \
                  src/lib/vfs/ip/socket_error.h \
                  src/lib/vfs/ip/symbol.map

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_OS) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_OS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
