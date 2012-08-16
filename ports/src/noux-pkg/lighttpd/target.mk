TARGET = lighttpd_noux

include $(REP_DIR)/src/app/lighttpd/target.inc

# search path for 'plugin-static.h'
INC_DIR += $(REP_DIR)/src/app/lighttpd

LIBS += libc_noux
