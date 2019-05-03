TARGET   = exynos5_sd_card_drv
REQUIRES = arm_v7
INC_DIR  = $(call select_from_repositories,include/spec/exynos5)

include $(REP_DIR)/src/drivers/sd_card/target.inc
