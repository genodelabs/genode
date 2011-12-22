TARGET   = timer
SRC_CC   = main.cc platform_timer.cc
REQUIRES = codezero
LIBS     = cxx server env alarm

INC_DIR += $(PRG_DIR)/../include $(PRG_DIR)/../include_periodic

#
# Supply dummy includes to prevent warning about missing string.h and stdio.h,
# which are included by Codezero's headers.
#
REP_INC_DIR += include/codezero/dummies

vpath main.cc $(PRG_DIR)/..
