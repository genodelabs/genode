TARGET   = verify
SRC_CC   = main.cc
LIBS     = base libc libgcrypt

GNUPG_SRC_DIR := $(call select_from_ports,gnupg)/src/app/gnupg/g10

INC_DIR += $(PRG_DIR) $(GNUPG_SRC_DIR)

SRC_C := gnupg.c dummies.c

# source codes from GnuPG
SRC_C += verify.c armor.c iobuf.c stringhelp.c progress.c strlist.c \
         cpr.c status.c mainproc.c sig-check.c keyid.c kbnode.c parse-packet.c \
         misc.c logging.c compliance.c free-packet.c mdfilter.c plaintext.c \
         seskey.c pkglue.c openpgp-oid.c

CC_OPT += -DGPGRT_ENABLE_ES_MACROS \
          -DHAVE_ISASCII \
          -DHAVE_FSEEKO \
          -DHAVE_SIGNAL_H \
          -DGNUPG_NAME='"GnuPG"' \
          -DPRINTABLE_OS_NAME='"Genode"' \
          -DVERSION='"$(< $(GNUPG_SRC_DIR)/../VERSION)"' \
          -DGPG_USE_RSA

CC_OPT_armor        += -Wno-pointer-sign
CC_OPT_iobuf        += -Wno-pointer-sign
CC_OPT_stringhelp   += -Wno-pointer-sign
CC_OPT_progress     += -Wno-pointer-sign
CC_OPT_mainproc     += -Wno-pointer-sign
CC_OPT_sig-check    += -Wno-pointer-sign
CC_OPT_parse-packet += -Wno-pointer-sign
CC_OPT_misc         += -DSCDAEMON_NAME='"scdaemon"' -DEXTSEP_S='"."' -DPATHSEP_S='";"'

vpath gnupg.c   $(PRG_DIR)
vpath dummies.c $(PRG_DIR)
vpath %.c $(GNUPG_SRC_DIR)
vpath %.c $(GNUPG_SRC_DIR)/../common
