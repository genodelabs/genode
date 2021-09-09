include $(REP_DIR)/lib/mk/virtualbox6-common.inc

SRC_CC += Devices/Audio/AudioHlp.cpp
SRC_CC += Devices/Audio/AudioMixBuffer.cpp
SRC_CC += Devices/Audio/AudioMixer.cpp
SRC_CC += Devices/Audio/DevHda.cpp
SRC_CC += Devices/Audio/DevHdaCodec.cpp
SRC_CC += Devices/Audio/DevHdaStream.cpp
SRC_CC += Devices/Audio/DevIchAc97.cpp
SRC_CC += Devices/Audio/DrvAudio.cpp
SRC_CC += Devices/Audio/DrvHostAudioNull.cpp
SRC_CC += Devices/Audio/DrvHostAudioOss.cpp
SRC_CC += Devices/Bus/DevPCI.cpp
SRC_CC += Devices/Bus/DevPciIch9.cpp
SRC_CC += Devices/Bus/MsiCommon.cpp
SRC_CC += Devices/Bus/MsixCommon.cpp
SRC_CC += Devices/EFI/DevFlash.cpp
SRC_CC += Devices/EFI/DevSmc.cpp
SRC_CC += Devices/EFI/FlashCore.cpp
SRC_CC += Devices/GIMDev/DrvUDP.cpp
SRC_CC += Devices/GIMDev/GIMDev.cpp
SRC_CC += Devices/Graphics/DevVGA.cpp
SRC_CC += Devices/Graphics/DevVGA-SVGA.cpp
SRC_CC += Devices/Graphics/DevVGA_VBVA.cpp
SRC_CC += Devices/Graphics/DevVGA_VDMA.cpp
SRC_CC += Devices/Graphics/HGSMI/HGSMIHost.cpp
SRC_CC += Devices/Graphics/HGSMI/SHGSMIHost.cpp
SRC_CC += Devices/Input/DevPS2.cpp
SRC_CC += Devices/Input/DevPS2K.cpp
SRC_CC += Devices/Input/DevPS2M.cpp
SRC_CC += Devices/Input/DrvKeyboardQueue.cpp
SRC_CC += Devices/Input/DrvMouseQueue.cpp
SRC_CC += Devices/Input/UsbKbd.cpp
SRC_CC += Devices/Input/UsbMouse.cpp
SRC_CC += Devices/Network/DevE1000.cpp
SRC_CC += Devices/Network/DevE1000Phy.cpp
SRC_CC += Devices/Network/DevEEPROM.cpp
SRC_CC += Devices/Network/DevPCNet.cpp
SRC_CC += Devices/Network/DrvNetShaper.cpp
SRC_CC += Devices/Network/DrvNetSniffer.cpp
SRC_CC += Devices/Network/Pcap.cpp
SRC_CC += Devices/Parallel/DevParallel.cpp
SRC_CC += Devices/PC/ACPI/VBoxAcpi.cpp
SRC_CC += Devices/PC/DevACPI.cpp
SRC_CC += Devices/PC/DevDMA.cpp
SRC_CC += Devices/PC/DevFwCommon.cpp
SRC_CC += Devices/PC/DevHPET.cpp
SRC_CC += Devices/PC/DevIoApic.cpp
SRC_CC += Devices/PC/DevLpc-new.cpp
SRC_C  += Devices/PC/DevPcArch.c
SRC_CC += Devices/PC/DevPcBios.cpp
SRC_CC += Devices/PC/DevPIC.cpp
SRC_CC += Devices/PC/DevPit-i8254.cpp
SRC_CC += Devices/PC/DevRTC.cpp
SRC_CC += Devices/PC/DrvACPI.cpp
SRC_CC += Devices/PC/DrvAcpiCpu.cpp
SRC_CC += Devices/PC/DevLpc-new.cpp
SRC_CC += Devices/Serial/DevSerial.cpp
SRC_CC += Devices/Serial/DrvChar.cpp
SRC_CC += Devices/Serial/DrvHostSerial.cpp
SRC_CC += Devices/Serial/DrvNamedPipe.cpp
SRC_CC += Devices/Serial/DrvRawFile.cpp
SRC_CC += Devices/Serial/DrvTCP.cpp
SRC_CC += Devices/Serial/UartCore.cpp
SRC_CC += Devices/Storage/ATAPIPassthrough.cpp
SRC_CC += Devices/Storage/Debug.cpp
SRC_CC += Devices/Storage/DevAHCI.cpp
SRC_CC += Devices/Storage/DevATA.cpp
SRC_C  += Devices/Storage/DevFdc.c
SRC_CC += Devices/Storage/DrvHostBase.cpp
SRC_CC += Devices/Storage/DrvHostDVD.cpp
SRC_CC += Devices/Storage/DrvSCSI.cpp
SRC_CC += Devices/Storage/DrvVD.cpp
SRC_CC += Devices/Storage/HBDMgmt-generic.cpp
SRC_CC += Devices/Storage/IOBufMgmt.cpp
SRC_CC += Devices/Storage/VSCSI/VSCSIDevice.cpp
SRC_CC += Devices/Storage/VSCSI/VSCSIIoReq.cpp
SRC_CC += Devices/Storage/VSCSI/VSCSILun.cpp
SRC_CC += Devices/Storage/VSCSI/VSCSILunMmc.cpp
SRC_CC += Devices/Storage/VSCSI/VSCSILunSbc.cpp
SRC_CC += Devices/Storage/VSCSI/VSCSISense.cpp
SRC_CC += Devices/Storage/VSCSI/VSCSIVpdPagePool.cpp
SRC_CC += Devices/Trace/DrvIfsTrace.cpp
SRC_CC += Devices/Trace/DrvIfsTrace-serial.cpp
SRC_CC += Devices/USB/DevOHCI.cpp
SRC_CC += Devices/USB/DrvVUSBRootHub.cpp
SRC_CC += Devices/USB/USBProxyDevice.cpp
SRC_CC += Devices/USB/VUSBDevice.cpp
SRC_CC += Devices/USB/VUSBSniffer.cpp
SRC_CC += Devices/USB/VUSBSnifferPcapNg.cpp
SRC_CC += Devices/USB/VUSBSnifferUsbMon.cpp
SRC_CC += Devices/USB/VUSBSnifferVmx.cpp
SRC_CC += Devices/USB/VUSBUrb.cpp
SRC_CC += Devices/USB/VUSBUrbPool.cpp
SRC_CC += Devices/USB/VUSBUrbTrace.cpp
SRC_CC += Devices/VMMDev/VMMDev.cpp
SRC_CC += Devices/VMMDev/VMMDevHGCM.cpp

