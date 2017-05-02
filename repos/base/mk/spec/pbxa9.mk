#
# Enable peripherals of the platform
#
SPECS += pl050 pl11x ps2 pl180 lan9118 framebuffer

#
# Pull in CPU specifics
#
SPECS += cortex_a9

#
# Add device parameters to include search path
#
REP_INC_DIR += include/spec/pbxa9

include $(BASE_DIR)/mk/spec/cortex_a9.mk
