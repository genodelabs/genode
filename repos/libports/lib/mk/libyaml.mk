include $(REP_DIR)/lib/import/import-libyaml.mk

YAML_SRC_DIR := $(YAML_PORT_DIR)/src/lib/yaml/src

LIBS += libc

SRC_C += api.c reader.c scanner.c parser.c loader.c writer.c emitter.c dumper.c

CC_DEF += -DHAVE_CONFIG_H

CC_OPT += -Wno-unused-but-set-variable -Wno-unused-value

INC_DIR += $(YAML_SRC_DIR)

vpath %c $(YAML_SRC_DIR)
