LIBC_LOCALE_DIR = $(LIBC_DIR)/lib/libc/locale

CC_OPT += -D_Thread_local=""

# strip locale support down to "C"
FILTER_OUT = \
	c16rtomb.c c32rtomb_iconv.c mbrtoc16_iconv.c mbrtoc32_iconv.c \
	setlocale.c xlocale.c setrunelocale.c \
	ascii.c big5.c euc.co gb18030.c gb2312.c gbk.c mbsinit.c mskanji.c utf8.c \

SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(LIBC_LOCALE_DIR)/*.c)))

# locale dummies
SRC_CC += nolocale.cc
CXX_OPT += -fpermissive

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_LOCALE_DIR)
vpath nolocale.cc $(REP_DIR)/src/lib/libc

CC_CXX_WARN_STRICT =
