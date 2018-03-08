TARGET = test-text_painter
SRC_CC = main.cc
LIBS   = base ttf_font vfs

SRC_BIN += droidsansb10.tff default.tff

vpath %.tff $(call select_from_repositories,src/app/scout/data)
vpath %.tff $(call select_from_repositories,src/server/nitpicker)
