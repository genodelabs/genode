GMP      = gmp-4.3.2
GMP_TBZ2 = $(GMP).tar.bz2
GMP_URL  = ftp://ftp.gmplib.org/pub/$(GMP)/$(GMP_TBZ2)

#
# Interface to top-level prepare Makefile
#
PORTS += $(GMP)

GMP_INCLUDES  = include/gmp/gmp-impl.h
GMP_INCLUDES += include/gmp/arm/gmp-mparam.h
GMP_INCLUDES += include/gmp/x86_32/gmp-mparam.h
GMP_INCLUDES += include/gmp/x86_64/gmp-mparam.h
GMP_SRC      += src/lib/gmp/mpn/asm-defs.m4 \
                src/lib/gmp/mpn/arm/hamdist.c \
                src/lib/gmp/mpn/arm/popcount.c \
                src/lib/gmp/mpn/x86_32/add_n.asm \
                src/lib/gmp/mpn/x86_32/sub_n.asm \
                src/lib/gmp/mpn/x86_64/add_n.asm \
                src/lib/gmp/mpn/x86_64/sub_n.asm \
                src/lib/gmp/mpn/x86_64/hamdist.asm \
                src/lib/gmp/mpn/x86_64/popcount.asm

prepare-gmp: $(CONTRIB_DIR)/$(GMP) $(GMP_INCLUDES) $(GMP_SRC)

$(CONTRIB_DIR)/$(GMP): clean-gmp

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(GMP_TBZ2):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(GMP_URL) && touch $@

$(CONTRIB_DIR)/$(GMP): $(DOWNLOAD_DIR)/$(GMP_TBZ2)
	$(VERBOSE)tar xfj $< -C $(CONTRIB_DIR) && touch $@

include/gmp/gmp-impl.h:
	$(VERBOSE)ln -sf ../../$(CONTRIB_DIR)/$(GMP)/gmp-impl.h $@

include/gmp/arm/gmp-mparam.h: $(CONTRIB_DIR)/$(GMP)/mpn/arm/gmp-mparam.h
	$(VERBOSE)mkdir -p include/gmp/arm
	$(VERBOSE)ln -sf ../../../$< $@

include/gmp/x86_32/gmp-mparam.h: $(CONTRIB_DIR)/$(GMP)/mpn/x86/pentium/gmp-mparam.h
	$(VERBOSE)mkdir -p include/gmp/x86_32
	$(VERBOSE)ln -sf ../../../$< $@

include/gmp/x86_64/gmp-mparam.h: $(CONTRIB_DIR)/$(GMP)/mpn/x86_64/gmp-mparam.h
	$(VERBOSE)mkdir -p include/gmp/x86_64
	$(VERBOSE)ln -sf ../../../$< $@

src/lib/gmp/mpn/asm-defs.m4: $(CONTRIB_DIR)/$(GMP)/mpn/asm-defs.m4
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../../../$< $@

src/lib/gmp/mpn/arm/hamdist.c: $(CONTRIB_DIR)/$(GMP)/mpn/generic/popham.c
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../../../../$< $@

src/lib/gmp/mpn/arm/popcount.c: $(CONTRIB_DIR)/$(GMP)/mpn/generic/popham.c
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../../../../$< $@

src/lib/gmp/mpn/x86_32/add_n.asm: $(CONTRIB_DIR)/$(GMP)/mpn/x86/pentium/aors_n.asm
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../../../../$< $@

src/lib/gmp/mpn/x86_32/sub_n.asm: $(CONTRIB_DIR)/$(GMP)/mpn/x86/pentium/aors_n.asm
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../../../../$< $@

src/lib/gmp/mpn/x86_64/add_n.asm: $(CONTRIB_DIR)/$(GMP)/mpn/x86_64/aors_n.asm
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../../../../$< $@

src/lib/gmp/mpn/x86_64/sub_n.asm: $(CONTRIB_DIR)/$(GMP)/mpn/x86_64/aors_n.asm
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../../../../$< $@

src/lib/gmp/mpn/x86_64/hamdist.asm: $(CONTRIB_DIR)/$(GMP)/mpn/x86_64/popham.asm
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../../../../$< $@

src/lib/gmp/mpn/x86_64/popcount.asm: $(CONTRIB_DIR)/$(GMP)/mpn/x86_64/popham.asm
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../../../../$< $@

clean-gmp:
	$(VERBOSE)rm -f  $(GMP_INCLUDES)
	$(VERBOSE)rm -f  $(GMP_SRC)
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(GMP)
