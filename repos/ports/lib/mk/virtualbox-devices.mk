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
SRC_CC += Devices/Graphics/DevVGA-SVGA.cpp
SRC_CC += Devices/Graphics/HGSMI/HGSMIHost.cpp
SRC_CC += Devices/Graphics/HGSMI/HGSMIHostHlp.cpp
SRC_CC += Devices/Graphics/HGSMI/SHGSMIHost.cpp
SRC_CC += Devices/Storage/ATAPIPassthrough.cpp
SRC_CC += Devices/Storage/DevAHCI.cpp
SRC_CC += Devices/Storage/DevATA.cpp
SRC_CC += Devices/Storage/Debug.cpp
SRC_CC += Devices/Storage/DevFdc.c
SRC_CC += Devices/Network/DevE1000.cpp
SRC_CC += Devices/Network/DevE1000Phy.cpp
SRC_CC += Devices/Network/DevEEPROM.cpp
SRC_CC += Devices/Network/DevPCNet.cpp
SRC_CC += Devices/VMMDev/VMMDev.cpp
SRC_CC += Devices/VMMDev/VMMDevHGCM.cpp
SRC_CC += GuestHost/HGSMI/HGSMICommon.cpp
SRC_CC += Devices/Serial/DevSerial.cpp
SRC_CC += Devices/PC/DevIoApic.cpp

SRC_C  += Devices/Audio/audio.c
SRC_C  += Devices/Audio/audiosniffer.c
SRC_C  += Devices/Audio/filteraudio.c
SRC_C  += Devices/Audio/mixeng.c
SRC_C  += Devices/Audio/noaudio.c
SRC_CC += Devices/Audio/DevIchAc97.cpp
SRC_CC += Devices/Audio/DevIchHda.cpp
SRC_CC += Devices/Audio/DevIchHdaCodec.cpp
SRC_CC += Devices/USB/DevOHCI.cpp
SRC_CC += Devices/USB/USBProxyDevice.cpp
SRC_CC += Devices/USB/VUSBDevice.cpp
SRC_CC += Devices/USB/VUSBReadAhead.cpp
SRC_CC += Devices/USB/VUSBUrb.cpp
SRC_CC += Devices/Input/UsbMouse.cpp
SRC_CC += Devices/Input/UsbKbd.cpp

SRC_CC += Devices/build/VBoxDD.cpp

SRC_CC += devxhci.cc

INC_DIR += $(VBOX_DIR)/Devices/build
INC_DIR += $(VBOX_DIR)/Devices/Bus

CC_WARN += -Wno-unused-but-set-variable

CC_OPT += -DVBOX_WITH_WDDM -DVBOX_WITH_WDDM_W8 -DVBOXWDDM_WITH_VBVA
CC_OPT += -DVBOX_WITH_VDMA
CC_OPT += -DVBOX_WITH_VMSVGA

# found in src/VBox/Devices/Makefile.kmk
CC_OPT += -DVBOX_HGCM_HOST_CODE

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
vpath %.cc  $(REP_DIR)/src/virtualbox
