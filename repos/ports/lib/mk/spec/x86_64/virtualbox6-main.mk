include $(REP_DIR)/lib/mk/virtualbox6-common.inc

LIBS  += stdcxx

SRC_CC += Main/xml/Settings.cpp

SRC_CC += Main/src-all/AuthLibrary.cpp
SRC_CC += Main/src-all/AutoCaller.cpp
SRC_CC += Main/src-all/EventImpl.cpp
SRC_CC += Main/src-all/DisplayResampleImage.cpp
SRC_CC += Main/src-all/DisplayUtils.cpp
SRC_CC += Main/src-all/Global.cpp
SRC_CC += Main/src-all/HashedPw.cpp
SRC_CC += Main/src-all/PCIDeviceAttachmentImpl.cpp
SRC_CC += Main/src-all/ProgressImpl.cpp
SRC_CC += Main/src-all/SecretKeyStore.cpp
SRC_CC += Main/src-all/SharedFolderImpl.cpp
SRC_CC += Main/src-all/ThreadTask.cpp
SRC_CC += Main/src-all/VirtualBoxBase.cpp
SRC_CC += Main/src-all/GlobalStatusConversion.cpp
SRC_CC += Main/src-all/VirtualBoxErrorInfoImpl.cpp
SRC_CC += Main/src-server/AudioAdapterImpl.cpp
SRC_CC += Main/src-server/BandwidthControlImpl.cpp
SRC_CC += Main/src-server/BandwidthGroupImpl.cpp
SRC_CC += Main/src-server/BIOSSettingsImpl.cpp
SRC_CC += Main/src-server/ClientToken.cpp
SRC_CC += Main/src-server/ClientWatcher.cpp
SRC_CC += Main/src-server/DHCPServerImpl.cpp
SRC_CC += Main/src-server/DHCPConfigImpl.cpp
SRC_CC += Main/src-server/GraphicsAdapterImpl.cpp
SRC_CC += Main/src-server/GuestOSTypeImpl.cpp
SRC_CC += Main/src-server/HostImpl.cpp
SRC_CC += Main/src-server/MachineImpl.cpp
SRC_CC += Main/src-server/MachineImplCloneVM.cpp
SRC_CC += Main/src-server/Matching.cpp
SRC_CC += Main/src-server/MediumAttachmentImpl.cpp
SRC_CC += Main/src-server/MediumImpl.cpp
SRC_CC += Main/src-server/MediumFormatImpl.cpp
SRC_CC += Main/src-server/MediumLock.cpp
SRC_CC += Main/src-server/NATEngineImpl.cpp 
SRC_CC += Main/src-server/NATNetworkImpl.cpp
SRC_CC += Main/src-server/NetworkAdapterImpl.cpp
SRC_CC += Main/src-server/NetworkServiceRunner.cpp
SRC_CC += Main/src-server/ParallelPortImpl.cpp
SRC_CC += Main/src-server/Performance.cpp
SRC_CC += Main/src-server/PerformanceImpl.cpp
SRC_CC += Main/src-server/ProgressProxyImpl.cpp
SRC_CC += Main/src-server/RecordingSettingsImpl.cpp
SRC_CC += Main/src-server/RecordingScreenSettingsImpl.cpp
SRC_CC += Main/src-server/SerialPortImpl.cpp
SRC_CC += Main/src-server/SnapshotImpl.cpp
SRC_CC += Main/src-server/StorageControllerImpl.cpp
SRC_CC += Main/src-server/SystemPropertiesImpl.cpp
SRC_CC += Main/src-server/TokenImpl.cpp
SRC_CC += Main/src-server/USBControllerImpl.cpp
SRC_CC += Main/src-server/USBDeviceFilterImpl.cpp
SRC_CC += Main/src-server/USBDeviceFiltersImpl.cpp
SRC_CC += Main/src-server/VirtualBoxImpl.cpp
SRC_CC += Main/src-server/VRDEServerImpl.cpp
SRC_CC += Main/src-server/HostDnsService.cpp
SRC_CC += Main/src-server/HostNetworkInterfaceImpl.cpp
SRC_CC += Main/src-server/MediumIOImpl.cpp
SRC_CC += Main/src-server/DataStreamImpl.cpp
SRC_CC += Main/src-server/HostPower.cpp
SRC_CC += Main/src-server/generic/NetIf-generic.cpp

# use OS/2 version of 'pm::createHAL()' because it is empty
SRC_CC += Main/src-server/os2/PerformanceOs2.cpp

# generated from VBox/Main/idl/comimpl.xsl
SRC_CC += Main/VBoxEvents.cpp

# see comment in virtualbox6-client.mk
CC_OPT_Main/src-server/MediumImpl = -Wno-enum-compare

# prevent double define of 'LOG_GROUP'
VBOX_CC_OPT += -DIN_VBOXSVC

INC_DIR += $(VBOX_DIR)/Main/xml
INC_DIR += $(VBOX_DIR)/Main/include
INC_DIR += $(VIRTUALBOX_DIR)/VBoxAPIWrap
INC_DIR += $(VIRTUALBOX_DIR)/include/VBox/Graphics

CC_CXX_WARN_STRICT =
