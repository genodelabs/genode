ARORA = arora-0.11.0

# identify the qt4 repository by searching for a file that is unique for qt4
QT4_REP_DIR := $(call select_from_repositories,lib/import/import-qt4.inc)

ifeq ($(QT4_REP_DIR),)
REQUIRES += qt4
endif

QT4_REP_DIR := $(realpath $(dir $(QT4_REP_DIR))../..)

-include $(QT4_REP_DIR)/src/app/tmpl/target_defaults.inc

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

QT_MAIN_STACK_SIZE = 768*1024

LIBS += libc_lwip libc_lwip_nic_dhcp qpluginwidget

RESOURCES += demo_html.qrc

#
# Prevent contrib code from causing warnings with our toolchain compiler
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

-include $(QT4_REP_DIR)/src/app/tmpl/target_final.inc
