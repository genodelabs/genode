include $(REP_DIR)/lib/import/import-qt5_qml.mk

SHARED_LIB = yes

ifneq ($(call select_from_ports,qt5),)
all: $(QT5_PORT_DIR)/src/lib/qt5/qtdeclarative/src/3rdparty/masm/generated.tag
endif

# make the 'HOST_TOOLS' variable known
include $(REP_DIR)/lib/mk/qt5_host_tools.mk

$(QT5_PORT_DIR)/src/lib/qt5/qtdeclarative/src/3rdparty/masm/generated.tag: $(HOST_TOOLS)

	$(VERBOSE)mkdir -p $(dir $@)

	python $(QT5_CONTRIB_DIR)/qtdeclarative/src/3rdparty/masm/create_regex_tables > $(dir $@)/RegExpJitTables.h

	$(VERBOSE)touch $@

include $(REP_DIR)/lib/mk/qt5_qml_generated.inc

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_network qt5_core libc
