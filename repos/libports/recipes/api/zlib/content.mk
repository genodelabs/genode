content: include lib/symbols/zlib LICENSE FindZLIB.cmake zlib.pc

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/zlib)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/zlib/* $@/

lib/symbols/zlib:
	$(mirror_from_rep_dir)

LICENSE:
	echo "zlib license" > $@

FindZLIB.cmake:
	echo 'set(ZLIB_FOUND True)' > $@

VERSION := $(shell sed -n 's/VERSION.*:=[ ]*\(.*\)/\1/p' $(REP_DIR)/ports/zlib.port)

zlib.pc:
	echo "Name: zlib" > $@
	echo "Description: zlib compression library" >> $@
	echo "Version: $(VERSION)" >> $@
	echo "Libs: -l:zlib.lib.so" >> $@
