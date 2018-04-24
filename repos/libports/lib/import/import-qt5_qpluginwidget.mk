QT5_INC_DIR += $(realpath $(call select_from_repositories,include/qt5/qpluginwidget)/..)

# 'qpluginwidget.h' includes 'qnitpickerviewwidget.h'
include $(call select_from_repositories,lib/import/import-qt5_qnitpickerviewwidget.mk)
