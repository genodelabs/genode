include $(REP_DIR)/lib/mk/virtualbox5-common.inc

ifeq ($(shell which yasm),)
REQUIRES += installation_of_yasm
endif

SRC_O += VBoxPcBiosBin.o VBoxVgaBiosBin.o VBoxBiosLogoBin.o

VBox%Bin.o : VBox%Bin.rom
	$(MSG_CONVERT)$@
	$(VERBOSE)echo ".global g_ab$*Binary, g_cb$*Binary;" \
	               ".data;" \
	               "g_cb$*Binary:; .long g_ab$*BinaryEnd - g_ab$*Binary;" \
	               ".align 4096;" \
	               "g_ab$*Binary:; .incbin \"$<\";" \
	               "g_ab$*BinaryEnd:;" | \
		$(AS) $(AS_OPT) -f -o $@ -

VBoxPcBiosBin.rom: Devices/PC/BIOS/VBoxBiosAlternative.asm
	$(MSG_ASSEM)
	$(VERBOSE)yasm -f bin -o $@ $<

VBoxVgaBiosBin.rom: Devices/Graphics/BIOS/VBoxVgaBiosAlternative.asm
	$(MSG_ASSEM)
	$(VERBOSE)yasm -f bin -o $@ $<

VBoxBiosLogoBin.o: Devices/Graphics/BIOS/ose_logo.bmp
	$(MSG_CONVERT)$@
	$(VERBOSE)echo ".global g_abVgaDefBiosLogo, g_cbVgaDefBiosLogo;" \
	               ".data;" \
	               "g_cbVgaDefBiosLogo:; .long g_abVgaDefBiosLogoEnd - g_abVgaDefBiosLogo;" \
	               ".align 4096;" \
	               "g_abVgaDefBiosLogo:; .incbin \"$<\";" \
	               "g_abVgaDefBiosLogoEnd:;" | \
		$(AS) $(AS_OPT) -f -o $@ -

vpath %.bmp $(VBOX_DIR)
