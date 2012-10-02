include $(PRG_DIR)/../target.inc

REQUIRES += arm
SRC_CC   += platform_arm.cc

vpath io_port_session_component.cc $(GEN_CORE_DIR)/arm
vpath platform_services.cc         $(GEN_CORE_DIR)

