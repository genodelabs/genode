include $(REP_DIR)/lib/mk/virtualbox6-common.inc

LIBS  += stdcxx

#
# The 'ProgressImpl' compilation unit is used by both the VBox server and
# VBox client, but compiled with (client) and without (server) the
# 'VBOX_COM_INPROC' define set. Since the ABI for 'ProgressImpl' is not
# the same for client and server but we want to like client and server
# together, we need to disambiguate both flavours.
#
# - At the client side, we rename the 'Progress' class to 'ClientProgress'
# - We set the 'VBOX_COM_INPROC' define for the client code only
#
VBOX_CC_OPT += -DProgress=ClientProgress
VBOX_CC_OPT += -DVBOX_COM_INPROC

SRC_CC += Main/src-all/ProgressImpl.cpp

SRC_CC += Main/src-client/AdditionsFacilityImpl.cpp
SRC_CC += Main/src-client/BusAssignmentManager.cpp
SRC_CC += Main/src-client/ClientTokenHolder.cpp
SRC_CC += Main/src-client/ConsoleImpl.cpp
SRC_CC += Main/src-client/ConsoleImpl2.cpp
SRC_CC += Main/src-client/ConsoleVRDPServer.cpp
SRC_CC += Main/src-client/DisplaySourceBitmapImpl.cpp
SRC_CC += Main/src-client/DisplayImpl.cpp
SRC_CC += Main/src-client/DisplayImplLegacy.cpp
SRC_CC += Main/src-client/DrvAudioVRDE.cpp
SRC_CC += Main/src-client/EmulatedUSBImpl.cpp
SRC_CC += Main/src-client/GuestCtrlImpl
SRC_CC += Main/src-client/GuestCtrlPrivate
SRC_CC += Main/src-client/GuestDirectoryImpl
SRC_CC += Main/src-client/GuestDnDPrivate
SRC_CC += Main/src-client/GuestFileImpl
SRC_CC += Main/src-client/GuestProcessImpl
SRC_CC += Main/src-client/GuestDnDSourceImpl
SRC_CC += Main/src-client/GuestFsObjInfoImpl
SRC_CC += Main/src-client/GuestSessionImpl
SRC_CC += Main/src-client/GuestDnDTargetImpl
SRC_CC += Main/src-client/GuestImpl
SRC_CC += Main/src-client/GuestSessionImpl
SRC_CC += Main/src-client/GuestSessionImplTasks

SRC_CC += Main/src-client/HGCM.cpp
SRC_CC += Main/src-client/HGCMObjects.cpp
SRC_CC += Main/src-client/HGCMThread.cpp
SRC_CC += Main/src-client/KeyboardImpl.cpp
SRC_CC += Main/src-client/MachineDebuggerImpl.cpp
SRC_CC += Main/src-client/MouseImpl.cpp
SRC_CC += Main/src-client/RemoteUSBBackend.cpp
SRC_CC += Main/src-client/RemoteUSBDeviceImpl.cpp
SRC_CC += Main/src-client/SessionImpl.cpp
SRC_CC += Main/src-client/USBDeviceImpl.cpp
SRC_CC += Main/src-client/UsbWebcamInterface.cpp
SRC_CC += Main/src-client/VMMDevInterface.cpp

SRC_CC += GuestHost/DragAndDrop/DnDUtils.cpp

#
# Suppress warnings caused by using anonymous enum values in 'a ? b : c'
# expressions.
#
# "enumeral mismatch in conditional expression"
#
CC_OPT_Main/src-client/ConsoleImpl         = -Wno-enum-compare
CC_OPT_Main/src-client/ConsoleImpl2        = -Wno-enum-compare
CC_OPT_Main/src-client/GuestImpl           = -Wno-enum-compare
CC_OPT_Main/src-client/RemoteUSBDeviceImpl = -Wno-enum-compare
CC_OPT_Main/src-client/GuestDnDPrivate     = -Wno-enum-compare

INC_DIR += $(VBOX_DIR)/Main/xml
INC_DIR += $(VBOX_DIR)/Main/include
INC_DIR += $(VIRTUALBOX_DIR)/VBoxAPIWrap
INC_DIR += $(VIRTUALBOX_DIR)/include/VBox/Graphics

CC_CXX_WARN_STRICT =
