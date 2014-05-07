# prevent import file to be included twice, for example via import-qt_gui.mk
# and import-qt_core.mk

ifeq ($(QT4_IMPORTED),)
QT4_IMPORTED = true

REQUIRES += qt4_deprecated

# identify the qt4 repository by searching for a file that is unique for qt4
QT4_REP_DIR := $(call select_from_repositories,lib/import/import-qt4.inc)
QT4_REP_DIR := $(realpath $(dir $(QT4_REP_DIR))../..)

include $(QT4_REP_DIR)/lib/mk/qt_version.inc

QT4_INC_DIR := $(QT4_REP_DIR)/src/lib/qt4/mkspecs/qws/genode-generic-g++ \
               $(QT4_REP_DIR)/include/qt4 \
               $(QT4_REP_DIR)/contrib/$(QT4)/include \
               $(QT4_REP_DIR)/include/qt4/QtCore \
               $(QT4_REP_DIR)/contrib/$(QT4)/include/QtCore \
               $(QT4_REP_DIR)/include/qt4/QtGui \
               $(QT4_REP_DIR)/contrib/$(QT4)/include/QtGui \
               $(QT4_REP_DIR)/include/qt4/QtNetwork \
               $(QT4_REP_DIR)/contrib/$(QT4)/include/QtNetwork \
               $(QT4_REP_DIR)/include/qt4/QtScript \
               $(QT4_REP_DIR)/contrib/$(QT4)/include/QtScript \
               $(QT4_REP_DIR)/include/qt4/QtScriptTools \
               $(QT4_REP_DIR)/contrib/$(QT4)/include/QtScriptTools \
               $(QT4_REP_DIR)/include/qt4/QtSvg \
               $(QT4_REP_DIR)/contrib/$(QT4)/include/QtSvg \
               $(QT4_REP_DIR)/include/qt4/QtUiTools \
               $(QT4_REP_DIR)/contrib/$(QT4)/include/QtUiTools \
               $(QT4_REP_DIR)/include/qt4/QtWebKit \
               $(QT4_REP_DIR)/contrib/$(QT4)/include/QtWebKit \
               $(QT4_REP_DIR)/include/qt4/QtXml \
               $(QT4_REP_DIR)/contrib/$(QT4)/include/QtXml

INC_DIR += $(QT4_INC_DIR)

# extracted from qt.prf
QT_DEFINES += -DQT_STATICPLUGIN

QT_DEFINES += -D__GENODE__

#
# Prevent inclusion of non-existent 'features.h' from 'bits/os_defines.h'
# header that comes with the Codesourcery ARM tool chain.
#
QT_DEFINES += -D_GLIBCXX_OS_DEFINES

#
# When using the Codesourcery tool chain for ARM, the compiler provides a
# built-in definition for '__linux__', which is obviously wrong when using the
# compiler for Genode. Unfortunately, Webkit tests for this definition in
# 'JavaScriptCore/wtf/Platform.h'. To prevent webkit from drawing wrong
# conclusions, we explicitly undefine '__linux__'.
#
QT_DEFINES += -U__linux__

CC_CXX_OPT += $(QT_DEFINES)

SOURCES_FILTERED = $(filter-out $(SOURCES_FILTER_OUT), $(SOURCES))
HEADERS_FILTERED = $(filter-out $(HEADERS_FILTER_OUT), $(HEADERS))

# add sources defined in qmake project files
SRC_CC += $(SOURCES_FILTERED)

# handle moc-headers, resources and ui descriptions
$(SRC_CC:.cpp=.o): $(addprefix ui_,$(FORMS:.ui=.h))

SRC_CC_QT_GENERATED = $(addprefix moc_,$(HEADERS_FILTERED:.h=.cpp)) \
                      $(addprefix qrc_,$(RESOURCES:.qrc=.cpp))

.SECONDARY: $(SRC_CC_QT_GENERATED)
SRC_CC += $(SRC_CC_QT_GENERATED)

#
# Locations of moc, rcc, and uic binaries
#
# These binaries are created by calling 'make' in the 'tool' directory, which
# should have happened before starting to build Qt4 applications.
#
MOC = $(QT4_REP_DIR)/tool/moc/moc
RCC = $(QT4_REP_DIR)/tool/rcc/rcc
UIC = $(QT4_REP_DIR)/tool/uic/uic

$(MOC) $(RCC) $(UIC):
	@echo
	@echo "Attempting to build Qt4 application without having built the Qt4 tools."
	@echo "Please execute 'make prepare' in the root of the qt4 repository."
	@echo
	@false

# moc rules
moc_%.cpp: %.h $(MOC)
	$(MSG_CONVERT)$@
	$(VERBOSE) $(MOC) $(QT_DEFINES) $(addprefix -I,$(QT4_INC_DIR)) $< -o $@

%.moc: %.cpp $(MOC)
	$(MSG_CONVERT)$@
	$(VERBOSE) $(MOC) $(QT_DEFINES) $(addprefix -I,$(QT4_INC_DIR)) $< -o $@

# rcc rule
qrc_%.cpp: %.qrc $(RCC)
	$(MSG_CONVERT)$@
	$(VERBOSE) $(RCC) -name $(basename $(notdir $<)) $< -o $@

# uic rule
ui_%.h: %.ui $(UIC)
	$(MSG_CONVERT)$@
	$(VERBOSE) $(UIC) $< -o $@

# add include dirs for Qt4-specific genode addons
INC_DIR += $(QT4_REP_DIR)/include/genode

# add C++ include dirs and libs
#
# We cannot just extend the 'LIBS' variable here because 'import-*.mk' are
# included (in 'base/mk/lib.mk') by iterating through the elements of the
# 'LIBS' variable. Hence, we also need to manually import the stdlib snippet.
#
LIBS += stdcxx
include $(call select_from_repositories,lib/import/import-stdcxx.mk)

# custom main() thread stack size support via main() wrapper
ifeq ($(findstring -DQT_MAIN_STACK_SIZE, $(CC_CXX_OPT)), -DQT_MAIN_STACK_SIZE)
CC_CXX_OPT += -Dmain=qt_main
SRC_CC += qt_main.cc
vpath qt_main.cc $(QT4_REP_DIR)/src/lib/qt_main
endif

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
