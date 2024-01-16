MIRROR_FROM_REP_DIR := lib/import/import-curl.mk \
                       lib/symbols/curl \
                       src/lib/curl

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/curl)

content: include

include:
	mkdir -p $@
	cp -r $(PORT_DIR)/include/curl $@/

content: src/lib/curl

content: LICENSE FindCURL.cmake libcurl.pc

LICENSE:
	cp $(PORT_DIR)/src/lib/curl/COPYING $@

FindCURL.cmake:
	echo 'set(CURL_FOUND True)' > $@

VERSION := $(shell sed -n 's/VERSION.*:=[ ]*\(.*\)/\1/p' $(REP_DIR)/ports/curl.port)

libcurl.pc:
	echo "Name: libcurl" > $@
	echo "Description: Library to transfer files with ftp, http, etc." >> $@
	echo "Version: $(VERSION)" >> $@
	echo "Libs: -l:curl.lib.so" >> $@
	echo "supported_protocols=\"FILE FTP GOPHER HTTP HTTPS SCP SFTP\"" >> $@
