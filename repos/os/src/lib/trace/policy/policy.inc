#
# \brief  Common build rules for creating trace-policy modules
# \author Josef Soentgen
# \date   2013-08-12
#

CXX_OPT  = -g -ffreestanding -fPIC -fno-exceptions -fno-rtti \
           -nostdinc -nostdlib -MMD

CXX_OPT += $(CC_CXX_OPT_STD)

LD_SCRIPT= $(PRG_DIR)/../policy.ld

-include *.d
-include ../*.d

table.o: table.cc
	$(MSG_COMP)$@
	$(VERBOSE)$(CXX) -c $(CC_MARCH) $(CXX_OPT) $(INCLUDES) $< -o $@

policy.o: policy.cc
	$(MSG_COMP)$@
	$(VERBOSE)$(CXX) -c $(CC_MARCH) $(CXX_OPT) $(INCLUDES) $< -o $@

$(TARGET_POLICY).elf: table.o policy.o
	$(MSG_LINK)$@
	$(VERBOSE)$(LD) $(LD_MARCH) -T $(LD_SCRIPT) -Ttext=0 \
		$^ $(shell $(CXX) $(CC_MARCH) -print-libgcc-file-name) -o $@

$(INSTALL_DIR)/$(TARGET_POLICY): $(TARGET_POLICY).elf
	$(VERBOSE)$(OBJCOPY) -O binary $< $@

$(TARGET): $(INSTALL_DIR)/$(TARGET_POLICY)

clean cleanall: clean_policy

clean_policy:
	$(VERBOSE)$(RM) ../*.o ../*.d *.o *.d $(TARGET_POLICY).elf \
		$(BUILD_BASE_DIR)/bin/$(TARGET_POLICY)

vpath table.cc $(REP_DIR)/src/lib/trace/policy
