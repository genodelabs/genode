include $(REP_DIR)/ports/lighttpd.inc
LIGHTTPD_DIR = $(REP_DIR)/contrib/$(LIGHTTPD)

FILTER_OUT = lempar.c lighttpd-angel.c lemon.c
SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(LIGHTTPD_DIR)/src/*.c)))

vpath %.c $(LIGHTTPD_DIR)/src

CC_OPT += -DHAVE_SOCKLEN_T -DHAVE_SYSLOG_H -DHAVE_STDINT_H -DUSE_POLL
CC_OPT += -DHAVE_SYS_WAIT_H -DHAVE_SYS_UN_H -DHAVE_MMAP -DHAVE_SELECT
CC_OPT += -DHAVE_WRITEV -DUSE_WRITEV
CC_OPT += -DSBIN_DIR="\"/sbin\""
CC_OPT += -DPACKAGE_NAME="\"lighttpd\""
CC_OPT += -DLIGHTTPD_VERSION_ID='($(LIGHTTPD_MAIN) << 16 | $(LIGHTTPD_MAJOR) << 8 | $(LIGHTTPD_MINOR))'
CC_OPT += -DPACKAGE_VERSION="\"$(LIGHTTPD_MAIN).$(LIGHTTPD_MAJOR).$(LIGHTTPD_MINOR)\""
CC_OPT += -DLIBRARY_DIR="\"/lib\""
CC_OPT += -DLIGHTTPD_STATIC

CC_WARN = -Wall -Wno-unused-variable -Wno-unused-function

INC_DIR += $(PRG_DIR)
INC_DIR += $(LIGHTTPD_DIR)/src

LIBS += cxx env libc libm
LIBS += zlib config_args
