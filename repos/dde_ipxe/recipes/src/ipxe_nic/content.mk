PORT_DIR := $(call port_dir,$(REP_DIR)/ports/dde_ipxe)

LIB_MK := $(addprefix lib/mk/, dde_ipxe_nic.inc) \
          $(foreach SPEC,x86_32 x86_64,lib/mk/spec/$(SPEC)/dde_ipxe_nic.mk)

MIRROR_FROM_REP_DIR := $(LIB_MK) src patches include

MIRROR_FROM_PORT_DIR := $(addprefix src/lib/dde_ipxe/src/, \
                          arch/i386/core/rdtsc_timer.c \
                          arch/x86/core/x86_string.c \
                          core/bitops.c \
                          core/iobuf.c \
                          core/list.c \
                          core/string.c \
                          core/random.c \
                          drivers/bitbash/bitbash.c \
                          drivers/bitbash/spi_bit.c \
                          drivers/bus/pciextra.c \
                          drivers/net/eepro100.c \
                          drivers/net/eepro100.h \
                          drivers/net/intel.c \
                          drivers/net/intel.h \
                          drivers/net/mii.c \
                          drivers/net/realtek.c \
                          drivers/net/realtek.h \
                          drivers/net/tg3/tg3.c \
                          drivers/net/tg3/tg3.h \
                          drivers/net/tg3/tg3_hw.c \
                          drivers/net/tg3/tg3_phy.c \
                          drivers/nvs/nvs.c \
                          drivers/nvs/threewire.c \
                          net/eth_slow.c \
                          net/iobpad.c \
                          net/ethernet.c \
                          net/netdevice.c \
                          net/nullnet.c \
                          include \
                          arch/i386/include \
                          arch/x86_64/include \
                          arch/x86/include \
                          config)

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_PORT_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

LICENSE:
	cp $(PORT_DIR)/src/lib/dde_ipxe/COPYING $@
