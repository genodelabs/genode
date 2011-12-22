MESA     = Mesa-7.8.1
MESA_DIR = ../../contrib/$(MESA)
SRC_C := $(wildcard $(MESA_DIR)/src/glsl/pp/*.c) \
         $(wildcard $(MESA_DIR)/src/glsl/cl/*.c) \
         $(MESA_DIR)/src/glsl/apps/compile.c

CFLAGS = -I$(MESA_DIR)/glsl/pp -I$(MESA_DIR)/glsl/cl

all: glsl/compiler

glsl:
	mkdir -p $@

glsl/compiler: glsl $(SRC_C)
	$(CC) $(CFLAGS) $(SRC_C) -o $@

clean:
	rm -rf glsl
