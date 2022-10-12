TARGET   = test-vmm_x86
REQUIRES = x86
SRC_CC   = component.cc
LIBS     = base

CC_CXX_WARN_STRICT_CONVERSION =

SRC_BIN = guest.bin

guest.o: guest.s
	$(MSG_ASSEM)$@
	$(VERBOSE)$(CC) -m16 -c $< -o $@

guest.bin: guest.o
	$(MSG_CONVERT)$@
	$(VERBOSE)$(OBJCOPY) -O binary $< $@
