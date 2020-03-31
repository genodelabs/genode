include $(call select_from_repositories,src/app/qt5/tmpl/target_defaults.inc)

include $(call select_from_repositories,src/app/qt5/tmpl/target_final.inc)

main_window.o: main_window.moc

LIBS += qoost qt5_gui qt5_widgets qt5_core libc base

CC_CXX_WARN_STRICT =
