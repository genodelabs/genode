include ports/libogg.inc

LIBOGG_BASE_URL = http://downloads.xiph.org/releases/ogg

LIBOGG_TBX = $(LIBOGG).tar.xz
LIBOGG_SHA = $(LIBOGG)-SHA256SUMS

LIBOGG_TBX_URL = $(LIBOGG_BASE_URL)/$(LIBOGG_TBX)
LIBOGG_SHA_URL = $(LIBOGG_BASE_URL)/SHA256SUMS


#
# Interface to top-level prepare Makefile
#
PORTS +=$(LIBOGG)

prepare-libogg: $(CONTRIB_DIR)/$(LIBOGG) include/ogg

$(CONTRIB_DIR)/$(LIBOGG):clean-libogg


#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(LIBOGG_TBX):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LIBOGG_TBX_URL) && touch $@

$(DOWNLOAD_DIR)/$(LIBOGG_SHA):
	$(VERBOSE)wget -c -O $(DOWNLOAD_DIR)/$(LIBOGG_SHA) $(LIBOGG_SHA_URL) && touch $@

$(DOWNLOAD_DIR)/$(LIBOGG_TBX).verified: $(DOWNLOAD_DIR)/$(LIBOGG_TBX) $(DOWNLOAD_DIR)/$(LIBOGG_SHA)
	$(VERBOSE)$(HASHVERIFIER) $(DOWNLOAD_DIR)/$(LIBOGG_TBX) $(DOWNLOAD_DIR)/$(LIBOGG_SHA) sha256
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(LIBOGG): $(DOWNLOAD_DIR)/$(LIBOGG_TBX)
	$(VERBOSE)tar xfJ $(DOWNLOAD_DIR)/$(LIBOGG_TBX) -C $(CONTRIB_DIR) && touch $@

$(CONTRIB_DIR)/$(LIBOGG)/include/ogg/config_types.h: $(CONTRIB_DIR)/$(LIBOGG)
	$(VERBOSE)sed \
			-e 's/@INCLUDE_INTTYPES_H@/1/' \
			-e 's/@INCLUDE_STDINT_H@/1/' \
			-e 's/@INCLUDE_SYS_TYPES_H@/1/' \
			-e 's/@SIZE16@/int16_t/' \
			-e 's/@USIZE16@/uint16_t/' \
			-e 's/@SIZE32@/int32_t/' \
			-e 's/@USIZE32@/uint32_t/' \
			-e 's/@SIZE64@/int64_t/' \
		$@.in > $@

#
# Install Worbis headers
#
include/ogg: $(CONTRIB_DIR)/$(LIBOGG)/include/ogg/config_types.h
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)ln -fs \
		../../$(CONTRIB_DIR)/$(LIBOGG)/include/ogg/ogg.h \
		../../$(CONTRIB_DIR)/$(LIBOGG)/include/ogg/config_types.h \
		../../$(CONTRIB_DIR)/$(LIBOGG)/include/ogg/os_types.h \
			$@

clean-libogg:
	$(VERBOSE)rm -rf \
		include/ogg/ogg.h \
		include/ogg/config_types.h \
		include/ogg/os_types.h
	$(VERBOSE)rmdir include/ogg 2>/dev/null || true
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LIBOGG)
