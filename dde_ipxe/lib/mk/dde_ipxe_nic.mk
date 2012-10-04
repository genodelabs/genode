LIB_DIR     := $(REP_DIR)/src/lib/dde_ipxe
CONTRIB_DIR := $(REP_DIR)/contrib/src

LIBS = dde_kit dde_ipxe_support

SRC_C = nic.c dde.c dummies.c

SRC_C += $(addprefix core/, iobuf.c string.c)
SRC_C += $(addprefix arch/x86/core/, x86_string.c)
SRC_C += $(addprefix arch/i386/core/, rdtsc_timer.c)
SRC_C += $(addprefix net/, ethernet.c netdevice.c nullnet.c eth_slow.c)
SRC_C += $(addprefix drivers/bus/, pciextra.c)
SRC_C += $(addprefix drivers/net/, pcnet32.c) # TODO rtl8139.c eepro100.c virtio-net.c ns8390.c
SRC_C += $(addprefix drivers/net/e1000/, \
         e1000.c e1000_82540.c e1000_82541.c e1000_82542.c e1000_82543.c \
         e1000_api.c e1000_mac.c e1000_main.c e1000_manage.c e1000_nvm.c \
         e1000_phy.c)
SRC_C += $(addprefix drivers/net/e1000e/, \
         e1000e.c e1000e_80003es2lan.c e1000e_82571.c e1000e_ich8lan.c \
         e1000e_mac.c e1000e_main.c e1000e_manage.c e1000e_nvm.c \
         e1000e_phy.c)

INC_DIR += $(LIB_DIR)/include \
           $(CONTRIB_DIR)/include $(CONTRIB_DIR) \
           $(CONTRIB_DIR)/arch/x86/include \
           $(CONTRIB_DIR)/arch/i386/include \
           $(CONTRIB_DIR)/arch/i386/include/pcbios

CC_WARN = -Wall -Wno-address
CC_OPT += $(addprefix -fno-builtin-, putchar toupper tolower)
CC_OPT += -DARCH=i386 -DPLATFORM=pcbios -include compiler.h -DOBJECT=$(notdir $(*:.o=))

#
# Enable debugging of any iPXE object here via '-Ddebug_<object name>=<level>'.
# 'level' may be one of 1, 3, 7.
#
CC_OPT += -Ddebug_lib=7
#CC_OPT += -Ddebug_e1000_main=7 -Ddebug_e1000_82540=7 -Ddebug_netdevice=7
#CC_OPT += -Ddebug_e1000=7 -Ddebug_e1000_82540=7 -Ddebug_e1000_api=7
#CC_OPT += -Ddebug_e1000_main=7 -Ddebug_e1000_manage=7
#CC_OPT += -Ddebug_e1000_phy=7
#CC_OPT += -Ddebug_netdevice=7


vpath nic.c     $(LIB_DIR)
vpath dde.c     $(LIB_DIR)
vpath dummies.c $(LIB_DIR)

vpath %.c       $(CONTRIB_DIR)
