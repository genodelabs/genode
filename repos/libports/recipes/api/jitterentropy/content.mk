MIRROR_FROM_REP_DIR := lib/mk/jitterentropy.inc \
                       lib/import/import-jitterentropy.mk \
                       $(foreach SPEC,arm_v6 arm_v7 x86_32 x86_64,\
                         lib/mk/spec/$(SPEC)/jitterentropy.mk) \
                       src/lib/jitterentropy

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/jitterentropy)
MIRROR_FROM_PORT_DIR := src/lib/jitterentropy/jitterentropy-base.c \
                        include/jitterentropy/jitterentropy.h

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_PORT_DIR) LICENSE

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	( echo "The contrib sources"; \
	  echo "  include/jitterentropy/jitterentropy.h"; \
	  echo "  src/lib/jitterentropy/jitterentropy-base.c"; \
	  echo "by Stephan Mueller are licensed under a BSD-style license (please"; \
	  echo "see the corresponding copyright header in the files)." ) > $@
