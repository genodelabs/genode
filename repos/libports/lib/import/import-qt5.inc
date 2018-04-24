# prevent import file to be included twice, for example via import-qt5_gui.mk
# and import-qt5_core.mk

ifeq ($(QT5_IMPORTED),)
QT5_IMPORTED = true

# hook for selectively skipping qt5 builds by the buildbot
ifneq ($(INHIBIT_QT5),)
REQUIRES += removal_of_INHIBIT_QT5_env_var
endif

QT_VERSION := 5.8.0

# identify the qt5 repository by searching for a file that is unique for qt5
QT5_REP_DIR := $(call select_from_repositories,lib/import/import-qt5.inc)
QT5_REP_DIR := $(realpath $(dir $(QT5_REP_DIR))../..)

QT5_PORT_DIR := $(call select_from_ports,qt5)
QT5_CONTRIB_DIR := $(QT5_PORT_DIR)/src/lib/qt5/qt5

ifeq ($(CONTRIB_DIR),)
QT5_INC_DIR += $(call select_from_repositories,src/lib/qt5/qt5/qtbase/mkspecs/genode-g++)
else
QT5_INC_DIR += $(QT5_PORT_DIR)/include \
               $(QT5_CONTRIB_DIR)/qtbase/mkspecs/genode-g++ \
               $(QT5_REP_DIR)/include/qt5
endif

INC_DIR += $(QT5_INC_DIR)

# extracted from qt.prf
QT_DEFINES += -DQT_STATICPLUGIN

QT_DEFINES += -D__GENODE__

CC_OPT += $(QT_DEFINES)

SOURCES_FILTERED = $(filter-out $(SOURCES_FILTER_OUT), $(SOURCES))
HEADERS_FILTERED = $(filter-out $(HEADERS_FILTER_OUT), $(HEADERS))

# add sources defined in qmake project files
SRC_CC += $(SOURCES_FILTERED)

# handle moc-headers, resources and ui descriptions
$(addsuffix .o,$(basename $(SRC_CC))): $(addprefix ui_,$(FORMS:.ui=.h))

SRC_CC_QT_GENERATED = $(addprefix moc_,$(HEADERS_FILTERED:.h=.cpp)) \
                      $(addprefix qrc_,$(RESOURCES:.qrc=.cpp))

.SECONDARY: $(SRC_CC_QT_GENERATED)
SRC_CC += $(SRC_CC_QT_GENERATED)

#
# Locations of moc, rcc, and uic binaries
#
QT5_HOST_TOOL_INSTALL_LOCATION ?= /usr/local/genode-qt5
MOC                             = $(QT5_HOST_TOOL_INSTALL_LOCATION)/bin/moc
RCC                             = $(QT5_HOST_TOOL_INSTALL_LOCATION)/bin/rcc
UIC                             = $(QT5_HOST_TOOL_INSTALL_LOCATION)/bin/uic

$(MOC) $(RCC) $(UIC):
	$(error Error: could not find '$@'. The Qt5 host tools can be installed with the '<GENODE_DIR>/tool/tool_chain_qt5' script)

# moc rules
moc_%.cpp: %.h $(MOC)
	$(MSG_CONVERT)$@
	$(VERBOSE) $(MOC) $(QT_DEFINES) $(addprefix -I,$(QT5_INC_DIR)) $< -o $@

%.moc: %.cpp $(MOC)
	$(MSG_CONVERT)$@
	$(VERBOSE) $(MOC) $(QT_DEFINES) $(addprefix -I,$(QT5_INC_DIR)) $< -o $@

# rcc rule
qrc_%.cpp: %.qrc $(RCC)
	$(MSG_CONVERT)$@
	$(VERBOSE) $(RCC) -name $(basename $(notdir $<)) $< -o $@

# uic rule
ui_%.h: %.ui $(UIC)
	$(MSG_CONVERT)$@
	$(VERBOSE) $(UIC) $< -o $@

# add include dirs for QT5-specific genode addons
INC_DIR += $(QT5_REP_DIR)/include/qt5/genode

#
# add C++ include dirs and libs
#
# We cannot just extend the 'LIBS' variable here because 'import-*.mk' are
# included (in 'base/mk/lib.mk') by iterating through the elements of the
# 'LIBS' variable. Hence, we also need to manually import the stdcxx snippet.
#
LIBS += stdcxx
include $(call select_from_repositories,lib/import/import-stdcxx.mk)

# set QT_ARCH definition according to the SPECS variable
ifneq ($(filter x86_32,$(SPECS)),)
	QT_DEFINES += -DQT_ARCH_I386
endif
ifneq ($(filter x86_64,$(SPECS)),)
	QT_DEFINES += -DQT_ARCH_X86_64
endif
ifneq ($(filter arm,$(SPECS)),)
	QT_DEFINES += -DQT_ARCH_ARMV6
endif

# remove generated files in clean rules
clean cleanall: clean_rule
clean_rule:
	$(VERBOSE)$(RM) -f $(SRC_CC_QT_GENERATED)
	$(VERBOSE)$(RM) -f $(SOURCES_FILTERED:.cpp=.moc)
	$(VERBOSE)$(RM) -f $(addprefix ui_,$(FORMS:.ui=.h))
endif
