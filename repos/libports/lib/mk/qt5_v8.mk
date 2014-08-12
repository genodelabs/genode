include $(REP_DIR)/lib/import/import-qt5_v8.mk

SHARED_LIB = yes

#
# Generated files
#

all: $(REP_DIR)/src/lib/qt5/qtjsbackend/generated/generated.tag

V8_DIR = $(QT5_CONTRIB_DIR)/qtjsbackend/src/v8/../3rdparty/v8

$(REP_DIR)/src/lib/qt5/qtjsbackend/generated/generated.tag:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)python $(V8_DIR)/tools/js2c.py $(dir $@)/experimental-libraries.cpp EXPERIMENTAL off $(V8_DIR)/src/macros.py $(V8_DIR)/src/proxy.js
	$(VERBOSE)python $(V8_DIR)/tools/js2c.py $(dir $@)/libraries.cpp CORE off $(V8_DIR)/src/macros.py $(V8_DIR)/src/runtime.js $(V8_DIR)/src/v8natives.js $(V8_DIR)/src/array.js $(V8_DIR)/src/string.js $(V8_DIR)/src/uri.js $(V8_DIR)/src/math.js $(V8_DIR)/src/messages.js $(V8_DIR)/src/apinatives.js $(V8_DIR)/src/date.js $(V8_DIR)/src/regexp.js $(V8_DIR)/src/json.js $(V8_DIR)/src/liveedit-debugger.js $(V8_DIR)/src/mirror-debugger.js $(V8_DIR)/src/debug-debugger.js


include $(REP_DIR)/lib/mk/qt5_v8_generated.inc

#
# Qt was configured for x86_64.
# If the Genode target architecture differs, the x86_64-files need to get removed first.
#
ifneq ($(filter-out $(SPECS),x86_64),)
QT_DEFINES += -UV8_TARGET_ARCH_X64
QT_SOURCES_FILTER_OUT = \
  assembler-x64.cc \
  builtins-x64.cc \
  code-stubs-x64.cc \
  codegen-x64.cc \
  cpu-x64.cc \
  debug-x64.cc \
  deoptimizer-x64.cc \
  disasm-x64.cc \
  frames-x64.cc \
  full-codegen-x64.cc \
  ic-x64.cc \
  lithium-codegen-x64.cc \
  lithium-gap-resolver-x64.cc \
  lithium-x64.cc \
  macro-assembler-x64.cc \
  regexp-macro-assembler-x64.cc \
  stub-cache-x64.cc
ifeq ($(filter-out $(SPECS),x86_32),)
QT_DEFINES += -DV8_TARGET_ARCH_IA32
QT_SOURCES += \
  assembler-ia32.cc \
  builtins-ia32.cc \
  code-stubs-ia32.cc \
  codegen-ia32.cc \
  cpu-ia32.cc \
  debug-ia32.cc \
  deoptimizer-ia32.cc \
  disasm-ia32.cc \
  frames-ia32.cc \
  full-codegen-ia32.cc \
  ic-ia32.cc \
  lithium-codegen-ia32.cc \
  lithium-gap-resolver-ia32.cc \
  lithium-ia32.cc \
  macro-assembler-ia32.cc \
  regexp-macro-assembler-ia32.cc \
  stub-cache-ia32.cc
QT_VPATH += qtjsbackend/src/3rdparty/v8/src/ia32
else
ifeq ($(filter-out $(SPECS),arm),)
QT_DEFINES += -DV8_TARGET_ARCH_ARM
QT_SOURCES += \
  assembler-arm.cc \
  builtins-arm.cc \
  code-stubs-arm.cc \
  codegen-arm.cc \
  constants-arm.cc \
  cpu-arm.cc \
  debug-arm.cc \
  deoptimizer-arm.cc \
  disasm-arm.cc \
  frames-arm.cc \
  full-codegen-arm.cc \
  ic-arm.cc \
  lithium-codegen-arm.cc \
  lithium-gap-resolver-arm.cc \
  lithium-arm.cc \
  macro-assembler-arm.cc \
  regexp-macro-assembler-arm.cc \
  stub-cache-arm.cc
QT_VPATH += qtjsbackend/src/3rdparty/v8/src/arm
endif
endif
endif

QT_VPATH += qtjsbackend/generated

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_network
