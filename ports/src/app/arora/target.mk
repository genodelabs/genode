ARORA = arora-0.11.0

# build with QtCore and QtGui support by default
# (can be overridden in the qmake project file)
QT = core gui

# find out the name of the project directory
PROJECT_DIR_NAME = $(notdir $(abspath $(PRG_DIR)))

INC_DIR += $(PRG_DIR)

###### start of editable part ######

# include the qmake project file
# -> change the filename if it differs from the project dir name

QMAKE_PROJECT_FILE = $(realpath $(PRG_DIR)/$(PROJECT_DIR_NAME).pro)

ifneq ($(strip $(QMAKE_PROJECT_FILE)),)
include $(QMAKE_PROJECT_FILE)
endif

#
# The following files don't need to be moc'ed,
# so exclude them from $(SOURCES) and $(HEADERS).
#
NO_MOC_SOURCES = aboutdialog.cpp \
                 acceptlanguagedialog.cpp \
                 adblockblockednetworkreply.cpp \
                 adblockdialog.cpp \
                 adblockmanager.cpp \
                 adblockmodel.cpp \
                 adblocknetwork.cpp \
                 adblockpage.cpp \
                 adblockrule.cpp \
                 adblockschemeaccesshandler.cpp \
                 adblocksubscription.cpp \
                 addbookmarkdialog.cpp \
                 arorawebplugin.cpp \
                 autofilldialog.cpp \
                 autofillmanager.cpp \
                 autosaver.cpp \
                 bookmarknode.cpp \
                 bookmarksdialog.cpp \
                 bookmarksmanager.cpp \
                 bookmarksmenu.cpp \
                 bookmarksmodel.cpp \
                 bookmarkstoolbar.cpp \
                 browserapplication.cpp \
                 browsermainwindow.cpp \
                 clearbutton.cpp \
                 clearprivatedata.cpp \
                 clicktoflash.cpp \
                 clicktoflashplugin.cpp \
                 cookiedialog.cpp \
                 cookieexceptionsdialog.cpp \
                 cookieexceptionsmodel.cpp \
                 cookiejar.cpp \
                 cookiemodel.cpp \
                 downloadmanager.cpp \
                 editlistview.cpp \
                 edittableview.cpp \
                 edittreeview.cpp \
                 fileaccesshandler.cpp \
                 history.cpp \
                 historycompleter.cpp \
                 historymanager.cpp \
                 languagemanager.cpp \
                 lineedit.cpp \
                 locationbar.cpp \
                 locationbarsiteicon.cpp \
                 main.cpp \
                 modelmenu.cpp \
                 modeltoolbar.cpp \
                 networkaccessmanager.cpp \
                 networkaccessmanagerproxy.cpp \
                 networkcookiejar.cpp \
                 networkdiskcache.cpp \
                 networkproxyfactory.cpp \
                 nitpickerplugin.cpp \
                 nitpickerpluginwidget.cpp \
                 opensearchdialog.cpp \
                 opensearchengine.cpp \
                 opensearchengineaction.cpp \
                 opensearchenginedelegate.cpp \
                 opensearchenginemodel.cpp \
                 opensearchmanager.cpp \
                 opensearchreader.cpp \
                 opensearchwriter.cpp \
                 plaintexteditsearch.cpp \
                 privacyindicator.cpp \
                 schemeaccesshandler.cpp \
                 searchbar.cpp \
                 searchbutton.cpp \
                 searchlineedit.cpp \
                 settings.cpp \
                 singleapplication.cpp \
                 sourcehighlighter.cpp \
                 sourceviewer.cpp \
                 squeezelabel.cpp \
                 tabbar.cpp \
                 tabwidget.cpp \
                 toolbarsearch.cpp \
                 treesortfilterproxymodel.cpp \
                 useragentmenu.cpp \
                 webactionmapper.cpp \
                 webpage.cpp \
                 webpageproxy.cpp \
                 webpluginfactory.cpp \
                 webview.cpp \
                 webviewsearch.cpp \
                 xbelreader.cpp \
                 xbelwriter.cpp

NO_MOC_HEADER =  adblockschemeaccesshandler.h \
                 adblockrule.h \
                 arorawebplugin.h \
                 bookmarknode.h \
                 clicktoflashplugin.h \
                 networkcookiejar_p.h \
                 nitpickerplugin.h \
                 networkproxyfactory.h \
                 opensearchenginedelegate.h \
                 opensearchreader.h \
                 opensearchwriter.h \
                 schemeaccesshandler.h \
                 twoleveldomains_p.h \
                 trie_p.h \
                 xbelreader.h \
                 xbelwriter.h

SOURCES := $(filter-out $(NO_MOC_SOURCES), $(SOURCES))
HEADERS := $(filter-out $(NO_MOC_HEADER), $(HEADERS))
SRC_CC   = $(NO_MOC_SOURCES)

