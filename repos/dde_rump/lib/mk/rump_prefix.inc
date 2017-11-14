# We prefix any global symbol in object files from SRC_NOLINK with 'rmpns_',
# for this we add a 'rmpns_<source file>.o' to 'SRC_O' and perform a symbol
# prefixing using an AWK script and object copy below which in turn creates the
# prefixed object file. The rpmns files are linked into the library.

SRC_NOLINK_OBJ = $(filter %.o,$(SRC_NOLINK:%.c=%.o) $(SRC_NOLINK:%.S=%.o))
OBJ_PREFIX     = $(addprefix rmpns_,$(SRC_NOLINK_OBJ))

SRC_O     += $(OBJ_PREFIX)
PREFIX_AWK = '$$NF!~/^(rump|RUMP|__|_GLOBAL_OFFSET_TABLE)/  {s=$$NF;sub(/^/, "&rumpns_", s); print $$NF, s}'


$(OBJ_PREFIX): $(SRC_NOLINK_OBJ) $(SRC_NOLINK)
	$(VERBOSE_MK)$(CUSTOM_NM) -go $(RUMP_LIB_BASE)/$(subst rmpns_,,$@) | awk $(PREFIX_AWK) \
	          > $(RUMP_LIB_BASE)/_$@
	$(VERBOSE_MK)$(CUSTOM_OBJCOPY) --preserve-dates --redefine-syms $(RUMP_LIB_BASE)/_$@ \
	          $(RUMP_LIB_BASE)/$(subst rmpns_,,$@) $(RUMP_LIB_BASE)/$@
	$(VERBOSE_MK)rm $(RUMP_LIB_BASE)/_$@

# vi:set ft=make :
