LIB_DIR          := $(REP_DIR)/src/lib/dde_ipxe
IPXE_CONTRIB_DIR := $(call select_from_ports,dde_ipxe)/src/lib/dde_ipxe/src

SRC_CC = dde_support.cc
SRC_C  = nic.c dde.c dummies.c

SRC_C += $(addprefix core/, iobuf.c string.c bitops.c list.c random.c)
SRC_C += $(addprefix arch/x86/core/, x86_string.c)
SRC_C += $(addprefix arch/i386/core/, rdtsc_timer.c)
SRC_C += $(addprefix net/, ethernet.c netdevice.c nullnet.c eth_slow.c iobpad.c)
SRC_C += $(addprefix drivers/bus/, pciextra.c)
SRC_C += $(addprefix drivers/bitbash/, bitbash.c spi_bit.c)
SRC_C += $(addprefix drivers/nvs/, nvs.c threewire.c)
SRC_C += $(addprefix drivers/net/, intel.c eepro100.c realtek.c mii.c)
SRC_C += $(addprefix drivers/net/tg3/, tg3.c tg3_hw.c tg3_phy.c)

INC_DIR += $(LIB_DIR)/include

INC_DIR += $(IPXE_CONTRIB_DIR)/include $(IPXE_CONTRIB_DIR) \
           $(IPXE_CONTRIB_DIR)/arch/x86/include

CC_WARN = -Wall -Wno-address
CC_OPT += $(addprefix -fno-builtin-, putchar toupper tolower)
CC_C_OPT += -DARCH=i386 -DPLATFORM=pcbios -include compiler.h -DOBJECT=$(notdir $(*:.o=))

#
# Enable debugging of any iPXE object here via '-Ddebug_<object name>=<level>'.
# 'level' may be one of 1, 3, 7.
#
CC_OPT += -Ddebug_lib=7
#CC_OPT += -Ddebug_intel=7
#CC_OPT += -Ddebug_netdevice=7


vpath nic.c          $(LIB_DIR)
vpath dde.c          $(LIB_DIR)
vpath dummies.c      $(LIB_DIR)
vpath dde_support.cc $(LIB_DIR)

vpath %.c            $(IPXE_CONTRIB_DIR)

# vi: set ft=make :
