include $(REP_DIR)/lib/mk/virtualbox5-common.inc

SRC_CC += Storage/VCICache.cpp
SRC_CC += Storage/VD.cpp
SRC_CC += Storage/VMDK.cpp
SRC_CC += Storage/DMG.cpp
SRC_CC += Storage/ISCSI.cpp
SRC_CC += Storage/Parallels.cpp
SRC_CC += Storage/QCOW.cpp
SRC_CC += Storage/QED.cpp
SRC_CC += Storage/RAW.cpp
SRC_CC += Storage/VD.cpp
SRC_CC += Storage/VDI.cpp
SRC_CC += Storage/VDIfVfs.cpp
SRC_CC += Storage/VHD.cpp
SRC_CC += Storage/VHDX.cpp
SRC_CC += Storage/VMDK.cpp

CC_CXX_WARN_STRICT =
