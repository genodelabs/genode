include $(REP_DIR)/lib/mk/virtualbox-common.inc

SRC_CC += Devices/Input/DrvKeyboardQueue.cpp
SRC_CC += Devices/Input/DrvMouseQueue.cpp
SRC_CC += Devices/Storage/DrvBlock.cpp
SRC_CC += Devices/Storage/DrvMediaISO.cpp
SRC_CC += Devices/Storage/DrvVD.cpp
SRC_CC += Devices/PC/DrvACPI.cpp
SRC_CC += Devices/Serial/DrvChar.cpp
SRC_CC += Devices/Serial/DrvRawFile.cpp
SRC_CC += Devices/Serial/DrvHostSerial.cpp
SRC_CC += Main/src-client/MouseImpl.cpp

SRC_CC += network.cpp

vpath network.cpp $(REP_DIR)/src/virtualbox

# includes needed by 'MouseImpl.cpp'
INC_DIR += $(VBOX_DIR)/Main/include
INC_DIR += $(VBOX_DIR)/Frontends/VBoxBFE
