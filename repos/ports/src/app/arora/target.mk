ARORA = arora-0.11.0

QT_TMPL_DIR = $(call select_from_repositories,src/app/qt5/tmpl)

ifneq ($(QT_TMPL_DIR),)
LIBS += qt5_printsupport qt5_qpluginwidget qt5_qnitpickerviewwidget
else
REQUIRES += qt5
endif

include $(QT_TMPL_DIR)/target_defaults.inc

HEADERS_FILTER_OUT = \
  adblockschemeaccesshandler.h \
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

SRC_CC += arora_component.cc

LIBS += libm

RESOURCES += demo_html.qrc

#
# Prevent contrib code from causing warnings with our tool chain
#
CC_WARN += -Wno-unused-but-set-variable

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

ARORA_PORT_DIR := $(call select_from_ports,arora)
vpath % $(ARORA_PORT_DIR)/src/app/arora/src
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/adblock
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/bookmarks
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/bookmarks/xbel
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/network
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/network/cookiejar
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/network/cookiejar/networkcookiejar
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/data
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/data/graphics
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/data/searchengines
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/history
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/htmls
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/locationbar
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/opensearch
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/qwebplugins
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/qwebplugins/clicktoflash
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/qwebplugins/nitpicker
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/useragent
vpath % $(ARORA_PORT_DIR)/src/app/arora/src/utils

include $(QT_TMPL_DIR)/target_final.inc

CC_CXX_WARN_STRICT =
