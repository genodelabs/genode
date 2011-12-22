GMP      = gmp-4.3.2
GMP_TBZ2 = $(GMP).tar.bz2
GMP_URL  = ftp://ftp.gmplib.org/pub/$(GMP)/$(GMP_TBZ2)

#
# Interface to top-level prepare Makefile
#
PORTS += $(GMP)

GMP_INCLUDES  = include/gmp/gmp-impl.h
GMP_INCLUDES += include/gmp/x86_32/gmp-mparam.h
GMP_M4_SRC   += src/lib/gmp/mpn/asm-defs.m4

prepare-gmp: $(CONTRIB_DIR)/$(GMP) $(GMP_INCLUDES) $(GMP_M4_SRC)

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

include/gmp/x86_32/gmp-mparam.h: $(CONTRIB_DIR)/$(GMP)/mpn/x86/gmp-mparam.h
	$(VERBOSE)ln -sf ../../../$< $@

src/lib/gmp/mpn/asm-defs.m4: $(CONTRIB_DIR)/$(GMP)/mpn/asm-defs.m4
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf ../../../../$< $@

clean-gmp:
	$(VERBOSE)rm -f  $(GMP_INCLUDES)
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(GMP)
