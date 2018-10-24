PORT_DIR := $(call port_dir,$(REP_DIR)/ports/rust)

PORT_LIBS := libcore liblibc liballoc libcollections librustc_unicode \
             liballoc_system librand

RUST_LIBS := $(addsuffix -rust,$(PORT_LIBS)) libunwind-rust builtins-rust

MIRROR_FROM_REP_DIR := src/lib/rust-targets

content: lib/mk src/lib/rust lib/import LICENSE \
         $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/rust:
	mkdir -p $@/src
	cp -r $(addprefix $(PORT_DIR)/$@/src/,$(PORT_LIBS)) $@/src/
	cp -r $(REP_DIR)/$@/* $@
	rm -rf $@/mk/target.mk

lib/import:
	mkdir -p $@
	cp $(addprefix $(REP_DIR)/$@/,import-libcore-rust.mk) $@

lib/mk:
	mkdir -p $@
	cp $(addprefix $(REP_DIR)/$@/,$(addsuffix .mk,$(RUST_LIBS))) $@
	cp $(addprefix $(REP_DIR)/$@/,rust.inc) $@

LICENSE:
	cp $(PORT_DIR)/src/lib/rust/COPYRIGHT $@
