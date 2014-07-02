include $(REP_DIR)/lib/mk/virtualbox-common.inc

SRC_CC += Devices/PC/DevFwCommon.cpp
SRC_CC += Devices/PC/DevPcBios.cpp
SRC_CC += Devices/Bus/DevPCI.cpp
SRC_CC += Devices/PC/DevACPI.cpp
SRC_CC += Devices/PC/ACPI/VBoxAcpi.cpp
SRC_C  += Devices/PC/DevPcArch.c
SRC_CC += Devices/Input/DevPS2.cpp
SRC_CC += Devices/Input/PS2K.cpp
SRC_CC += Devices/PC/DevPit-i8254.cpp
SRC_CC += Devices/PC/DevPIC.cpp
SRC_CC += Devices/PC/DevRTC.cpp
SRC_CC += Devices/PC/DevDMA.cpp
SRC_CC += Devices/PC/DevAPIC.cpp
SRC_CC += Devices/Graphics/DevVGA.cpp
SRC_CC += Devices/Graphics/DevVGA_VBVA.cpp
SRC_CC += Devices/Graphics/DevVGA_VDMA.cpp
SRC_CC += Devices/Graphics/HGSMI/HGSMIHost.cpp
SRC_CC += Devices/Graphics/HGSMI/HGSMIHostHlp.cpp
SRC_CC += Devices/Graphics/HGSMI/SHGSMIHost.cpp
SRC_CC += Devices/Storage/DevATA.cpp
SRC_CC += Devices/Storage/Debug.cpp
SRC_CC += Devices/Storage/fdc.c
SRC_CC += Devices/Storage/DrvRawImage.cpp
SRC_CC += Devices/Network/DevE1000.cpp
SRC_CC += Devices/Network/DevE1000Phy.cpp
SRC_CC += Devices/Network/DevEEPROM.cpp
SRC_CC += Devices/Network/DevPCNet.cpp
SRC_CC += Devices/VMMDev/VMMDev.cpp
SRC_CC += Devices/VMMDev/VMMDevHGCM.cpp
SRC_CC += GuestHost/HGSMI/HGSMICommon.cpp
SRC_CC += Devices/Serial/DevSerial.cpp
SRC_CC += Devices/PC/DevIoApic.cpp


INC_DIR += $(VBOX_DIR)/Devices/build
INC_DIR += $(VBOX_DIR)/Devices/Bus

CC_WARN += -Wno-unused-but-set-variable

#
# Definitions needed to compile DevVGA.cpp
#
# VBOX_WITH_VDMA is needed because otherwise, the alignment of the
# VGASTATE::lock member would violate the assertion
# '!((uintptr_t)pvSample & 7)' in 'stamR3RegisterU'.
#
CC_OPT += -DVBOX_WITH_WDDM -DVBOX_WITH_VDMA

Devices/Graphics/DevVGA.o: vbetables.h

vbetables.h: vbetables-gen
	$(MSG_CONVERT)$@
	$(VERBOSE)./$^ > $@

vbetables-gen: Devices/Graphics/BIOS/vbetables-gen.c
	$(MSG_BUILD)$@
	$(VERBOSE)g++ $(VBOX_CC_OPT) $(addprefix -I,$(INC_DIR)) -o $@ $^



Devices/PC/ACPI/VBoxAcpi.o: vboxaml.hex vboxssdt-standard.hex vboxssdt-cpuhotplug.hex

vboxaml.hex: vbox.dsl
	iasl -tc -vs -p $@ $^

vboxssdt-standard.hex: vbox-standard.dsl
	iasl -tc -vs -p $@ $^ && \
	mv $@ $@.tmp && \
	sed "s/AmlCode/AmlCodeSsdtStandard/g" <$@.tmp >$@ && \
	rm $@.tmp

vboxssdt-cpuhotplug.hex: vbox-cpuhotplug.dsl
	gcc -E -P -x c -o $@.pre $< && \
	iasl -tc -vs -p $@ $@.pre && \
	mv $@ $@.tmp && \
	sed "s/AmlCode/AmlCodeSsdtCpuHotPlug/g" <$@.tmp >$@ && \
	rm $@.tmp $@.pre

vpath %.dsl $(VBOX_DIR)/Devices/PC
