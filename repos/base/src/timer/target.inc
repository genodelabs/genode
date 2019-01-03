SRC_CC  += main.cc
LIBS    += base
INC_DIR += $(call select_from_repositories,src/timer/include)

vpath main.cc $(dir $(call select_from_repositories,src/timer/main.cc))
