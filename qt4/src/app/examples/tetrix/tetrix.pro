QT       += script
CONFIG   += uitools
FORMS     = tetrixwindow.ui
RESOURCES = tetrix.qrc
HEADERS   = tetrixboard.h

# install
target.path   = $$[QT_INSTALL_EXAMPLES]/script/tetrix
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS tetrix.pro *.js
sources.path  = $$[QT_INSTALL_EXAMPLES]/script/tetrix
INSTALLS     += target sources
