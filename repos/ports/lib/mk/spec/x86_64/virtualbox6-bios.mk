include $(REP_DIR)/lib/mk/virtualbox6-common.inc

ifeq ($(shell which yasm),)
REQUIRES += installation_of_yasm
endif

SRC_O += VBoxPcBiosBinary8086.o  VBoxPcBiosBinary286.o  VBoxPcBiosBinary386.o
SRC_O += VBoxVgaBiosBinary8086.o VBoxVgaBiosBinary286.o VBoxVgaBiosBinary386.o
SRC_O += XVBoxBiosLogoBin.o
SRC_O += VBoxEFI32.o VBoxEFI64.o

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
	$(VERBOSE)yasm -w -f bin -o $@ $<

VBoxVgaBiosBinary%.rom: Devices/Graphics/BIOS/VBoxVgaBiosAlternative%.asm
	$(MSG_ASSEM)
	$(VERBOSE)yasm -w -f bin -o $@ $<

XVBoxBiosLogoBin.o: $(VBOX_DIR)/Devices/Graphics/BIOS/ose_logo.bmp
	$(MSG_CONVERT)$@
	$(VERBOSE)echo ".global g_abVgaDefBiosLogo, g_cbVgaDefBiosLogo;" \
	               ".data;" \
	               "g_cbVgaDefBiosLogo:; .long g_abVgaDefBiosLogoEnd - g_abVgaDefBiosLogo;" \
	               ".align 4096;" \
	               "g_abVgaDefBiosLogo:; .incbin \"$<\";" \
	               "g_abVgaDefBiosLogoEnd:;" | \
		$(AS) $(AS_OPT) -f -o $@ -

VBoxEFI%.o: $(VBOX_DIR)/Devices/EFI/FirmwareBin/VBoxEFI%.fd
	$(MSG_CONVERT)$@
	$(VERBOSE)echo ".global g_abEfiFirmware$*, g_cbEfiFirmware$*;" \
	               ".data;" \
	               ".align 4096;" \
	               "g_abEfiFirmware$*:; .incbin \"$<\";" \
	               "g_cbEfiFirmware$*:; .long g_cbEfiFirmware$* - g_abEfiFirmware$*;" | \
		$(AS) $(AS_OPT) -f -o $@ -

vpath $(VBOX_DIR)

CC_CXX_WARN_STRICT =
