INC_DIR += $(call select_from_ports,curl)/include

INC_DIR += $(REP_DIR)/src/lib/curl/64bit
INC_DIR += $(REP_DIR)/src/lib/curl/64bit/curl

include $(REP_DIR)/lib/mk/curl.inc
