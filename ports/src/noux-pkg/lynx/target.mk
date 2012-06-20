NOUX_CONFIGURE_ARGS = --without-ssl \
                      --disable-nls \
                      --disable-ipv6 \
                      --disable-rpath-hack \
                      --with-cfg-file=/etc/lynx.cfg \
                      --with-lss-file=/etc/lynx.lss

LIBS += ncurses

include $(REP_DIR)/mk/noux.mk
