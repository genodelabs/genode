#note: leave empty to disable debugging output
DEBUG =

include $(BASE_DIR)/mk/base-libs.mk
LIBS   = $(BASE_LIBS) ldso-arch

SRC_S  = rtld_start.S
SRC_C  = reloc.c rtld.c map_object.c xmalloc.c debug.c main.c  \
         ldso_types.c rtld_dummies.c platform.c
SRC_CC = stdio.cc stdlib.cc file.cc err.cc string.cc lock.cc \
         test.cc environ.cc thread.cc

INC_DIR += $(DIR)/ \
           $(DIR)/contrib \
           $(DIR)/include/libc \
           $(DIR)/include/libc_emu

#
# Change link address
#
# When LINK_ADDRESS does not match the memory address of ld.lib.so at runtime,
# GDB shows the following message when attaching to a dynamically linked Genode
# process (for example test-lwip_httpsrv):
#
# "warning: .dynamic section for ".../libcache/ld/ld.lib.so" is not at the
#  expected address (wrong library or version mismatch?)"
#
# In this case, backtraces show no symbol names or source lines.
#
ifneq ($(filter linux, $(SPECS)),)
LINK_ADDRESS = 0x50000000
else
LINK_ADDRESS = 0x30000
endif

ENTRY_POINT  = _start

D_OPTS += IN_RTLD __BSD_VISIBLE=1 LINK_ADDRESS=$(LINK_ADDRESS) $(RENAME_FUNCS)
D_OPTS += $(if $(DEBUG),DEBUG,)
D_OPTS := $(addprefix -D,$(D_OPTS))

CC_DEF  += $(D_OPTS) -fno-builtin
CXX_DEF += $(D_OPTS)
AS_OPT  += $(D_OPTS)

CC_DEF  += -Iinclude -include $(DIR)/include/libc_emu/ldso_types.h
LD_OPT  += -Bsymbolic-functions  -T$(DIR)/ldso.ld --version-script=$(DIR)/symbol.map

CC_OPT  += -Wno-unused-but-set-variable

#
# Add context area script to Linux version of linker
#
ifneq ($(filter linux, $(SPECS)),)
LD_OPT += -T$(call select_from_repositories,src/platform/context_area.nostdlib.ld)
endif

vpath %.cc $(DIR)
vpath %.c  $(DIR)/contrib
vpath %.c  $(DIR)


# vim:set ft=make:
