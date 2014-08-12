#
# Build the Qt5 host tools
#

include $(REP_DIR)/lib/mk/qt5_version.inc

QT5_PORT_DIR := $(call select_from_ports,qt5)
QT5_CONTRIB_DIR := $(QT5_PORT_DIR)/src/lib/qt5/$(QT5)

QT5_TOOL_DIR := $(BUILD_BASE_DIR)/tool/qt5
QMAKE_DIR    := $(QT5_TOOL_DIR)/qmake
MOC_DIR      := $(QT5_TOOL_DIR)/moc
RCC_DIR      := $(QT5_TOOL_DIR)/rcc
UIC_DIR      := $(QT5_TOOL_DIR)/uic

HOST_TOOLS += $(QMAKE_DIR)/bin/qmake $(MOC_DIR)/moc $(RCC_DIR)/rcc $(UIC_DIR)/uic

$(QMAKE_DIR)/bin/qmake:
	$(VERBOSE)mkdir -p $(QMAKE_DIR)/bin
	$(VERBOSE)QT5_CONTRIB_DIR=$(QT5_CONTRIB_DIR) $(MAKE) -C $(QMAKE_DIR) -f $(REP_DIR)/lib/mk/qt5_host_tools_qmake.inc

# parallel build failed sporadically for unknown reason, so building of the tools is serialized for now

$(MOC_DIR)/moc: $(QMAKE_DIR)/bin/qmake
	$(VERBOSE)QT5_CONTRIB_DIR=$(QT5_CONTRIB_DIR) $(MAKE) -C $(QT5_TOOL_DIR) -f $(REP_DIR)/lib/mk/qt5_host_tools.inc moc/moc

$(RCC_DIR)/rcc: $(QMAKE_DIR)/bin/qmake $(MOC_DIR)/moc
	$(VERBOSE)QT5_CONTRIB_DIR=$(QT5_CONTRIB_DIR) $(MAKE) -C $(QT5_TOOL_DIR) -f $(REP_DIR)/lib/mk/qt5_host_tools.inc rcc/rcc

$(UIC_DIR)/uic: $(QMAKE_DIR)/bin/qmake $(RCC_DIR)/rcc
	$(VERBOSE)QT5_CONTRIB_DIR=$(QT5_CONTRIB_DIR) $(MAKE) -C $(QT5_TOOL_DIR) -f $(REP_DIR)/lib/mk/qt5_host_tools.inc uic/uic
