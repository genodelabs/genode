include $(call select_from_repositories,lib/import/import-qt5_angle.mk)

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt5_angle_generated.inc

QT_INCPATH += qtwebkit/Source/ThirdParty/ANGLE/generated

QT_VPATH += qtwebkit/Source/ThirdParty/ANGLE/generated

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_opengl

CC_CXX_WARN_STRICT =
