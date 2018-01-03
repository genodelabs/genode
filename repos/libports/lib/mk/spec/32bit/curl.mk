INC_DIR += $(call select_from_ports,curl)/include

INC_DIR += $(REP_DIR)/src/lib/curl/spec/32bit
INC_DIR += $(REP_DIR)/src/lib/curl/spec/32bit/curl

include $(REP_DIR)/lib/mk/curl.inc

CC_CXX_WARN_STRICT =
