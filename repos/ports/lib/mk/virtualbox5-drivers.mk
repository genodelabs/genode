include $(REP_DIR)/lib/mk/virtualbox5-common.inc

#
# Prevent inclusion of the Genode::Log definition after the vbox #define
# of 'Log'. Otherwise, the attempt to compile base/log.h will fail.
#
VBOX_CC_OPT += -include base/log.h

SRC_CC += Devices/Input/DrvKeyboardQueue.cpp
SRC_CC += Devices/Input/DrvMouseQueue.cpp
SRC_CC += Devices/USB/DrvVUSBRootHub.cpp
SRC_CC += Devices/Storage/DrvVD.cpp
SRC_CC += Devices/PC/DrvACPI.cpp
SRC_CC += Devices/Serial/DrvChar.cpp
SRC_CC += Devices/Serial/DrvRawFile.cpp
SRC_CC += Devices/Serial/DrvHostSerial.cpp
SRC_CC += Devices/Audio/DrvAudio.cpp
SRC_CC += Devices/Audio/DrvHostNullAudio.cpp

SRC_CC += network.cpp
SRC_CC += audio.cc

INC_DIR += $(VBOX_DIR)/Devices/Audio

vpath network.cpp  $(REP_DIR)/src/virtualbox
vpath audio.cc     $(REP_DIR)/src/virtualbox5
