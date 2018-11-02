MIRROR_FROM_REP_DIR := $(addprefix lib/mk/,\
	test-ldso_lib_dl.mk \
	test-ldso_lib_1.mk \
	test-ldso_lib_2.mk )

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	mkdir -p $(dir $@)
	cp -r $(REP_DIR)/$@ $@

SRC_DIR = src/test/ldso

include $(GENODE_DIR)/repos/base/recipes/src/content.inc
