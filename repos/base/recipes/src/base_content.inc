#
# Common content rules shared among all base-<kernel> source archives
#

content: src/include src/core src/lib src/drivers/timer lib/mk LICENSE

src/include src/core src/lib lib/mk:
	mkdir -p $@
	cp -r $(GENODE_DIR)/repos/base/$@/* $@
	cp -r $(REP_DIR)/$@/* $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

content: etc/specs.conf

etc/specs.conf:
	$(mirror_from_rep_dir)

MIRRORED_FROM_OS := $(addprefix lib/mk/,timeout.mk alarm.mk config.mk timed_semaphore.mk)\
                    $(addprefix src/lib/,timeout alarm config timed_semaphore) \
                    $(addprefix include/os/,timeout.h alarm.h config.h timed_semaphore.h \
                                            time_source.h) \
                    include/timer_session

content: $(MIRRORED_FROM_OS)

$(MIRRORED_FROM_OS): lib/mk

$(MIRRORED_FROM_OS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $(dir $@)
