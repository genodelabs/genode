TARGET  := pc_nic_drv
LIBS    := base pc_lx_emul jitterentropy

INC_DIR += $(PRG_DIR)/../..
SRC_CC  += main.cc
SRC_C   += dummies.c
SRC_C   += lx_emul.c
SRC_C   += lx_emul/common_dummies.c
SRC_C   += lx_user.c
SRC_C   += rtnetlink.c

vpath %.c  $(REP_DIR)/src/lib/pc
vpath %.cc $(REP_DIR)/src/lib/pc

vpath %.c $(PRG_DIR)/../..
vpath %.cc $(PRG_DIR)/../..

SRC_C += generated_dummies.c
vpath generated_dummies.c $(PRG_DIR)

SRC_CC += genode_c_api/uplink.cc
vpath genode_c_api/uplink.cc $(dir $(call select_from_repositories,src/lib/genode_c_api))

SRC_CC += genode_c_api/mac_address_reporter.cc
vpath genode_c_api/mac_address_reporter.cc $(dir $(call select_from_repositories,src/lib/genode_c_api))
