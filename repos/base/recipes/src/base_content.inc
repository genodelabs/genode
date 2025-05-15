#
# Common content rules shared among all base-<kernel> source archives
#

content: src/include src/core src/lib src/timer lib/mk LICENSE

src/include src/core src/lib src/timer lib/mk:
	mkdir -p $@
	cp -r $(GENODE_DIR)/repos/base/$@/* $@
	cp -r $(REP_DIR)/$@/* $@

# content needed for os/backtrace.h
MIRROR_FROM_OS += include/os/backtrace.h include/util/formatted_output.h \
                  $(foreach A, x86_32 x86_64 arm arm_64, \
                     include/spec/$A/os/for_each_return_address.h)

content: $(MIRROR_FROM_OS)
$(MIRROR_FROM_OS):
	mkdir -p $(dir $@)
	cp $(GENODE_DIR)/repos/os/$@ $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

content: etc/specs.conf

etc/specs.conf:
	$(mirror_from_rep_dir)
