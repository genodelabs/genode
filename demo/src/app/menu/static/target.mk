include $(PRG_DIR)/../target.mk

TARGET := static_menu

#
# Additional declarations to created self-contained menu
#
SRC_BIN += default.png hover.png selected.png hselected.png

#
# Let the compiler find the local 'rom_file.h' first
#
INC_DIR += $(PRG_DIR) $(PRG_DIR)/..

vpath % $(PRG_DIR)/..
