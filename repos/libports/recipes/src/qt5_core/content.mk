MIRROR_FROM_REP_DIR := lib/mk/qt5_core.mk \
                       lib/mk/qt5_core_generated.inc \
                       lib/mk/qt5.inc \
                       src/lib/qt5/qtbase/src/corelib \
                       src/lib/qt5/libc_dummies.cc \
                       include/libc-plugin \
                       lib/mk/libc_pipe.mk \
                       src/lib/libc_pipe

content: $(MIRROR_FROM_REP_DIR) src/lib/qt5_core/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/qt5_core/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_core" > $@

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

MIRROR_FROM_PORT_DIR := src/lib/qt5/qt5/qtbase/src/3rdparty/double-conversion \
                        src/lib/qt5/qt5/qtbase/src/3rdparty/easing \
                        src/lib/qt5/qt5/qtbase/src/3rdparty/freebsd \
                        src/lib/qt5/qt5/qtbase/src/3rdparty/harfbuzz \
                        src/lib/qt5/qt5/qtbase/src/3rdparty/md4 \
                        src/lib/qt5/qt5/qtbase/src/3rdparty/md5 \
                        src/lib/qt5/qt5/qtbase/src/3rdparty/pcre \
                        src/lib/qt5/qt5/qtbase/src/3rdparty/rfc6234 \
                        src/lib/qt5/qt5/qtbase/src/3rdparty/sha1 \
                        src/lib/qt5/qt5/qtbase/src/3rdparty/sha3 \
                        src/lib/qt5/qt5/qtbase/src/corelib

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/qt5/qt5/LICENSE.LGPLv3 $@
