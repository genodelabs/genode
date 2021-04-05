MIRROR_FROM_REP_DIR := lib/import/import-stdcxx.mk \
                       lib/symbols/stdcxx

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/stdcxx)

content: include/stdcxx \
         include/spec/arm/stdcxx \
         include/spec/arm_64/stdcxx \
         include/spec/x86_32/stdcxx \
         include/spec/x86_64/stdcxx

include/stdcxx:
	mkdir -p $@
	cp -r $(PORT_DIR)/include/stdcxx/* $@/
	cp -r $(REP_DIR)/include/stdcxx/bits/* $@/bits/

include/spec/arm/stdcxx:
	mkdir -p $@
	cp -r $(REP_DIR)/$@/* $@/

include/spec/arm_64/stdcxx:
	mkdir -p $@
	cp -r $(REP_DIR)/$@/* $@/

include/spec/x86_32/stdcxx:
	mkdir -p $@
	cp -r $(REP_DIR)/$@/* $@/

include/spec/x86_64/stdcxx:
	mkdir -p $@
	cp -r $(REP_DIR)/$@/* $@/

content: LICENSE

LICENSE:
	( echo "GNU GPL version 3 with runtime library exception,"; \
	  echo "see src/lib/stdcxx/doc/html/manual/license.html" ) > $@