# how to name the generated executable
# (if not already defined in the qmake project file)
# -> change if it shall not be the name of the project dir

ifndef TARGET
TARGET = $(PROJECT_DIR_NAME)
endif

CC_CXX_OPT += -DQT_MAIN_STACK_SIZE=768*1024

#
# Prevent contrib code from causing warnings with our toolchain compiler
#
CC_CXX_OPT += -Wno-unused-but-set-variable

###### end of editable part ######

LIBS += libc libc_lwip libc_lwip_nic_dhcp qpluginwidget
RESOURCES += demo_html.qrc

# static Qt plugins
ifeq ($(findstring qgif, $(QT_PLUGIN)), qgif)
LIBS += qgif
endif
ifeq ($(findstring qjpeg, $(QT_PLUGIN)), qjpeg)
LIBS += qjpeg
endif

# QtCore stuff
ifeq ($(findstring core, $(QT)), core)
QT_DEFINES += -DQT_CORE_LIB
LIBS += qt_core
endif

# QtGui stuff
ifeq ($(findstring gui, $(QT)), gui)
QT_DEFINES += -DQT_GUI_LIB
LIBS += qt_gui dejavusans
endif

# QtNetwork stuff
ifeq ($(findstring network, $(QT)), network)
LIBS += qt_network
endif

# QtScript stuff
ifeq ($(findstring script, $(QT)), script)
LIBS += qt_script
endif

# QtScriptTools stuff
ifeq ($(findstring scripttools, $(QT)), scripttools)
LIBS += qt_scripttools
endif

# QtSvg stuff
ifeq ($(findstring svg, $(QT)), svg)
LIBS += qt_svg
endif

# QtXml stuff
ifeq ($(findstring xml, $(QT)), xml)
LIBS += qt_xml
endif

# QtUiTools stuff
ifeq ($(findstring uitools, $(CONFIG)), uitools)
LIBS += qt_ui_tools
endif

# QtWebKit stuff
ifeq ($(findstring webkit, $(QT)), webkit)
LIBS += qt_webcore
endif

#endif # $(QMAKE_PROJECT_FILE)

vpath % $(PRG_DIR)
vpath % $(PRG_DIR)/adblock
vpath % $(PRG_DIR)/bookmarks
vpath % $(PRG_DIR)/bookmarks/xbel
vpath % $(PRG_DIR)/network
vpath % $(PRG_DIR)/network/cookiejar
vpath % $(PRG_DIR)/network/cookiejar/networkcookiejar
vpath % $(PRG_DIR)/data
vpath % $(PRG_DIR)/data/graphics
vpath % $(PRG_DIR)/data/searchengines
vpath % $(PRG_DIR)/history
vpath % $(PRG_DIR)/htmls
vpath % $(PRG_DIR)/locationbar
vpath % $(PRG_DIR)/opensearch
vpath % $(PRG_DIR)/qwebplugins
vpath % $(PRG_DIR)/qwebplugins/clicktoflash
vpath % $(PRG_DIR)/qwebplugins/nitpicker
vpath % $(PRG_DIR)/useragent
vpath % $(PRG_DIR)/utils

vpath % $(REP_DIR)/contrib/$(ARORA)/src
vpath % $(REP_DIR)/contrib/$(ARORA)/src/adblock
vpath % $(REP_DIR)/contrib/$(ARORA)/src/bookmarks
vpath % $(REP_DIR)/contrib/$(ARORA)/src/bookmarks/xbel
vpath % $(REP_DIR)/contrib/$(ARORA)/src/network
vpath % $(REP_DIR)/contrib/$(ARORA)/src/network/cookiejar
vpath % $(REP_DIR)/contrib/$(ARORA)/src/network/cookiejar/networkcookiejar
vpath % $(REP_DIR)/contrib/$(ARORA)/src/data
vpath % $(REP_DIR)/contrib/$(ARORA)/src/data/graphics
vpath % $(REP_DIR)/contrib/$(ARORA)/src/data/searchengines
vpath % $(REP_DIR)/contrib/$(ARORA)/src/history
vpath % $(REP_DIR)/contrib/$(ARORA)/src/htmls
vpath % $(REP_DIR)/contrib/$(ARORA)/src/locationbar
vpath % $(REP_DIR)/contrib/$(ARORA)/src/opensearch
vpath % $(REP_DIR)/contrib/$(ARORA)/src/qwebplugins
vpath % $(REP_DIR)/contrib/$(ARORA)/src/qwebplugins/clicktoflash
vpath % $(REP_DIR)/contrib/$(ARORA)/src/qwebplugins/nitpicker
vpath % $(REP_DIR)/contrib/$(ARORA)/src/useragent
vpath % $(REP_DIR)/contrib/$(ARORA)/src/utils
