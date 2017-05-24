IMPORT_QT5_INC=$(call select_from_repositories,lib/import/import-qt5.inc)

include $(IMPORT_QT5_INC)

#
# Public QtGui headers include OpenGL headers
#
# We cannot just extend the 'LIBS' variable here because 'import-*.mk' are
# included (in 'base/mk/lib.mk') by iterating through the elements of the
# 'LIBS' variable. Hence, we also need to manually import the stdcxx snippet.
#
LIBS += gallium
include $(call select_from_repositories,lib/import/import-gallium.mk)

QT5_INC_DIR += $(QT5_CONTRIB_DIR)/qtbase/include/QtGui
