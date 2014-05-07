TARGET = bootstrap
PKGS   = drivers-frst/include drivers-frst/of drivers-frst/uart bootstrap
LIBS   = l4re_support

include $(REP_DIR)/mk/l4_pkg.mk
