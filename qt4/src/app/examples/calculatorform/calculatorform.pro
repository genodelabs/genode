HEADERS     = calculatorform.h
FORMS       = calculatorform.ui

# install
target.path = $$[QT_INSTALL_EXAMPLES]/designer/calculatorform
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS *.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/designer/calculatorform
INSTALLS += target sources
