TARGET   = noux_net
LIBS    += alarm libc libc_lwip_nic_dhcp vfs
SRC_CC   = main.cc syscall.cc net.cc construct.cc

INC_DIR += $(PRG_DIR)
INC_DIR += $(PRG_DIR)/../

vpath main.cc    $(PRG_DIR)/..
vpath syscall.cc $(PRG_DIR)/..
vpath net.cc     $(PRG_DIR)

CC_CXX_WARN_STRICT =
