PCSC_LITE_DIR := $(call select_from_ports,pcsc-lite)/src/lib/pcsc-lite

include $(call select_from_repositories,lib/import/import-pcsc-lite.mk)

LIBS += ccid libc

# find 'config.h'
INC_DIR += $(REP_DIR)/src/lib/pcsc-lite

INC_DIR += $(PCSC_LITE_DIR)/src

CC_DEF += -DPCSCLITE_STATIC_DRIVER -DIFDHANDLERv3 -DGENODE

SRC_C = debug.c \
        error.c \
        winscard_clnt.c \
        simclist.c \
        sys_unix.c \
        utils.c \
        winscard_msg.c \
        auth.c \
        atrhandler.c \
        debuglog.c \
        dyn_unix.c \
        eventhandler.c \
        hotplug_generic.c \
        ifdwrapper.c \
        powermgt_generic.c \
        prothandler.c \
        readerfactory.c \
        winscard.c

SRC_CC = init.cc

CC_OPT_winscard = -DSCardEstablishContext=SCardEstablishContextImpl \
                  -DSCardReleaseContext=SCardReleaseContextImpl \
                  -DSCardConnect=SCardConnectImpl \
                  -DSCardReconnect=SCardReconnectImpl \
                  -DSCardDisconnect=SCardDisconnectImpl \
                  -DSCardBeginTransaction=SCardBeginTransactionImpl \
                  -DSCardEndTransaction=SCardEndTransactionImpl \
                  -DSCardStatus=SCardStatusImpl \
                  -DSCardControl=SCardControlImpl \
                  -DSCardGetAttrib=SCardGetAttribImpl \
                  -DSCardSetAttrib=SCardSetAttribImpl \
                  -DSCardTransmit=SCardTransmitImpl
                  
vpath %.c $(PCSC_LITE_DIR)/src
vpath %.cc $(REP_DIR)/src/lib/pcsc-lite

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
