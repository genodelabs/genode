TARGET   = decorator
SRC_CC   = main.cc texture_by_id.cc default_font.h window.cc
SRC_BIN  = closer.rgba maximize.rgba minimize.rgba windowed.rgba
SRC_BIN += droidsansb10.tff
LIBS     = base config
TFF_DIR  = $(call select_from_repositories,src/app/scout/data)
INC_DIR += $(PRG_DIR)

vpath %.tff  $(TFF_DIR)
vpath %.rgba $(PRG_DIR)
