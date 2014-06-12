TARGET = maelstrom

MAELSTROM_DIR := $(call select_from_ports,maelstrom)/src/app/maelstrom

SRC_CC  = $(notdir $(wildcard $(MAELSTROM_DIR)/*.cpp))
SRC_CC += $(notdir $(wildcard $(MAELSTROM_DIR)/screenlib/*.cpp))
SRC_CC += $(notdir $(wildcard $(MAELSTROM_DIR)/maclib/Mac_*.cpp))
SRC_CC += $(notdir $(wildcard $(MAELSTROM_DIR)/netlogic/*.cpp))

vpath %.cpp $(MAELSTROM_DIR)
vpath %.cpp $(MAELSTROM_DIR)/screenlib
vpath %.cpp $(MAELSTROM_DIR)/maclib
vpath %.cpp $(MAELSTROM_DIR)/netlogic

INC_DIR += $(MAELSTROM_DIR)
INC_DIR += $(addprefix $(MAELSTROM_DIR)/, screenlib maclib netlogic)

include $(REP_DIR)/ports/maelstrom.port
CC_OPT = -DVERSION=\"$(VERSION)\"

CC_WARN += -Wno-write-strings

LIBS += libc libc_lwip_nic_dhcp
LIBS += stdcxx sdl sdl_mixer sdl_net libm
