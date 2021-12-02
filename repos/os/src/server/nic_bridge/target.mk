TARGET      = nic_bridge
LIBS        = base net
SRC_CC      = component.cc main.cc nic.cc packet_handler.cc
CONFIG_XSD  = config.xsd
INC_DIR    += $(PRG_DIR)

CC_CXX_WARN_STRICT_CONVERSION =
