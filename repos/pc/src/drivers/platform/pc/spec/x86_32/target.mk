TARGET   = pc_platform_drv
REQUIRES = x86_32

include $(call select_from_repositories,src/drivers/platform/target.inc)
