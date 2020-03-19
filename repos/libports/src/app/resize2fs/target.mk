TARGET := resize2fs
LIBS   := posix e2fsprogs

E2FS_DIR := $(addsuffix /src/lib/e2fsprogs, $(call select_from_ports,e2fsprogs-lib))

SRC_C := $(addprefix resize/, extent.c resize2fs.c main.c online.c \
                              resource_track.c sim_progress.c)
vpath %.c $(E2FS_DIR)

CC_CXX_WARN_STRICT =
