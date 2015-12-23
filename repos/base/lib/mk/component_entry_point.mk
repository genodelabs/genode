#
# Component entry point (a trampoline for component startup from ldso)
#

SRC_CC = component_entry_point.cc component_construct.cc

vpath %.cc $(REP_DIR)/src/lib/startup
