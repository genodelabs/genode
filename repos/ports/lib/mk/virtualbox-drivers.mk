include $(REP_DIR)/lib/mk/virtualbox-common.inc

SRC_CC += Devices/Input/DrvKeyboardQueue.cpp
SRC_CC += Devices/Input/DrvMouseQueue.cpp
SRC_CC += Devices/USB/DrvVUSBRootHub.cpp
SRC_CC += Devices/Storage/DrvBlock.cpp
SRC_CC += Devices/Storage/DrvMediaISO.cpp
SRC_CC += Devices/Storage/DrvVD.cpp
SRC_CC += Devices/Storage/DrvRawImage.cpp
SRC_CC += Devices/PC/DrvACPI.cpp
SRC_CC += Devices/Serial/DrvChar.cpp
SRC_CC += Devices/Serial/DrvRawFile.cpp
SRC_CC += Devices/Serial/DrvHostSerial.cpp

SRC_CC += network.cpp

vpath network.cpp $(REP_DIR)/src/virtualbox
