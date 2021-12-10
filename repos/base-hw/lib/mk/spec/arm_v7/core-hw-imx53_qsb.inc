REP_INC_DIR += src/core/board/imx53_qsb

SRC_CC += spec/arm/imx_epit.cc
SRC_CC += spec/arm/imx_tzic.cc

include $(call select_from_repositories,lib/mk/spec/cortex_a8/core-hw.inc)

CC_CXX_WARN_STRICT_CONVERSION =
