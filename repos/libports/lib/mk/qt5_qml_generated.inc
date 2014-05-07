QT_DEFINES += -DQT_NO_LIBUDEV -DQT_NO_XCB -DQT_NO_XKBCOMMON -DQT_NO_URL_CAST_FROM_STRING -DQT_NO_INTEGER_EVENT_COORDINATES -DQT_BUILD_QML_LIB -DQT_BUILDING_QT -DQT_NO_CAST_TO_ASCII -DQT_ASCII_CAST_WARNINGS -DQT_MOC_COMPAT -DQT_USE_QSTRINGBUILDER -DQT_DEPRECATED_WARNINGS -DQT_DISABLE_DEPRECATED_BEFORE=0x050000 -DQT_NO_EXCEPTIONS -DQT_NO_DEBUG -DQT_NETWORK_LIB -DQT_CORE_LIB -DQT_V8_LIB -DV8_SHARED -DUSING_V8_SHARED

QT_INCPATH += \
  qtbase/include \
  qtbase/include/QtCore \
  qtbase/include/QtCore/5.1.0 \
  qtbase/include/QtCore/5.1.0/QtCore \
  qtbase/include/QtNetwork \
  qtbase/mkspecs/genode-g++ \
  qtdeclarative/include \
  qtdeclarative/include/QtQml \
  qtdeclarative/include/QtQml/5.1.0 \
  qtdeclarative/include/QtQml/5.1.0/QtQml \
  qtdeclarative/src/qml \
  qtdeclarative/src/qml/animations \
  qtjsbackend/include \
  qtjsbackend/include/QtV8 \
  qtjsbackend/include/QtV8/5.1.0 \
  qtjsbackend/include/QtV8/5.1.0/QtV8 \

QT_SOURCES += \
  qqmlchangeset.cpp \
  qqmllistaccessor.cpp \
  qqmllistcompositor.cpp \
  qqmladaptormodel.cpp \
  qqmlpropertymap.cpp \
  qqmlinstruction.cpp \
  qqmlopenmetaobject.cpp \
  qqmlvmemetaobject.cpp \
  qqmlengine.cpp \
  qqmlexpression.cpp \
  qqmlproperty.cpp \
  qqmlcomponent.cpp \
  qqmlincubator.cpp \
  qqmlcontext.cpp \
  qqmlcustomparser.cpp \
  qqmlpropertyvaluesource.cpp \
  qqmlpropertyvalueinterceptor.cpp \
  qqmlproxymetaobject.cpp \
  qqmlvme.cpp \
  qqmlcompiler.cpp \
  qqmlcompileddata.cpp \
  qqmlboundsignal.cpp \
  qqmlmetatype.cpp \
  qqmlstringconverters.cpp \
  qqmlparserstatus.cpp \
  qqmltypeloader.cpp \
  qqmlinfo.cpp \
  qqmlerror.cpp \
  qqmlscript.cpp \
  qqmlrewrite.cpp \
  qqmlvaluetype.cpp \
  qqmlaccessors.cpp \
  qqmlxmlhttprequest.cpp \
  qqmlwatcher.cpp \
  qqmlcleanup.cpp \
  qqmlpropertycache.cpp \
  qqmlnotifier.cpp \
  qqmlintegercache.cpp \
  qqmltypenotavailable.cpp \
  qqmltypenamecache.cpp \
  qqmlscriptstring.cpp \
  qqmlnetworkaccessmanagerfactory.cpp \
  qqmldirparser.cpp \
  qqmlextensionplugin.cpp \
  qqmlimport.cpp \
  qqmllist.cpp \
  qqmllocale.cpp \
  qqmlabstractexpression.cpp \
  qqmljavascriptexpression.cpp \
  qqmlabstractbinding.cpp \
  qqmlvaluetypeproxybinding.cpp \
  qqmlglobal.cpp \
  qqmlfile.cpp \
  qqmlbundle.cpp \
  qqmlmemoryprofiler.cpp \
  qqmlplatform.cpp \
  qqmlbinding.cpp \
  qqmlabstracturlinterceptor.cpp \
  qqmlapplicationengine.cpp \
  qqmljsast.cpp \
  qqmljsastvisitor.cpp \
  qqmljsengine_p.cpp \
  qqmljsgrammar.cpp \
  qqmljslexer.cpp \
  qqmljsparser.cpp \
  textwriter.cpp \
  qintrusivelist.cpp \
  qhashedstring.cpp \
  qqmlpool.cpp \
  qqmlthread.cpp \
  qqmltrace.cpp \
  qv4compiler.cpp \
  qv4ir.cpp \
  qv4irbuilder.cpp \
  qv4instruction.cpp \
  qv4bindings.cpp \
  qjsengine.cpp \
  qjsvalue.cpp \
  qjsvalueiterator.cpp \
  qv8stringwrapper.cpp \
  qv8engine.cpp \
  qv8sequencewrapper.cpp \
  qv8contextwrapper.cpp \
  qv8qobjectwrapper.cpp \
  qv8typewrapper.cpp \
  qv8listwrapper.cpp \
  qv8variantwrapper.cpp \
  qv8valuetypewrapper.cpp \
  qv8jsonwrapper.cpp \
  qv8include.cpp \
  qv8worker.cpp \
  qv8bindings.cpp \
  qv8domerrors.cpp \
  qv8sqlerrors.cpp \
  qqmlbuiltinfunctions.cpp \
  qqmldebugservice.cpp \
  qqmlprofilerservice.cpp \
  qqmldebugserver.cpp \
  qqmlinspectorservice.cpp \
  qv8debugservice.cpp \
  qv8profilerservice.cpp \
  qqmlenginedebugservice.cpp \
  qdebugmessageservice.cpp \
  qabstractanimationjob.cpp \
  qanimationgroupjob.cpp \
  qsequentialanimationgroupjob.cpp \
  qparallelanimationgroupjob.cpp \
  qpauseanimationjob.cpp \
  qqmlbind.cpp \
  qqmlconnections.cpp \
  qqmldelegatemodel.cpp \
  qqmllistmodel.cpp \
  qqmllistmodelworkeragent.cpp \
  qqmlmodelsmodule.cpp \
  qqmlobjectmodel.cpp \
  qqmltimer.cpp \
  qquickpackage.cpp \
  qquickworkerscript.cpp \
  qqmlinstantiator.cpp \
  moc_qqmlpropertymap.cpp \
  moc_qqmlglobal_p.cpp \
  moc_qqmlcomponent.cpp \
  moc_qqmlengine.cpp \
  moc_qqmlcontext.cpp \
  moc_qqmlvaluetype_p.cpp \
  moc_qqmlwatcher_p.cpp \
  moc_qqmltypenotavailable_p.cpp \
  moc_qqmlextensionplugin.cpp \
  moc_qqmllocale_p.cpp \
  moc_qqmlcomponentattached_p.cpp \
  moc_qqmlplatform_p.cpp \
  moc_qv8include_p.cpp \
  moc_qqmldebugservice_p.cpp \
  moc_qqmlinspectorservice_p.cpp \
  moc_qv8debugservice_p.cpp \
  moc_qv8profilerservice_p.cpp \
  moc_qqmlenginedebugservice_p.cpp \
  moc_qdebugmessageservice_p.cpp \
  moc_qabstractanimationjob_p.cpp \
  moc_qqmlbind_p.cpp \
  moc_qqmlconnections_p.cpp \
  moc_qqmldelegatemodel_p.cpp \
  moc_qqmldelegatemodel_p_p.cpp \
  moc_qqmllistmodel_p.cpp \
  moc_qqmllistmodel_p_p.cpp \
  moc_qqmllistmodelworkeragent_p.cpp \
  moc_qqmlobjectmodel_p.cpp \
  moc_qqmltimer_p.cpp \
  moc_qquickpackage_p.cpp \
  moc_qquickworkerscript_p.cpp

