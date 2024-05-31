SRC_DIR = src/driver/rtc/dummy
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

CONTENT = src/driver/rtc/README src/driver/rtc/rtc.h src/driver/rtc/main.cc \
          src/driver/rtc/target.inc

content: $(CONTENT)

$(CONTENT):
	$(mirror_from_rep_dir)
