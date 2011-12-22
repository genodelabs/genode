SRC    = ../x86/genode.ld
TARGET = genode.ld

all:
	@echo "--- available targets ---"
	@echo "  genode.ld  - generate $(TARGET) from $(SRC)"
	@echo "  cleanall   - remove generated file"

#
# NOTE: We change the start address to 0x60000, which
#       is the same address as used by the original
#       roottask.
#
# On L4x0, the thread ID type is only 32bit instead of 64bit
# for L4v2. Therefore, we have to adapt the place holder for
# thread ID part of the parent capability.
#
genode.ld:
	cp $(SRC) $@
	sed -i "s/= 0x[0-9]\+;/= 0x00060000;/" $@
	sed -i "54s/^.*$$/\t\tLONG(0xffffffff);/" $@

clean cleanall:
	rm -f $(TARGET)