QT_VPATH += \
  qtdeclarative/src/qml/animations \
  qtdeclarative/src/qml/debugger \
  qtdeclarative/src/qml/qml \
  qtdeclarative/src/qml/qml/ftw \
  qtdeclarative/src/qml/qml/parser \
  qtdeclarative/src/qml/qml/rewriter \
  qtdeclarative/src/qml/qml/v4 \
  qtdeclarative/src/qml/qml/v8 \
  qtdeclarative/src/qml/types \
  qtdeclarative/src/qml/util \

# some source files need to be generated by moc from other source/header files before
# they get #included again by the original source file in the compiling stage

# source files generated from existing header files ('moc_%.cpp: %.h' rule in import-qt5.inc)
# extracted from 'compiler_moc_header_make_all' target

COMPILER_MOC_HEADER_MAKE_ALL_FILES = \
  moc_qqmlpropertymap.cpp \
  moc_qqmlglobal_p.cpp \
  moc_qqmlcomponent.cpp \
  moc_qqmlengine.cpp \
  moc_qqmlcontext.cpp \
  moc_qqmlexpression.cpp \
  moc_qqmlvaluetype_p.cpp \
  moc_qqmlwatcher_p.cpp \
  moc_qqmltypenotavailable_p.cpp \
  moc_qqmlextensionplugin.cpp \
  moc_qqmllocale_p.cpp \
  moc_qqmlcomponentattached_p.cpp \
  moc_qqmlplatform_p.cpp \
  moc_qqmlapplicationengine.cpp \
  moc_qjsengine.cpp \
  moc_qv8include_p.cpp \
  moc_qqmldebugservice_p.cpp \
  moc_qqmldebugserver_p.cpp \
  moc_qqmlinspectorservice_p.cpp \
  moc_qv8debugservice_p.cpp \
  moc_qv8profilerservice_p.cpp \
  moc_qqmlenginedebugservice_p.cpp \
  moc_qdebugmessageservice_p.cpp \
  moc_qabstractanimationjob_p.cpp \
  moc_qqmlbind_p.cpp \
  moc_qqmlconnections_p.cpp \
  moc_qqmldelegatemodel_p.cpp \
  moc_qqmldelegatemodel_p_p.cpp \
  moc_qqmllistmodel_p.cpp \
  moc_qqmllistmodel_p_p.cpp \
  moc_qqmllistmodelworkeragent_p.cpp \
  moc_qqmlobjectmodel_p.cpp \
  moc_qqmltimer_p.cpp \
  moc_qquickpackage_p.cpp \
  moc_qquickworkerscript_p.cpp \
  moc_qqmlinstantiator_p.cpp

# source files generated from existing source files ('%.moc: %.cpp' rule in import-qt5.inc)
# extracted from 'compiler_moc_source_make_all' rule

COMPILER_MOC_SOURCE_MAKE_ALL_FILES = \
  qqmladaptormodel.moc \
  qqmltypeloader.moc \
  qqmlxmlhttprequest.moc \
  qqmlwatcher.moc \
  qqmlfile.moc \
  qquickworkerscript.moc

