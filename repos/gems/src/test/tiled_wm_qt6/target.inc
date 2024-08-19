QT6_PORT_LIBS = libQt6Core libQt6Gui libQt6Widgets

LIBS = qt6_qmake base libc libm mesa stdcxx qoost

QT6_COMPONENT_LIB_SO =

QT6_GENODE_LIBS_APP += ld.lib.so
qmake_prepared.tag: $(addprefix build_dependencies/lib/,$(QT6_GENODE_LIBS_APP))

INC_DIR += $(PRG_DIR)/..

#
# We need Qt headers in a local directory for MOC to work correctly
#

# icon.h from the qoost API package
ICON_H = $(call select_from_repositories,include/qoost/icon.h)

# fall-back to the qoost port directory if that fails
ifeq ($(ICON_H),)
ICON_H := $(call select_from_ports,qoost)/include/qoost/icon.h
endif

icon.h: $(ICON_H)
	$(VERBOSE)cp $(ICON_H) .
