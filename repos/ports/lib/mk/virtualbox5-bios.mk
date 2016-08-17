include $(REP_DIR)/lib/mk/virtualbox5-common.inc

ifeq ($(shell which yasm),)
REQUIRES += installation_of_yasm
endif

SRC_O += VBoxPcBiosBinary8086.o  VBoxPcBiosBinary286.o  VBoxPcBiosBinary386.o
SRC_O += VBoxVgaBiosBinary8086.o VBoxVgaBiosBinary286.o VBoxVgaBiosBinary386.o
SRC_O += XVBoxBiosLogoBin.o

VBox%.o : VBox%.rom
	$(MSG_CONVERT)$@
	$(VERBOSE)echo ".global g_ab$*, g_cb$*;" \
	               ".data;" \
	               "g_cb$*:; .long g_ab$*End - g_ab$*;" \
	               ".align 4096;" \
	               "g_ab$*:; .incbin \"$<\";" \
	               "g_ab$*End:;" | \
		$(AS) $(AS_OPT) -f -o $@ -

VBoxPcBiosBinary%.rom: Devices/PC/BIOS/VBoxBiosAlternative%.asm
	$(MSG_ASSEM)
	$(VERBOSE)yasm -f bin -o $@ $<

VBoxVgaBiosBinary%.rom: Devices/Graphics/BIOS/VBoxVgaBiosAlternative%.asm
	$(MSG_ASSEM)
	$(VERBOSE)yasm -f bin -o $@ $<

XVBoxBiosLogoBin.o: Devices/Graphics/BIOS/ose_logo.bmp
	$(MSG_CONVERT)$@
	$(VERBOSE)echo ".global g_abVgaDefBiosLogo, g_cbVgaDefBiosLogo;" \
	               ".data;" \
	               "g_cbVgaDefBiosLogo:; .long g_abVgaDefBiosLogoEnd - g_abVgaDefBiosLogo;" \
	               ".align 4096;" \
	               "g_abVgaDefBiosLogo:; .incbin \"$<\";" \
	               "g_abVgaDefBiosLogoEnd:;" | \
		$(AS) $(AS_OPT) -f -o $@ -

vpath %.bmp $(VBOX_DIR)
