REQUIRES := x86_64

TARGET := pc_i2c_hid
LIBS   := base pc_lx_emul jitterentropy

INC_DIR += $(PRG_DIR)

SRC_CC += main.cc
SRC_CC += lx_emul/event.cc
SRC_CC += lx_emul/acpi.cc
SRC_CC += genode_c_api/event.cc

SRC_C += $(notdir $(wildcard $(PRG_DIR)/generated_dummies.c))
SRC_C += acpixf.c
SRC_C += dummies.c
SRC_C += lx_emul.c
SRC_C += lx_emul/common_dummies.c
SRC_C += lx_emul/shadow/drivers/gpio/gpiolib-acpi.c
SRC_C += lx_emul/shadow/drivers/i2c/i2c-core-acpi.c
SRC_C += lx_emul/shadow/drivers/input/evdev.c
SRC_C += lx_user.c

CC_OPT_lx_emul/shadow/drivers/gpio/gpiolib-acpi += -I$(LX_SRC_DIR)/drivers/gpio
CC_OPT += -I$(LX_SRC_DIR)/drivers/i2c/busses
CC_OPT += -DI2C_HID_CONFIGURE_HOOK

# pull in acpi emulation from 'dde_linux' instead of 'pc'
DDE_LINUX_ACPI := 1

SRC_C += lx_emul/shadow/drivers/acpi/bus.c
SRC_C += lx_emul/shadow/drivers/acpi/device_sysfs.c
SRC_C += lx_emul/shadow/drivers/acpi/glue.c
SRC_C += lx_emul/shadow/drivers/acpi/property.c
SRC_C += lx_emul/shadow/drivers/acpi/scan.c
SRC_C += lx_emul/shadow/drivers/acpi/utils.c

DDE_LINUX_DIR := $(subst /src/include/lx_kit,,$(call select_from_repositories,src/include/lx_kit))
vpath %    $(DDE_LINUX_DIR)/src/lib
vpath %.c  $(PRG_DIR)
vpath %.cc $(PRG_DIR)
vpath %.c  $(REP_DIR)/src/lib/pc
vpath %.cc $(REP_DIR)/src/lib/pc

vpath genode_c_api/event.cc $(dir $(call select_from_repositories,src/lib/genode_c_api))
