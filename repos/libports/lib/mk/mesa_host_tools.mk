#
# Build shader compiler as host tool
#

MESA_PORT_DIR := $(call select_from_ports,mesa)

GLSL_COMPILER := $(BUILD_BASE_DIR)/tool/mesa/glsl/compiler

HOST_TOOLS += $(GLSL_COMPILER)

GLSL_SRC_C := $(wildcard $(MESA_PORT_DIR)/src/lib/mesa/src/glsl/pp/*.c) \
              $(wildcard $(MESA_PORT_DIR)/src/lib/mesa/src/glsl/cl/*.c) \
              $(MESA_PORT_DIR)/src/lib/mesa/src/glsl/apps/compile.c

GLSL_CFLAGS = -I$(MESA_PORT_DIR)/src/lib/mesa/src/glsl/pp \
              -I$(MESA_PORT_DIR)/src/lib/mesa/src/glsl/cl

$(GLSL_COMPILER): $(GLSL_SRC_C)
	$(MSG_BUILD)tool/mesa/glsl/compiler
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)$(HOST_CC) $(GLSL_CFLAGS) $(GLSL_SRC_C) -o $@
