include $(REP_DIR)/lib/mk/rump.inc

LIBS += rump

RUMP_LIBS = librumpdev.a         \
            librumpdev_cgd.a     \
            librumpdev_disk.a    \
            librumpdev_rnd.a     \
            librumpkern_crypto.a \
            librumpvfs.a

ARCHIVE += $(addprefix $(RUMP_LIB)/,$(RUMP_LIBS))
