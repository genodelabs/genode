include $(call select_from_repositories,src/app/qt5/tmpl/target_defaults.inc)

include $(call select_from_repositories,src/app/qt5/tmpl/target_final.inc)

CC_CXX_WARN_STRICT =

INC_DIR += $(PRG_DIR)/..

LIBS += qoost

#
# We need Qt headers in a local directory for MOC to work correctly
#

# icon.h from the qoost API package
ICON_H = $(call select_from_repositories,include/qoost/icon.h)

# fall-back to the qoost port directory if that fails
ifeq ($(ICON_H),)
ICON_H = $(call select_from_ports,qoost)/include/qoost/icon.h
endif

icon.h: $(ICON_H)
	$(VERBOSE)cp $(ICON_H) .

