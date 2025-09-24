#
# Steps used for generating 'public.tar'
#

default: clean public.tar

TEST_VERSION := 2025-09-24
TEST_KEYID   := A01E82934A543906BB0220A415109A413A2E45E2

ARCHIVES := test1/raw/good/$(TEST_VERSION).tar.xz \
            test1/image/os-pc-$(TEST_VERSION).tar.xz \
            test1/index/25.09.xz \
            test2/raw/tamper/$(TEST_VERSION).tar.xz \
            test3/raw/corrupted/$(TEST_VERSION).tar.xz

SIGNATURES := $(addsuffix .sig,$(ARCHIVES))

public.tar: $(ARCHIVES) $(SIGNATURES)
	tar cf $@ $^

%.xz.sig: %.xz
	gpg --detach-sign --digest-algo SHA256 --local-user $(TEST_KEYID) $<

test1/raw/good/$(TEST_VERSION).tar.xz:
	rm -rf $(dir $@)
	mkdir -p $(@:.tar.xz=)
	echo "content expected to be installed" > $(@:.tar.xz=)/message
	cd $(dir $@); tar cf - $(TEST_VERSION) | xz > $(notdir $@)

test1/image/os-pc-$(TEST_VERSION).tar.xz:
	rm -rf $(dir $@)
	mkdir -p $(@:.tar.xz=)
	echo "system image expected to be installed" > $(@:.tar.xz=)/message
	cd $(dir $@); tar cf - os-pc-$(TEST_VERSION) | xz > $(notdir $@)

test1/index/25.09.xz:
	rm -rf $(dir $@)
	mkdir -p $(dir $@)
	echo "index expected to be installed" > $(@:.xz=)
	xz $(@:.xz=)

test2/raw/tamper/$(TEST_VERSION).tar.xz:
	rm -rf $(dir $@)
	mkdir -p $(dir $@)/1980-03-04
	echo "tampered with old version" > $(dir $@)/1980-03-04/message
	cd $(dir $@); tar cf - 1980-03-04 | xz > $(notdir $@)

test3/raw/corrupted/$(TEST_VERSION).tar.xz:
	rm -rf $(dir $@)
	mkdir -p $(@:.tar.xz=)
	echo "content should never be installed" > $(@:.tar.xz=)/message
	seq -s "," 100 > $(@:.tar.xz=)/truncated
	# truncate tar archive to let extract stumble after having extracted 'message'
	cd $(dir $@); tar cf - $(TEST_VERSION) | head -c 3000 | xz > $(notdir $@)

clean:
	rm -rf test1 test2 test3 public.tar
