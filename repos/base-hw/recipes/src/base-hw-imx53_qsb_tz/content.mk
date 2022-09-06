CONTENT += src/core/board/imx53_qsb \
           src/bootstrap/board/imx53_qsb \
           lib/mk/spec/arm_v7/core-hw-imx53_qsb.inc \
           lib/mk/spec/arm_v7/bootstrap-hw-imx53_qsb.inc

include $(GENODE_DIR)/repos/base-hw/recipes/src/base-hw_content.inc
