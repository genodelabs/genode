SHARED_LIB = yes
DIR        = $(REP_DIR)/src/lib/ldso

include $(BASE_DIR)/mk/base-libs.mk

LIBS         = $(BASE_LIBS)
SRC_CC       = main.cc test.cc exception.cc file.cc dependency.cc debug.cc \
               shared_object.cc
SRC_S        = jmp_slot.s
INC_DIR     += $(DIR)/include
LD_OPT      += -Bsymbolic-functions --version-script=$(DIR)/symbol.map
ENTRY_POINT  = _start

ifneq ($(filter linux, $(SPECS)),)
LD_OPT += -T$(call select_from_repositories,src/platform/context_area.nostdlib.ld)
else
LD_OPT += -T$(DIR)/linker.ld
endif

vpath %.cc $(DIR)

# vi:ft=make

