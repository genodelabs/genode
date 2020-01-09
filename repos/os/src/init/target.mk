TARGET   = init
SRC_CC   = main.cc
LIBS     = base
INC_DIR += $(PRG_DIR)

CONFIG_XSD = config.xsd

# statically link sandbox library to avoid dependency from sandbox.lib.so
SRC_CC  += library.cc child.cc server.cc
INC_DIR += $(REP_DIR)/src/lib/sandbox
vpath %.cc $(REP_DIR)/src/lib/sandbox
