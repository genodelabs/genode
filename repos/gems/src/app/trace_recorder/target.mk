TARGET      = trace_recorder
INC_DIR    += $(PRG_DIR)
SRC_CC      = main.cc monitor.cc policy.cc ctf/backend.cc pcapng/backend.cc
CONFIG_XSD  = config.xsd
LIBS       += base vfs
