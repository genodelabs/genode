#
# Host tools required to build rump
#
include $(REP_DIR)/lib/mk/rump_common.inc

HOST_CC   ?= gcc
HOST_YACC ?= bison
HOST_LEX  ?= flex
COMPAT_DEFS = -include $(RUMP_PORT_DIR)/src/tools/compat/compat_defs.h


#
# nbconfig
#

RUMP_TOOL = $(BUILD_BASE_DIR)/var/libcache/rump_tools

HOST_TARGET = $(RUMP_TOOL)/bin/nbconfig

HOST_SRC_C = files.c \
             hash.c \
             lint.c \
             main.c \
             mkdevsw.c \
             mkheaders.c \
             mkioconf.c \
             mkmakefile.c \
             mkswap.c \
             pack.c \
             sem.c \
             util.c \
             gram.c \
             lex.yy.c \
             efun.c \
             strlcat.c \
             strlcpy.c \
             unvis.c \
             vis.c \
             crc.c

HOST_TOOLS += $(HOST_TARGET)

HOST_D_OPT = $(addprefix -D,HAVE_DIRFD=1 \
                            HAVE_FLOCK=1 \
                            HAVE_SOCKLEN_T=1 \
                            HAVE_INTTYPES_H=1 \
                            HAVE_DECL_HTOBE16=1 \
                            HAVE_DECL_HTOBE32=1 \
                            HAVE_DECL_HTOBE64=1 \
                            HAVE_DECL_HTOLE16=1 \
                            HAVE_DECL_HTOLE32=1 \
                            HAVE_DECL_HTOLE64=1 \
                            HAVE_DECL_BE16TOH=1 \
                            HAVE_DECL_BE32TOH=1 \
                            HAVE_DECL_BE64TOH=1 \
                            HAVE_DECL_LE16TOH=1 \
                            HAVE_DECL_LE32TOH=1 \
                            HAVE_DECL_LE64TOH=1 \
                            HAVE_NBTOOL_CONFIG_H=1 \
                            HAVE_ISBLANK=1 \
                            MAKE_BOOTSTRAP)

HOST_INC_DIR = $(addprefix -I,$(RUMP_TOOL) \
                              $(RUMP_PORT_DIR)/src/usr.bin/cksum \
                              $(RUMP_PORT_DIR)/src/tools/compat \
                              $(RUMP_PORT_DIR)/src/usr.bin/config)

HOST_CC_OPT = $(COMPAT_DEFS) $(HOST_D_OPT) $(HOST_INC_DIR)

HOST_OBJ = $(HOST_SRC_C:%.c=$(RUMP_TOOL)/%.o)

$(RUMP_TOOL)/util.h:
	$(VERBOSE_MK)touch $@

$(RUMP_TOOL)/nbtool_config.h: $(RUMP_TOOL)/util.h
	$(VERBOSE_MK)touch $@

$(RUMP_TOOL)/gram.c: $(RUMP_TOOL)/nbtool_config.h
	$(VERBOSE)$(HOST_YACC) -d -o $@ $(RUMP_PORT_DIR)/src/usr.bin/config/gram.y

$(RUMP_TOOL)/lex.yy.c: $(RUMP_TOOL)/gram.c
	$(VERBOSE)$(HOST_LEX) -o $@ $(RUMP_PORT_DIR)/src/usr.bin/config/scan.l

$(HOST_OBJ) : $(RUMP_TOOL)/lex.yy.c

$(HOST_TARGET): $(HOST_OBJ)
	$(VERBOSE_MK)mkdir -p $(RUMP_TOOL)/bin
	$(VERBOSE)$(HOST_CC) -lrt -lz -o $@ $(HOST_OBJ)

$(RUMP_TOOL)/%.o: %.c
	$(VERBOSE)$(HOST_CC) $(HOST_CC_OPT) -o $@ -c $<

vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libc/string
vpath %.c $(RUMP_PORT_DIR)/src/lib/libutil
vpath %.c $(RUMP_PORT_DIR)/src/lib/libc/gen
vpath %.c $(RUMP_PORT_DIR)/src/usr.bin/config
vpath %.c $(RUMP_PORT_DIR)/src/usr.bin/cksum
vpath %.c $(RUMP_TOOL)

CC_CXX_WARN_STRICT =
