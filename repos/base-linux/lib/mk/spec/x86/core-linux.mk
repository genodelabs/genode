include $(REP_DIR)/lib/mk/core-linux.inc

SRC_CC += io_port_session_support.cc \
          io_port_session_component.cc

vpath io_port_session_support.cc   $(GEN_CORE_DIR)/spec/x86
vpath io_port_session_component.cc $(REP_DIR)/src/core/spec/$(BOARD)