SRC_CC += GuestHost/HGSMI/HGSMICommon.cpp
SRC_CC += GuestHost/HGSMI/HGSMIMemAlloc.cpp
SRC_CC += GuestHost/DragAndDrop/DnDTransferList.cpp
SRC_CC += GuestHost/DragAndDrop/DnDTransferObject.cpp
SRC_CC += GuestHost/DragAndDrop/DnDDroppedFiles.cpp
SRC_CC += GuestHost/DragAndDrop/DnDMIME.cpp
SRC_CC += GuestHost/DragAndDrop/DnDPath.cpp

INC_DIR += $(VBOX_DIR)/Devices/build
INC_DIR += $(VBOX_DIR)/Devices/Bus
INC_DIR += $(VIRTUALBOX_DIR)/include/VBox/Graphics

# found in src/VBox/Devices/Makefile.kmk
CC_OPT += -DVBOX_HGCM_HOST_CODE

Devices/Graphics/DevVGA.o: vbetables.h

vbetables.h: vbetables-gen
	$(MSG_CONVERT)$@
	$(VERBOSE)./$^ > $@

vbetables-gen: Devices/Graphics/BIOS/vbetables-gen.c
	$(MSG_BUILD)$@
	$(VERBOSE)gcc $(VBOX_CC_OPT) $(addprefix -I,$(INC_DIR)) -o $@ $^

Devices/PC/ACPI/VBoxAcpi.o: vboxaml.hex vboxssdt_standard.hex vboxssdt_cpuhotplug.hex

vboxaml.hex: vbox.dsl
	$(VERBOSE)( \
	 iasl -tc -vi -vr -vs -p $@ $^ && \
	 mv $@ $@.tmp && \
	 sed "s/vboxaml_aml_code/AmlCode/g" <$@.tmp >$@ && \
	 rm $@.tmp \
	)

vboxssdt_standard.hex: vbox-standard.dsl
	$(VERBOSE)( \
	 iasl -tc -vi -vr -vs -p $@ $^ && \
	 mv $@ $@.tmp && \
	 sed "s/AmlCode\|vboxssdt_standard_aml_code/AmlCodeSsdtStandard/g" <$@.tmp >$@.tmp2 && \
	 sed "s/__VBOXSSDT-STANDARD_HEX__/__VBOXSSDT_STANDARD_HEX__/g" <$@.tmp2 >$@ && \
	 rm $@.tmp $@.tmp2 \
	)

vboxssdt_cpuhotplug.hex: vbox-cpuhotplug.dsl
	$(VERBOSE)( \
	 gcc -E -P -x c -o $@.pre $< && \
	 sed "s/<NL>/\n/g" <$@.pre >$@.pre1 && \
	 iasl -tc -vi -vr -vs -p $@ $@.pre1 && \
	 mv $@ $@.tmp && \
	 sed "s/AmlCode\|vboxssdt_cpuhotplug_aml_code/AmlCodeSsdtCpuHotPlug/g" <$@.tmp >$@.tmp2 && \
	 sed "s/__VBOXSSDT-CPUHOTPLUG_HEX__/__VBOXSSDT_CPUHOTPLUG_HEX__/g" <$@.tmp2 >$@ && \
	 rm $@.tmp $@.tmp2 $@.pre $@.pre1 \
	)

vpath %.dsl $(VBOX_DIR)/Devices/PC

CC_CXX_WARN_STRICT =
