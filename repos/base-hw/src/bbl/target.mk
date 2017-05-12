TARGET   = bbl
REQUIRES = riscv

SRC_C = bbl.c \
        configstring.c \
        kernel_elf.c \
        logo.c \
        mtrap.c \
        minit.c \
        sbi_impl.c \
        snprintf.c \
        string.c \
        mentry.S \
        sbi_entry.S \
        sbi.S \
        image.S

IMAGE_ELF   ?= $(PRG_DIR)/dummy

INC_DIR      += $(PRG_DIR)

CC_OPT_PIC    =
CC_C_OPT     += -mcmodel=medany
CC_OPT_image += -DBBL_PAYLOAD=\"$(IMAGE_ELF)\"

LD_TEXT_ADDR = 0x80000000

CONTRIB  = $(call select_from_ports,bbl)/src/lib/bbl/bbl

LD_SCRIPT_STATIC = $(CONTRIB)/bbl.lds

vpath %.c $(CONTRIB)
vpath %.S $(CONTRIB)
