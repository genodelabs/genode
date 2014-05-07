OKL4_DIR = ../okl4_2.1.1-patch.9
CXXFLAGS  = -I$(OKL4_DIR)/build/pistachio/include -nostdlib -nostdinc
#CXXFLAGS += -DCONFIG_MAX_THREAD_BITS=10
#CXXFLAGS += -DCONFIG_CPU_IA32_I586
#CXXFLAGS += -DCONFIG_KDB_CONS
LD_SCRIPT = genode.ld
LDFLAGS   = -Wl,-Ttext=0x01000000 -Wl,-T$(LD_SCRIPT)

all: image

hello: hello.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -static crt0.s hello.cc -o $@

#
# Merge kernel and hello program into a single elf image
#
# We need to strip the elf image to make the image loadable by grub.
# Otherwise GRUB complaints:
#
# "Error 28: Selected item cannot fit into memory"
#
image: hello weaver.xml
	$(OKL4_DIR)/tools/pyelf/elfweaver merge -o $@ weaver.xml
	strip $@

clean:
	rm -f hello image
