TARGET = test-pcg32
LIBS = libpcg_random posix

PCG_SRC_DIR = $(call select_from_ports,pcg-c)/src/lib/pcg-c

INC_DIR += $(PCG_SRC_DIR)/extras
SRC_C = check-pcg32-global.c entropy.c

vpath %.c $(PCG_SRC_DIR)/test-high
vpath %.c $(PCG_SRC_DIR)/extras

.PHONY: test-pcg32.out

$(TARGET): test-pcg32.out

test-pcg32.out: $(PCG_SRC_DIR)/test-high/expected/check-pcg32-global.out
	$(VERBOSE)cp $< $(PWD)/bin/$@
