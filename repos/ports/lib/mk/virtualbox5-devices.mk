include $(REP_DIR)/lib/mk/virtualbox5-common.inc

SRC_CC += Devices/Bus/DevPCI.cpp
SRC_CC += Devices/Bus/DevPciIch9.cpp
SRC_CC += Devices/Bus/MsiCommon.cpp
SRC_CC += Devices/Bus/MsixCommon.cpp
SRC_CC += Devices/EFI/DevSmc.cpp
SRC_CC += Devices/Input/DevPS2.cpp
SRC_CC += Devices/Input/PS2K.cpp
SRC_CC += Devices/Input/PS2M.cpp
SRC_CC += Devices/PC/DevAPIC.cpp
SRC_CC += Devices/PC/DevACPI.cpp
SRC_CC += Devices/PC/DevFwCommon.cpp
SRC_CC += Devices/PC/DevDMA.cpp
SRC_CC += Devices/PC/DevHPET.cpp
ifeq ($(filter $(VBOX_CC_OPT),-DVBOX_WITH_NEW_IOAPIC),)
SRC_CC += Devices/PC/DevIoApic_Old.cpp
else
SRC_CC += Devices/PC/DevIoApic.cpp
endif
SRC_CC += Devices/PC/DevLPC.cpp
SRC_CC += Devices/PC/DevPcBios.cpp
SRC_C  += Devices/PC/DevPcArch.c
SRC_CC += Devices/PC/DevPit-i8254.cpp
SRC_CC += Devices/PC/DevPIC.cpp
SRC_CC += Devices/PC/DevRTC.cpp
SRC_CC += Devices/PC/ACPI/VBoxAcpi.cpp
SRC_CC += Devices/Graphics/DevVGA.cpp
SRC_CC += Devices/Graphics/DevVGA_VBVA.cpp
SRC_CC += Devices/Graphics/DevVGA_VDMA.cpp
SRC_CC += Devices/Graphics/DevVGA-SVGA.cpp
SRC_CC += Devices/Graphics/HGSMI/HGSMIHost.cpp
SRC_CC += Devices/Graphics/HGSMI/SHGSMIHost.cpp
SRC_CC += Devices/Storage/ATAPIPassthrough.cpp
SRC_CC += Devices/Storage/DevAHCI.cpp
SRC_CC += Devices/Storage/DevATA.cpp
SRC_CC += Devices/Storage/Debug.cpp
SRC_C  += Devices/Storage/DevFdc.c
SRC_CC += Devices/Storage/IOBufMgmt.cpp
SRC_CC += Devices/Network/DevE1000.cpp
SRC_CC += Devices/Network/DevE1000Phy.cpp
SRC_CC += Devices/Network/DevEEPROM.cpp
SRC_CC += Devices/Network/DevPCNet.cpp
SRC_CC += Devices/VMMDev/VMMDev.cpp
SRC_CC += Devices/VMMDev/VMMDevHGCM.cpp
SRC_CC += Devices/Serial/DevSerial.cpp

SRC_CC += Devices/Audio/AudioMixBuffer.cpp
SRC_CC += Devices/Audio/AudioMixer.cpp
SRC_CC += Devices/Audio/DevHDA.cpp
SRC_CC += Devices/Audio/DevIchAc97.cpp
SRC_CC += Devices/Audio/DrvAudioCommon.cpp
SRC_CC += Devices/Audio/HDACodec.cpp
SRC_CC += Devices/USB/DevOHCI.cpp
SRC_CC += Devices/USB/USBProxyDevice.cpp
SRC_CC += Devices/USB/VUSBBufferedPipe.cpp
SRC_CC += Devices/USB/VUSBDevice.cpp
SRC_CC += Devices/USB/VUSBSniffer.cpp
SRC_CC += Devices/USB/VUSBSnifferPcapNg.cpp
SRC_CC += Devices/USB/VUSBSnifferUsbMon.cpp
SRC_CC += Devices/USB/VUSBSnifferVmx.cpp
SRC_CC += Devices/USB/VUSBUrb.cpp
SRC_CC += Devices/USB/VUSBUrbPool.cpp
SRC_CC += Devices/USB/VUSBUrbTrace.cpp
SRC_CC += Devices/Input/UsbMouse.cpp
SRC_CC += Devices/Input/UsbKbd.cpp

SRC_CC += Devices/build/VBoxDD.cpp

SRC_CC += GuestHost/HGSMI/HGSMICommon.cpp
SRC_CC += GuestHost/HGSMI/HGSMIMemAlloc.cpp

SRC_CC += devxhci.cc

INC_DIR += $(VBOX_DIR)/Devices/build
INC_DIR += $(VBOX_DIR)/Devices/Bus

CC_WARN += -Wno-unused-but-set-variable

# found in src/VBox/Devices/Makefile.kmk
CC_OPT += -DVBOX_HGCM_HOST_CODE

Devices/Graphics/DevVGA.o: vbetables.h

vbetables.h: vbetables-gen
	$(MSG_CONVERT)$@
	$(VERBOSE)./$^ > $@

vbetables-gen: Devices/Graphics/BIOS/vbetables-gen.c
	$(MSG_BUILD)$@
	$(VERBOSE)gcc $(VBOX_CC_OPT) $(addprefix -I,$(INC_DIR)) -o $@ $^

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
	sed "s/<NL>/\n/g" <$@.pre >$@.pre1 && \
	iasl -tc -vs -p $@ $@.pre1 && \
	mv $@ $@.tmp && \
	sed "s/AmlCode/AmlCodeSsdtCpuHotPlug/g" <$@.tmp >$@ && \
	rm $@.tmp $@.pre $@.pre1

vpath %.dsl $(VBOX_DIR)/Devices/PC
vpath devxhci.cc $(REP_DIR)/src/virtualbox5
